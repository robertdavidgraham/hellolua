/*
    hello03.c - Execute C functions from Lua
 
 This example shows:
    * how to construct a C function that can be called from Lua
    * how to register that C function with the Lua VM
    * how to call the C function from a script
    * how error handling works when the function has bad arguments

 In this example program, we demonstate how to register a C function and call
 it from a Lua script.
 
 In previous examples, we just ran a script, but didn't connect the script
 to our C code. In this example, we start to make that connection, by
 allowing a script to call a C function.
 */
#include <stdio.h>
#include "lua/lua.h"
#include "lua/lauxlib.h"
#include "lua/lualib.h"

/* This is the internal C function we'll be calling. All the C functions that Lua scripts
 * can call will have the same format, taking a single parameter (the Lua VM)
 * and returning an integer.
 *
 * The actual parameters are stored within the 'stack' within the VM. Remember
 * that the 'stack' is alwasy the point of contact between C and Lua. */
static int cfunc_printx(lua_State *L)
{
    const char *str;
    
    /* The arguments are passed on a "stack" within the VM. They are retrieved
     * by grabbing them by their indexed values, 1 for the first argument,
     * 2 for the second, 3 for the third, and so forth. You can figure
     * out how many arguments there are by calling lua_gettop(L), which
     * gets the index of the top argument on the stack. Remember that positive
     * stack indexes are from the bottom up, while negative stack indexes are from
     * the bottom down, so an index of -1 will be the last argument to this function.
     *
     * Now, we could just call "lua_tostring(L,1)" here to get the string,
     * as we showed in our last example of getting to Lua values on the stack,
     * but what happens if the item isn't a string, or doesn't even exist?
     * We need error checking, so we use a variant of this that first checks for
     * errors. If there is an error, this function won't return, but instead will
     * call a "longjmp()" function back int othe interpreter, which will print
     * the error message and exit. Always use the "check" version of these
     * functions in order to grab the parameters to the function.
     *
     * The returned 'str' string points to a buffer held somewhere in the VM
     * that will be garbage collected after we exit this function. Therefore,
     * we can use this pointer now in the function, such as to print it out,
     * but we we want to hold onto it later, we have to make a copy of the
     * string. */
    str = luaL_checkstring(L, 1);
    
    /* Now that we've got a nul-terminated string, we can print it out. */
    printf(": %s\n", str);
    
    /* Lua C functions return an integer indicating the number of return values.
     * A function can return more than one. If we had something to return,
     * we'd use "push" functions to push them onto the stack. In this example,
     * we don't have anything to return */
    return 0;
}


int main(int argc, char *argv[])
{
    lua_State *L;
    int i;
    int x;
    
    fprintf(stderr, "Running: hello03\n");
    fprintf(stderr, "Creating interpreter instance/VM\n");
    L = luaL_newstate();
    luaL_openlibs(L);

    /*
     * Register our C function. We first push the function onto the stack,
     * then we bind a name "printx" to the function, which will pop it from the
     * stack. This works as in our previous examples pushing integers and
     * strings on the stack, only now we are doing the same things with a function.
     * We can also just call the function "lua_register()" here, which is actually
     * just a macro executing these two functions
     */
    lua_pushcfunction(L, cfunc_printx);
    lua_setglobal(L, "printx");
    
    /*
     * Now let's execute a script, which will call this function.
     */
    luaL_dostring(L, "printx('--- called C function ---')");
    
    /*
     * Example calling the function with something that isn't a string,
     * but which will automatically be converted to a string anyway.
     */
    luaL_dostring(L, "printx(5)");
    
    /*
     * Example calling the function with something that cannot be converted
     * to a string, which will generate an error. I suppose I should also
     * handle possible errors in the above two examples, but I'm lazy.
     * Note that when we call "dostring()" here, the code will do a 'setjmp()'
     * in C, and then when "checkstring()" encounters this error, it'll set
     * an error message in the VM, then execute a "longjmp()" back,
     * terminating the script in the middle. The garbage collector will clean
     * up any memory allocated that's now been discarded.
     */
    fprintf(stderr, "The following error is supposed to happen\n");
    x = luaL_dostring(L, "printx(nil)");
    if (x != LUA_OK) {
        fprintf(stderr, "error: %s\n", lua_tostring(L, -1));
    }

    /*
     * Even though we've forced an error above, the VM isn't broken. We can
     * continue to use it to execute scripts.
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
    
    fprintf(stderr, "Exiting...\n");
    lua_close(L);
    return 0;
}
