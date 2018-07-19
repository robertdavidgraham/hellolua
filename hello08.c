/* 0123456789ABCDEFGHIJKLMNOPQRSTUVWXZabcdefghijklmnopqrstuvqxyz
 
    hello08.c - dynamic loaded library

 This example has nothing to do with Lua. Instead, it attempts to load
 the Lua library dynamically at RUNTIME. This doesn't mean at loadtime,
 but after the program has started running.

*/
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if 0
#include "lua/lua.h"
#include "lua/lauxlib.h"
#include "lua/lualib.h"
#else
#include "stub-lua.h"
#endif

static int cfunc_myread(lua_State *L)
{
    int count;
    count = (int)luaL_checkinteger(L,1);
    lua_pushinteger(L, count);
    return lua_yield(L, 1);
}


int main(int argc, char *argv[])
{
    lua_State *L;
    int x;
    
    fprintf(stderr, "Running: hello08\n");
    
    stublua_init();
    
    fprintf(stderr, "Lua Version = %d\n", (int)*lua_version(0));
    
    fprintf(stderr, "Creating interpreter instance/VM\n");
    L = luaL_newstate();
    luaL_openlibs(L);
    lua_register(L, "myread", cfunc_myread);
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
