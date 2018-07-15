/*
    hello07.c - TCP coroutines

    This takes previous concepts and builds a useful program wrapping TCP Sockets.
    The purpose is not to creat an altnerative to the LuaSocket library, but to
    focus on the scenario of having one coroutine per TCP connection
 */
#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lua/lua.h"
#include "lua/lauxlib.h"
#include "lua/lualib.h"

/*
 * This code compiles on Windows, macOS, and Linux, so we have to 
 * mess around a bit to include the Windows socket stuff.
 */
#if defined(WIN32)
#include <WinSock2.h>
#include <WS2tcpip.h>
#define WSA(err) (WSA##err)
#define errnosocket WSAGetLastError()
#else
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#define errnosocket (errno)
#define closesocket(fd) close(fd)
#define ioctlsocket(a,b,d) ioctl(a,b,c)
#define WSA(err) (err)
#endif

#if defined(_MSC_VER)
#pragma comment(lib, "ws2_32")
#endif


/* Lua: As explained in previous examples, this will uniquely identify the type
 * of our object */
static const char * MY_SOCKET_CLASS = "My Socket Class";


enum {
    SocketStatus_Closed,
    SocketStatus_Reading,
    SocketStatus_Writing,
    SocketStatus_Waiting,
};

/* As demonstrated in previous examples, this will wrap our socket */
struct SocketWrapper
{
    int fd;
    struct sockaddr_in6 client;
    int sizeof_client;
    lua_State *L;
    
    /* The number of bytes to either read or write, depending upon
     * whether we are sending or receiving */
    size_t byte_count;
    
    /* How much we are done either receiving or sending the desired number
     * of bytes */
    size_t bytes_done;
    
    /* A pointer to the buffer we are receiving into, or the bytes we
     * are sending */
    char *buf;
    unsigned is_buf_malloced:1;
    unsigned is_receive_line:1;
    
    int status;
    
    /* Lua: We have to keep a reference to the coroutine/thread in the master
     * state, otherwise it'll be garbage collected. When the thread is created,
     * that reference is on the stack, but you can't keep millions of threads on
     * the stack. Instead, we need to stick a reference in a table somewhere.
     * Lua has the luaL_ref() and luaL_unref() functions precisely for this
     * purpose */
    int ref;
    
    struct SocketWrapper *next;
    struct SocketWrapper *prev;
    
    char peername[50];
    char peerport[6];
};

/*
 * Globals
 */
struct SocketWrapper connections;
int connection_count;


static void wrapper_close_socket(struct SocketWrapper *wrapper)
{
    if (wrapper == NULL) {
        fprintf(stderr, "err: wrapper is NULL\n");
    }
    if (wrapper->fd > 0) {
        closesocket(wrapper->fd);
        wrapper->fd = -1;
    }
}

static void wrapper_close_thread(struct SocketWrapper *wrapper)
{
    /* Lua: Removes the reference to the thread, so that it will get garbage collected */
    luaL_unref(wrapper->L, LUA_REGISTRYINDEX, wrapper->ref);
    wrapper->L = NULL;
    wrapper->ref = 0;
}

static void wrapper_close_buffer(struct SocketWrapper *wrapper)
{
    if (wrapper->is_buf_malloced) {
        free(wrapper->buf);
    }
    wrapper->buf = 0;
    wrapper->is_buf_malloced = 0;
    wrapper->byte_count = 0;
    wrapper->bytes_done = 0;
    
    fprintf(stderr, "[%s]:%s:C: buffer cleared of %d bytes\n", wrapper->peername, wrapper->peerport, (int)wrapper->byte_count);
    
}

static struct SocketWrapper *wrapper_close_all(struct SocketWrapper *wrapper)
{
    struct SocketWrapper *prev = wrapper->prev;
    
    wrapper_close_socket(wrapper);
    wrapper_close_thread(wrapper);
    wrapper_close_buffer(wrapper);
    
    wrapper->next->prev = wrapper->prev;
    wrapper->prev->next = wrapper->next;
    
    
    wrapper->next = 0;
    wrapper->prev = 0;
    
    /* Kludge: Sometimes we delete this item when enumerating through a linked
     * list. We have to have a current object in order to enumerate to the next,
     * but this current object disappears. Therefore, return the previous one,
     * which will then iterate to the next correctly. */
    return prev;
}

/* Lua: closes the socket. This is basically only called when the object is
 * 'finalized' during garbage collection, though in theory it can be called
 * earlier from a script. */
static int socket_close(struct lua_State *L)
{
    struct SocketWrapper *wrapper;
    wrapper = luaL_checkudata(L, 1, MY_SOCKET_CLASS);
    wrapper_close_socket(wrapper);
    return 0;
}

/* Lua: get the IP address of the remote party, formatted as a string.
 * This can be either an IPv4 or IPv6 address */
static int socket_peername(struct lua_State *L)
{
    struct SocketWrapper *wrapper;
    wrapper = luaL_checkudata(L, 1, MY_SOCKET_CLASS);
    lua_pushstring(L, wrapper->peername);
    return 1;
}

/* Lua: get the port numbr of the remote party, formatted as a string
 * (though I suppose Lua can automatically convert such strings to
 * numbers). When you have many concurrent TCP connections
 * each one will have a different port number */
static int socket_peerport(struct lua_State *L)
{
    struct SocketWrapper *wrapper;
    wrapper = luaL_checkudata(L, 1, MY_SOCKET_CLASS);
    lua_pushstring(L, wrapper->peerport);
    return 1;
}

/* Lua: wraps the 'receive' function call. This doesn't actually receive
 * anything at this time, but yields/exits from the script back to the
 * dispatch loop in C. When the dispatch loop detects incoming information,
 * it will push that result on the stack and return back to the caller
 * of this function */
static int socket_receive(struct lua_State *L)
{
    struct SocketWrapper *wrapper;
    
    wrapper = luaL_checkudata(L, 1, MY_SOCKET_CLASS);
    if (lua_gettop(L) > 1) {
        wrapper->byte_count = (size_t)luaL_checkinteger(L, 2);
    } else {
        wrapper->byte_count = 0; /* zero means "as many as you can" */
    }
    
    wrapper_close_buffer(wrapper);
    wrapper->status = SocketStatus_Reading;
 
    return lua_yield(L, 0);
}

/* Same as "socket_receive()", but gets a line of input terminated
 * by a newline '\n' character */
static int socket_receiveline(struct lua_State *L)
{
    struct SocketWrapper *wrapper;
    
    wrapper = luaL_checkudata(L, 1, MY_SOCKET_CLASS);
    if (lua_gettop(L) > 1) {
        wrapper->byte_count = (size_t)luaL_checkinteger(L, 2);
    } else {
        wrapper->byte_count = 0; /* zero means "as many as you can" */
    }
    
    wrapper->is_receive_line = 1;
    
    wrapper_close_buffer(wrapper);
    wrapper->status = SocketStatus_Reading;
    
    return lua_yield(L, 0);
}

static int socket_send(struct lua_State *L)
{
    struct SocketWrapper *wrapper;
    
    wrapper = luaL_checkudata(L, 1, MY_SOCKET_CLASS);
    
    wrapper_close_buffer(wrapper);
    
    wrapper->buf = (char*)luaL_checklstring(L, 2, &wrapper->byte_count);
    wrapper->is_buf_malloced = 0;
    wrapper->status = SocketStatus_Writing;
    
    fprintf(stderr, "[%s]:%s:C: sending %d bytes from socket\n", wrapper->peername, wrapper->peerport, (int)wrapper->byte_count);
    return lua_yield(L, 0);

}


void network_server(struct lua_State *L, int port_number)
{
    int fdsrv;
    struct sockaddr_in6 sin = {0};
    int x;

#ifdef WIN32
    {WSADATA x; WSAStartup(0x101, &x);}
#endif

    /* Socket: creat a server that listens on either IPv4 or IPv6 */
    fdsrv = socket(AF_INET6, SOCK_STREAM, 0);
    
    /* Make sure we can handle both IPv4 and IPv6 incoming connections */
    {
        int off = 0;
        if (setsockopt(fdsrv, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&off, sizeof(off)) < 0) {
            fprintf(stderr, "setsockopt(!IPV6_V6ONLY): %d\n", (int)errnosocket);
        }
    }
    
    /* Quickly reuse the port number, otherwise when we stop this program and quickly
     * restart, we'd have to instead wait a minute */
    {
        int on = 1;
        if (setsockopt(fdsrv, SOL_SOCKET, SO_REUSEADDR, (char *)&on,sizeof(on)) < 0) {
            fprintf(stderr, "setsockopt(SO_REUSEADDR): %d\n", (int)errnosocket);
        }
    }
    
    /* Socket: initialize server-side address */
    sin.sin6_family = AF_INET6;
    sin.sin6_port   = htons((short)port_number);
    sin.sin6_addr   = in6addr_any;
    
    /* Socket: associate the socket to a port number, which can fail if there
     * is already a server listening on that address. */
    x = bind(fdsrv, (struct sockaddr *)&sin, sizeof(sin));
    if (x < 0) {
        fprintf(stderr, "bind(%d) failed %d\n", port_number, errnosocket);
        exit(1);
    }
    listen(fdsrv, 10);
    
    fprintf(stderr, "Starting event loop...\n");
    
    /*
     * Socket: Dispatch loop processing incoming data
     */
    for (;;) {
        struct SocketWrapper *wrapper;
        fd_set readset, writeset, errorset;
        int nfds = 0;
        
        /* Socket: zero out the select sets */
        FD_ZERO(&readset);
        FD_ZERO(&writeset);
        FD_ZERO(&errorset);

        /* Socket: add the server */
        FD_SET(fdsrv, &readset);
        FD_SET(fdsrv, &writeset);
        FD_SET(fdsrv, &errorset);
        if (nfds < fdsrv)
            nfds = fdsrv;
        
        /* Socket: add the socket descriptors for all the TCP connections */
        for (wrapper=connections.next; wrapper != &connections; wrapper = wrapper->next) {
            int fd = wrapper->fd;
            
            if (wrapper->status == SocketStatus_Reading)
                FD_SET(fd, &readset);
            if (wrapper->status == SocketStatus_Writing)
                FD_SET(fd, &writeset);
            FD_SET(fd, &errorset);
            if (nfds < fd)
                nfds = fd;
        }
        
        fprintf(stderr, "Dispatch: Selecting...nfds=%d\n", nfds);
        
        /* Socket: find which sockets have incoming data */
        x = select(nfds+1, &readset, &writeset, &errorset, 0);
        if (x < 0) {
            fprintf(stderr, "select: error %d\n", errnosocket);
            break;
        }
        fprintf(stderr, "Dispach: Selected\n");
        
        /* Socket: handle new connections, if any */
        if (FD_ISSET(fdsrv, &readset) || FD_ISSET(fdsrv, &writeset)) {
            struct sockaddr_in6 client;
            socklen_t sizeof_client = sizeof(client);
            int fd;
            
            /* Socket: Accept the incoming connection */
            fd = accept(fdsrv, (struct sockaddr*)&client, &sizeof_client);
            if (fd < 0) {
                /* Socket: some error occured */
                fprintf(stderr, "accept(): error %d\n", errnosocket);
            } else if (connection_count >= 30) {
                /* Socket: if we hit the incoming select() connection limit, discard the connection */
                closesocket(fd);
            } else {
                int x;
                int on = 1;
                
                /* Socket: mark this as non-blocking, which we don't really need for receiving data,
                 * but we do need for sending, incase the send() function blocks on large sends */
                if (ioctlsocket(fd, FIONBIO, (void *)&on)) {
                    fprintf(stderr, "ioctl(FIONBIO) failed %d\n", errnosocket);
                }
                
                /* Lua: create a  wrapper object and push it onto the stack */
                wrapper = lua_newuserdata(L, sizeof(*wrapper));
                memset(wrapper, 0, sizeof(*wrapper));
                
                /* Lua: set the class/type */
                luaL_setmetatable(L, MY_SOCKET_CLASS);
                
                /* Socket: fill in the relavent socket data */
                wrapper->fd = fd;
                wrapper->sizeof_client = sizeof_client;
                memcpy(&wrapper->client, &client, sizeof(client));
                getnameinfo((struct sockaddr*)&client,
                            sizeof_client,
                            wrapper->peername,
                            sizeof(wrapper->peername),
                            wrapper->peerport,
                            sizeof(wrapper->peerport),
                            NI_NUMERICHOST| NI_NUMERICSERV);
                if (IN6_IS_ADDR_V4MAPPED(&client.sin6_addr))
                    memmove(wrapper->peername, wrapper->peername + 7, strlen(wrapper->peername + 7) + 1);
                fprintf(stderr, "[%s]:%s:C: accepted connection\n", wrapper->peername, wrapper->peerport);
                
                /* Lua: create a new coroutine/thread to handle the TCP connection
                 * We have to store a reference to it somewhere so that the
                 * garbage collector doesn't delete it. */
                wrapper->L = lua_newthread(L);
                wrapper->ref = luaL_ref(L, LUA_REGISTRYINDEX);
                
                /* Sockets: Add the TCP connection information to our list of connections */
                wrapper->next = connections.next;
                connections.next = wrapper;
                wrapper->next->prev = wrapper;
                wrapper->prev = &connections;
                
                /* Lua: Get the function */
                lua_getglobal(wrapper->L, "onConnect");
                
                /* Lua: Now run the thread for the first time*/
                lua_xmove(L, wrapper->L, 1); /* move userdataobject from main thread to coroutine */
                printf("Starting script...%d-items, [-1]=%s, [-2]=%s\n",
                       lua_gettop(wrapper->L), luaL_typename(wrapper->L, -1), luaL_typename(wrapper->L, -2));
                x = lua_resume(wrapper->L, NULL, 1);
                if (x == LUA_YIELD) {
                    printf("Script yielded, %d items\n", lua_gettop(wrapper->L));
                } else if (x == LUA_OK) {
                    printf("Script premature exit\n");
                    wrapper_close_all(wrapper);
                } else {
                    fprintf(stderr, "Script error: %s\n", lua_tostring(wrapper->L, -1));
                    wrapper_close_all(wrapper);
                }
            }
        }
        
        /* Socket: Handle reads/writes/exceptions */
        for (wrapper=connections.next; wrapper != &connections; wrapper = wrapper->next) {
            int fd = wrapper->fd;
            int return_items = 0;
            
            printf("Wrapper bytes = %d\n", (int)wrapper->byte_count);
            
            /*
             * Sockets: read from the network
             */
            if (!wrapper->is_receive_line && FD_ISSET(fd, &readset)) {
                char buf[4096];
                size_t bytes_to_read;
                size_t bytes_read;
                
                /* Figure out which buffer we need to read into */
                if (wrapper->buf == 0) {
                    if (wrapper->byte_count > sizeof(buf)) {
                        wrapper->buf = malloc(wrapper->byte_count);
                        wrapper->is_buf_malloced = 1;
                    } else {
                        wrapper->buf = buf;
                        wrapper->is_buf_malloced = 0;
                    }
                }
                
                /* Calculate how many bytes to read */
                if (wrapper->byte_count)
                    bytes_to_read = wrapper->byte_count - wrapper->bytes_done;
                else
                    bytes_to_read = sizeof(buf);
                
                /* Do the receive */
                bytes_read = recv(fd, wrapper->buf + wrapper->bytes_done, bytes_to_read, 0);
                
                /* See if an error occured */
                if (bytes_read <= 0) {
                    fprintf(stderr, "[%s]:%s:C: error reading from socket %d\n", wrapper->peername, wrapper->peerport, errnosocket);
                    wrapper = wrapper_close_all(wrapper);
                    continue;
                }
                fprintf(stderr, "[%s]:%s:C: read %d bytes from socket\n", wrapper->peername, wrapper->peerport, (int)bytes_read);
                
                /* See if we've read all the content */
                if (wrapper->byte_count) {
                    wrapper->bytes_done += bytes_read;
                    if (wrapper->bytes_done < wrapper->byte_count)
                        continue;
                } else
                    wrapper->bytes_done = bytes_read;
                
                /* Return the string to the script */
                lua_pushlstring(wrapper->L, wrapper->buf, wrapper->bytes_done);
                return_items = 1;
               
            } else if (wrapper->is_receive_line && FD_ISSET(fd, &readset)) {
                char buf[4096];
                size_t bytes_read;
                size_t newline;
                
                /* Peek at the bytes  */
                bytes_read = recv(fd, buf, sizeof(buf), MSG_PEEK);
                if (bytes_read <= 0) {
                    fprintf(stderr, "[%s]:%s:C: error reading from socket %d\n", wrapper->peername, wrapper->peerport, errnosocket);
                    wrapper = wrapper_close_all(wrapper);
                    continue;
                }
                
                /* Find a newline if it exists */
                for (newline=0; newline<bytes_read; newline++) {
                    if (buf[newline] == '\n') {
                        newline++; /* include the trailing '\n' newline */
                        break;
                    }
                }

                if (wrapper->is_buf_malloced) {
                    /* If we already have a buffer, expand it so it can hold the additional data */
                    wrapper->buf = realloc(wrapper->buf, wrapper->bytes_done + newline);
                } else if (buf[newline-1] != '\n') {
                    /* If we haven't reached the end-of-line yet, then allocate a buffer to hold them */
                    wrapper->buf = malloc(newline);
                    wrapper->is_buf_malloced = 1;
                    wrapper->bytes_done = 0;
                } else {
                    /* If we have a complete line, then just use the stack */
                    wrapper->buf = buf;
                    wrapper->bytes_done = 0;
                    wrapper->is_buf_malloced = 0;
                }
                
                /* Now do a real, non-peek read */
                bytes_read = recv(fd, wrapper->buf + wrapper->bytes_done, newline, 0);
                if (bytes_read <= 0) {
                    fprintf(stderr, "[%s]:%s:C: error reading from socket %d\n", wrapper->peername, wrapper->peerport, errnosocket);
                    wrapper = wrapper_close_all(wrapper);
                    continue;
                } else
                    wrapper->bytes_done += bytes_read;
                fprintf(stderr, "[%s]:%s:C: read %d bytes from socket\n", wrapper->peername, wrapper->peerport, (int)bytes_read);
                
                /* If we haven't reached the end of line, then stop processing */
                if (wrapper->buf[wrapper->bytes_done-1] != '\n')
                    continue;
                
                /* Clean the string */
                while (wrapper->bytes_done && isspace(wrapper->buf[wrapper->bytes_done-1]))
                    wrapper->bytes_done--;
                
                /* Now return the string */
                lua_pushlstring(wrapper->L, wrapper->buf, wrapper->bytes_done);
                return_items = 1;

            } else if (FD_ISSET(fd, &writeset)) {
                size_t bytes_to_write;
                size_t bytes_written;
                
                bytes_to_write = wrapper->byte_count - wrapper->bytes_done;
                
                bytes_written = send(fd, wrapper->buf + wrapper->bytes_done, bytes_to_write, 0);
                
                /* See if an error occured */
                if (bytes_written <= 0) {
                    fprintf(stderr, "[%s]:%s:C: send error %d (wanted %d bytes)\n", wrapper->peername, wrapper->peerport, (int)errnosocket, (int)bytes_to_write);
                    wrapper = wrapper_close_all(wrapper);
                    continue;
                } else
                    fprintf(stderr, "[%s]:%s:C: sent %d bytes\n", wrapper->peername, wrapper->peerport, (int)bytes_written);
                
                
                /* See if we've written all the content */
                wrapper->bytes_done += bytes_written;
                if (wrapper->bytes_done < wrapper->byte_count)
                    continue;
                
                /* Mark the buffer as having been written */
                if (wrapper->is_buf_malloced)
                    free(wrapper->buf);
                wrapper->buf = 0;
                wrapper->is_buf_malloced = 0;
                
                return_items = 0;

            } else if (FD_ISSET(fd, &errorset)) {
                fprintf(stderr, "Socket error: %d\n", errnosocket);
                wrapper = wrapper_close_all(wrapper);
                continue;
            } else {
                /* no events triggered for this connection, so go
                 * to next connection. This code allows us to skip
                 * the bit below that resumes the thread */
                continue;
            }
            
            /*
             * If we reach this point, one of the send/receive events triggered
             * above, so therefore we need to resume the coroutine/thread. If there
             * was a receive() function, then we've pushed a string onto the stack
             * to resume.
             */
            x = lua_resume(wrapper->L, NULL, return_items);
            if (x == LUA_YIELD) {
                printf("Script yielded, %d items\n", lua_gettop(wrapper->L));
            } else if (x == LUA_OK) {
                printf("Script exit\n");
                wrapper = wrapper_close_all(wrapper);
                continue;
            } else {
                fprintf(stderr, "Script error: %s\n", lua_tostring(wrapper->L, -1));
                wrapper = wrapper_close_all(wrapper);
                continue;
            }
        }
        
    }
}

int main(int argc, char *argv[])
{
    lua_State *L;
    int x;
    const char *filename;
    int port_number;
    
    /*
     * Initialize our doubly-linked list of TCP connections
     */
    connections.next = &connections;
    connections.prev = &connections;
    
    /*
     * Grab the script to run
     */
    if (argc != 2) {
        fprintf(stderr, "No script specified\n");
        fprintf(stderr, "Usage: hello07 <scriptname>\n");
        fprintf(stderr, "Try 'hello07.lua'\n");
        return 1;
    } else {
        filename = argv[1];
    }
    
    fprintf(stderr, "Running: hello07\n");
    
    fprintf(stderr, "Creating interpreter instance/VM\n");
    L = luaL_newstate();
    luaL_openlibs(L);
    
    /*
     * Lua: Create a class to wrap a 'socket'
     */
    {
        static const luaL_Reg my_socket_methods[] = {
            {"close",       socket_close},
            {"receive",     socket_receive},
            {"receiveline", socket_receiveline},
            {"send",        socket_send},
            {"peername",    socket_peername},
            {"peerport",    socket_peerport},
            {"__gc",        socket_close},
            {NULL, NULL}
        };
        
        luaL_newmetatable(L, MY_SOCKET_CLASS);
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");
        luaL_setfuncs(L, my_socket_methods, 0);
        lua_pop(L, 1);
    }
    
    
    
    /*
     * Lua: Load our networking script and compile it. Any syntax errors will
     * be detected at this point.
     */
    x = luaL_loadfile(L, filename);
    if (x != LUA_OK) {
        fprintf(stderr, "error loading: %s: %s\n", filename, lua_tostring(L, -1));
        lua_close(L);
        return 0;
    }
    
    /*
     * Lua: Start running the script. At this stage, the "onConnection()" function doesn't
     * run. Instead, it's registered as a global function to be called later.
     */
    fprintf(stderr, "Running script file: %s\n", filename);
    x = lua_pcall(L, 0, 0, 0);
    if (x != LUA_OK) {
        fprintf(stderr, "error running: %s: %s\n", filename, lua_tostring(L, -1));
        lua_close(L);
        return 0;
    }
    
    /*
     * Get the port number the script has configured.
     */
    lua_getglobal(L, "port");
    port_number = (int)lua_tointeger(L, -1);
    if (port_number == 0)
        port_number = 7;
    lua_pop(L, 1);
    
    

   
    /*
     *
     *
     * Run the server dispatch loop
     *
     *
     */
    network_server(L, port_number);

    
    
    /*
     * Now that we are done running everything, close and exit.
     */
    fprintf(stderr, "Exiting...\n");
    lua_close(L);

    return 0;
}
