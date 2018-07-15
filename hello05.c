/* 0123456789ABCDEFGHIJKLMNOPQRSTUVWXZabcdefghijklmnopqrstuvqxyz
 
    hello05.c - threads/coroutines

 This example show:
    * a C function that "yields" control back to C from a coroutine
    * ..and then resumes

 Like previous examples, we'll have a "read()" function for reading
 a certain number of bytes from a file. However, instead of defining
 a C function that does the read, we instead use a coroutine to
 "yield" control back out, which then does the read, then resumes
 executing the script.
 
 This is a poor way of reading files, but what we really want this for
 is for handling many concurrent network connections, with a central
 dispatcher. The threads will wait for network events (connect,
 receive, send, close), which will yield back to the dispatcher. When
 the waited event arrives, the dispatcher will resume the thread. To
 simplify matters, we use files instead of networking.
*/
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lua/lua.h"
#include "lua/lauxlib.h"
#include "lua/lualib.h"


/*
 * An example function that will do a yield. In this example, the function
 * doesn't actually do anything useful -- it just yields to the continuation
 * function that will do the real work. The work being done will be outside
 * this function.
 *
 * We need to pass the input value (the count of the number of bytes to read)
 * back to the resume function. However, the true "return value" as seen
 * from the Lua caller will be bytes read from a file, which will be
 * done elsewhere outside this function. Which is relaly weird if you
 * think about it.
 */
static int cfunc_myread(lua_State *L)
{
    int count;
    
    /* Grab the parameter from the Lua script to this function, as usual,
     * check it first */
    count = luaL_checkinteger(L,1);
    
    /* Push this integer as the return value to C resume() function */
    lua_pushinteger(L, count);
    
    /* Yield one value, the count we pushed above */
    return lua_yield(L, 1);
}


int main(int argc, char *argv[])
{
    lua_State *L;
    int x;
    
    fprintf(stderr, "Running: hello05\n");
    fprintf(stderr, "Creating interpreter instance/VM\n");
    L = luaL_newstate();
    luaL_openlibs(L);

    /* Register our yielding function */
    lua_register(L, "myread", cfunc_myread);

    /*
     * Execute a script. This does NOT execute the function "connection()",
     * but registers it as a global function.
     */
    x = luaL_dostring(L,
                      "xyz = 5;\n"
                      "\n"
                      "function connection(n)\n"
                      "  local x;\n"
                      "  print(n..' Hello, world!');\n"
                      "  x = myread(5);\n"
                      "  print(n..' '..x);\n"
                      "  x = myread(6);\n"
                      "  print(n..' '..x);\n"
                      "  x = myread(7);\n"
                      "  print(n..' '..x);\n"
                      "  x = myread(8);\n"
                      "  print(n..' '..x);\n"
                      "  print(n..' Goodbyte!')\n"
                      "end\n"
                      );
    if (x != LUA_OK) {
        fprintf(stderr, "error: %s: %s\n", "script", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }
    

    /*
     * Now execute the "connection()" function in that script as a coroutine
     * that periodically yields control back to us.
     */
    {
        FILE *fp;
        size_t count;
        
        /* Just so that we have some real work to do, we are going to read
         * the contents of a file */
        fp = fopen("hello05.c", "rt");
        if (fp == NULL) {
            perror("hello05.c");
            return -1;
        }
        
        /* Retrieve the function "connection()" from the script and place it onto
         * the stack. */
        lua_getglobal(L, "connection");
        
        /* Push a parameter onto the stack, in this case, it'll be the "connection id"
         * that identies which connection we have, in case we have multiple */
        lua_pushinteger(L, 1);
        
        /* Begin the execution of the "connection()" function, which will end
         * at the first 'yield'.
         * CALL: stack ahas parameters and the function tiself
         * RETURN: stack has the return values from the yield call
         */
        printf("\nStarting, stack has %d items, a %s and a %s\n", lua_gettop(L), lua_typename(L,lua_type(L,-1)), lua_typename(L,lua_type(L,-2)));
        x = lua_resume(L, NULL, 1);
        printf("First yield hit, stack has %d item, with top item having value of %d\n",
               lua_gettop(L), (int)lua_tointeger(L, -1));
        printf("\n");
        
        /* The cfunc_myread will return a 'count' of the number of bytes to read
         * from the stream. So we pop this off the stack. */
        if (lua_gettop(L) != 1) {
            fprintf(stderr, "Unexpected return arguments, found %d, expected 1\n", lua_gettop(L));
            goto end;
        }
        if (lua_type(L, -1) != LUA_TNUMBER) {
            fprintf(stderr, "Unexpected return argument, found %s, expected Number\n", lua_typename(L, lua_type(L,-1)));
            goto end;
        }
        count = (size_t)lua_tointeger(L, -1);
        lua_pop(L, lua_gettop(L)); /*expected 1 item, but pop all items just in case */
        
        
        for (;;) {
            char buf[1024];
            size_t bytes_read;
            if (count > sizeof(buf))
                count = sizeof(buf);
            
            /* We we are going to do some simulated work here, reading the 'count'
             * number of bytes from a file. What we'd really do is have a dispatch
             * function here, checking which input had data avilable, then reading
             * that input and dispatching it to whichever thread was waiting for it */
            bytes_read = fread(buf, 1, count ,fp);
            lua_pushlstring(L, buf, bytes_read);
            
            printf("\nResuming, stack has %d items, a %s\n", lua_gettop(L), lua_typename(L,lua_type(L,-1)) );
            x = lua_resume(L, NULL, 1);
            if (x != LUA_YIELD) {
                printf("End hit\n\n");
                break;
            }
            printf("Yield hit, stack has %d item, with top item having value of %d\n",
                   lua_gettop(L), (int)lua_tointeger(L, -1));
            count = (size_t)lua_tointeger(L, -1);
            lua_pop(L, lua_gettop(L));
        }

    end:
        lua_pop(L, lua_gettop(L));
        fclose(fp);
    }

    fprintf(stderr, "Exiting...\n");
    lua_close(L);
    return 0;
}
