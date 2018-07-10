/*
    hello07.c - TCP coroutines

    This takes previous concepts and builds a useful program wrapping TCP Sockets.
    The purpose is not to creat an altnerative to the LuaSocket library, but to
    focus on the scenario of having one coroutine per TCP connection
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lua/lua.h"
#include "lua/lauxlib.h"
#include "lua/lualib.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <errno.h>

struct SocketWrapper connections;
int connection_count;

/* As explained in previous examples, this will uniquely identify the type
 * of our object */
static const char * MY_SOCKET_CLASS = "My Socket Class";


enum {
    SocketStatus_Closed,
    SocketStatus_Reading,
    SocketStatus_Writing,
    SocketStatus_Waiting,
};

/* As explained in previous examples, this will wrap our socket */
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
    char *bytes;
    
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
};

static void wrapper_close_socket(struct SocketWrapper *wrapper)
{
    if (wrapper == NULL) {
        fprintf(stderr, "err: wrapper is NULL\n");
    }
    if (wrapper->fd > 0) {
        close(wrapper->fd);
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

static void wrapper_close_all(struct SocketWrapper *wrapper)
{
    wrapper_close_socket(wrapper);
    wrapper_close_thread(wrapper);
    
    wrapper->next->prev = wrapper->prev;
    wrapper->prev->next = wrapper->next;
    
    wrapper->next = 0;
    wrapper->prev = 0;
    free(wrapper);
}

static int socket_close(struct lua_State *L)
{
    struct SocketWrapper *wrapper;
    wrapper = luaL_checkudata(L, 1, MY_SOCKET_CLASS);
    wrapper_close_socket(wrapper);
    return 0;
}

static int socket_receive(struct lua_State *L)
{
    struct SocketWrapper *wrapper;
    
    wrapper = luaL_checkudata(L, 1, MY_SOCKET_CLASS);
    if (lua_gettop(L) > 1) {
        wrapper->byte_count = luaL_checkinteger(L, 2);
    } else {
        wrapper->byte_count = 0; /* zero means "as many as you can" */
    }
    
    wrapper->bytes = 0;
    wrapper->bytes_done = 0;
    wrapper->status = SocketStatus_Reading;
 
    return 0;
}

static int socket_send(struct lua_State *L)
{
    struct SocketWrapper *wrapper;
    
    wrapper = luaL_checkudata(L, 1, MY_SOCKET_CLASS);
    
    wrapper->bytes = (char*)luaL_checklstring(L, 1, &wrapper->byte_count);
    
    wrapper->bytes_done = 0;
    wrapper->status = SocketStatus_Writing;
    
    return 0;

}

static int socket_settimeout(struct lua_State *L)
{
    return 0;
}



void network_server(struct lua_State *L, int port_number)
{
    int fdsrv;
    struct sockaddr_in6 sin = {0};
    int x;
    
    /* Socket: creat a server that listens on either IPv4 or IPv6 */
    fdsrv = socket(AF_INET6, SOCK_STREAM, 0);
    
    /* Socket: initialize server-side address */
    sin.sin6_family = AF_INET6;
    sin.sin6_port   = htons((short)port_number);
    sin.sin6_addr   = in6addr_any;
    
    /* Socket: associate the socket to a port number, which can fail if there
     * is already a server listening on that address. */
    x = bind(fdsrv, (struct sockaddr *)&sin, sizeof(sin));
    if (x < 0) {
        fprintf(stderr, "bind(%d) failed %d\n", port_number, errno);
        exit(1);
    }
    listen(fdsrv, 10);
    
    fprintf(stderr, "Starting event loop...%d\n", fdsrv);
    
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
        
        fprintf(stderr, "Selecting ...%d\n", nfds);
        
        /* Socket: find which sockets have incoming data */
        x = select(nfds, &readset, &writeset, &errorset, 0);
        if (x < 0) {
            fprintf(stderr, "select: error %d\n", errno);
            break;
        }
        fprintf(stderr, "Selected = %d\n", x);
        
        /* Socket: handle new connections, if any */
        if (FD_ISSET(fdsrv, &readset) || FD_ISSET(fdsrv, &writeset)) {
            struct sockaddr_in6 client;
            socklen_t sizeof_client = sizeof(client);
            int fd;
            
            /* Socket: Accept the incoming connection */
            fd = accept(fdsrv, (struct sockaddr*)&client, &sizeof_client);
            if (fd < 0) {
                /* Socket: some error occured */
                fprintf(stderr, "accept(): error %d\n", errno);
            } else if (connection_count >= 30) {
                /* Socket: if we hit the incoming select() connection limit, discard the connection */
                close(fd);
            } else {
                int x;
                
                /* Lua: create a  wrapper object and push it onto the stack */
                wrapper = lua_newuserdata(L, sizeof(*wrapper));
                memset(wrapper, 0, sizeof(*wrapper));
                
                /* Lua: set the class/type */
                luaL_setmetatable(L, MY_SOCKET_CLASS);
                
                /* Socket: fill in the relavent socket data */
                wrapper->fd = fd;
                wrapper->sizeof_client = sizeof_client;
                memcpy(&wrapper->client, &client, sizeof(client));
                
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
                
                /* Lua: Now run the thread for the first time*/
                lua_xmove(L, wrapper->L, 1); /* move object from main thread to coroutine */
                x = lua_resume(wrapper->L, NULL, 1);
                if (x == LUA_YIELD) {
                    if (lua_gettop(wrapper->L) != 2) {
                    }
                }
            }
        }
    }
}

int main(int argc, char *argv[])
{
    lua_State *L;
    int i;
    int x;
    const char *filename = "hello07.lua";
    int port_number;
    
    
    connections.next = &connections;
    connections.prev = &connections;
    
    fprintf(stderr, "Running: hello07\n");
    
    /*
     * Create an instances of the Lua interpreter and register
     * the standard libraries.
     */
    fprintf(stderr, "Creating interpreter instance\n");
    L = luaL_newstate();
    luaL_openlibs(L);
    
    /*
     * Lua: Create a table that will hold our threads.
     */
    lua_newtable(L);
    lua_setglobal(L, "threads");
  
    /*
     * Lua: Create a class to wrap a 'socket'
     */
    {
        static const luaL_Reg my_socket_methods[] = {
            {"close",       socket_close},
            {"receive",     socket_receive},
            {"send",        socket_send},
            {"settimeout",  socket_settimeout},
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
     * Load our networking script and compile it.
     */
    x = luaL_loadfile(L, filename);
    if (x != LUA_OK) {
        fprintf(stderr, "error loading: %s: %s\n", filename, lua_tostring(L, -1));
        lua_close(L);
        return 0;
    }
    
    /*
     * Run the script. This will set configuration parameters as well
     * as register the function that handles connections.
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
    
    

   
    network_server(L, port_number);

    
    
 
    /*
     * Also run some external scripts. Note that since we haven't registered
     * the base libraries above, then there's nothing you can do in such
     * scripts example call our example function.
     */
    for (i=1; i<argc; i++) {
        const char *filename = argv[i];
        
        x = luaL_dofile(L, filename);
        if (x != LUA_OK) {
            fprintf(stderr, "error: %s: %s\n", filename, lua_tostring(L, -1));
            lua_close(L);
            return 0;
        }
    }
    
    /*
     * Now that we are done running everything, close and exit.
     */
    fprintf(stderr, "Exiting...\n");
    lua_close(L);

    return 0;
}
