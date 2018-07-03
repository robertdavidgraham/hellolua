/*
    hello01.c - A minimal Lua integration example
 
 This example shows:
    * how to create a Lua VM/state instance
    * how to run a script contained in a string
    * how to run a script contained in a file
    * how to close/free a VM
 
 This shows the bare minimum of running a Lua script within C. It just runs
 a script, but is not yet integrated in terms of passing values between Lua and C.

 This program does add some complexity. Instead of calling "luaL_dofile()" to
 simply run a script in one step, it breaks apart the steps into "luaL_loadfile()"
 followed by "lua_pcall()". This is so we can highlight how loading a script
 compiles it. The hello01-*.lua script show errors that highlight, one
 a syntax error when the script is loaded, the other a runtime error.
*/
#include <stdio.h>
#include "lua/lua.h"
#include "lua/lauxlib.h"
#include "lua/lualib.h"


int main(int argc, char *argv[])
{
    lua_State *L;
    int i;
    
    fprintf(stderr, "Running: hello01\n");
    
    /*
     * Create an instances of the Lua interpreter/VM. In theory, there can be
     * multiple instances running side-by-side, as it's completely re-entrent.
     * In practice, most integration has just one instance. By convention,
     * the variable used for this is "L".
     */
    fprintf(stderr, "Creating interpreter instance/VM\n");
    L = luaL_newstate();
    
    /*
     * Register all the basic libraries. By default, the VM has no library,
     * so you can't do much with it, not even print() output to the screen. This
     * step attaches basic libraries to the VM. When doing full integration,
     * you'll want to change which libraries get registered, for esecurity
     * reasons, to limit the threat of hacker-written scripts hacking the
     * system.
     */
    fprintf(stderr, "Registering standard libraries\n");
    luaL_openlibs(L);
    
    /*
     * Run a minimal Lua program, specified as a C string. Our sample script
     * simply prints a line of text.
     */
    luaL_dostring(L, "print('Hello Lua!!')");

    /*
     * Also run some external scripts, if you've specified one on the command
     * line. There are several scripts of the form "hello01*.lua" that you'll
     * want to play with, including showing how errors work.
     */
    for (i=1; i<argc; i++) {
        int x;
        const char *filename = argv[i];
    
        /*
         * Load a script file. This will also compile the script into byte-code.
         * If there is a "syntax error", so that the script cannot be compiled,
         * then it will be generated here. The compiled script is placed on the
         * top of the Lua stack.
         */
        fprintf(stderr, "Loading script file: %s\n", filename);
        x = luaL_loadfile(L, filename);
        if (x != LUA_OK) {
            fprintf(stderr, "error loading: %s: %s\n", filename, lua_tostring(L, -1));
            continue;
        }
        
        /*
         * Run the script. If there is a runtime error, like a function not
         * found, then it will be generated here. The "pcall()" function runs
         * whatever function is on the top of the stack, which in this example
         * will be the script we just loaded above.
         */
        fprintf(stderr, "Running script file: %s\n", filename);
        x = lua_pcall(L, 0, 0, 0);
        if (x != LUA_OK) {
            fprintf(stderr, "error running: %s: %s\n", filename, lua_tostring(L, -1));
        }
    }
    
    /*
     * Now that we are done running everything, close and exit.
     */
    fprintf(stderr, "Exiting...\n");
    lua_close(L);

    return 0;
}
