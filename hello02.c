/*
    hello02.c - Get and set variables

 This example shows:
    * how the 'stack' is used to transfer data to/from the VM
    * how to pass a value to a script as a global variable
    * how to retrieve a value from the VM
 
 Communication between C and Lua happens within the 'stack', which is a data
 structure held within the VM. Lau objects must stay within the VM.
 
 The "pushstring()" function takes a C string, copies it into a Lua object,
 then pushs that object onto a stack. At this point, scripts and Lua
 functions can manipulate the Lua object.
 
 One such function is "setglobal()", which pops a Lua object off the stack
 and assigns it to a global variable with the given name.
 
 The "getglobal()" function works in reverse. It makes a copy of the named
 global variable, then pushes it on the stack.
 
 Functions like "tointeger()" and "tostring()" copy values out of the stack
 back into C variables.
*/
#include <stdio.h>
#include "lua/lua.h"
#include "lua/lauxlib.h"
#include "lua/lualib.h"


int main(int argc, char *argv[])
{
    lua_State *L;
    int n;
    const char *str;
    
    fprintf(stderr, "Running: hello02\n");
    fprintf(stderr, "Creating interpreter instance/VM\n");
    L = luaL_newstate();
    luaL_openlibs(L);

    /*
     * Take some internal C values and set them as variables that
     * can be retrieved by scripts. We first make a copy of the
     * values, wrap them in a Lua object, then push that object
     * onto the stack. We then call a function that pops the Lua
     * object off the stack and assigns it to a new global variable
     * with the specified name.
     */
    lua_pushstring(L, argv[0]);
    lua_setglobal(L, "argv0");
    lua_pushinteger(L, argc);
    lua_setglobal(L, "argc");
    
    /*
     * Here is an example script that uses these variables, just printing
     * out their values.
     * Also, this script creates a
     */
    luaL_dostring(L,
                  "print('Program name = '..argv0);\n"
                  "print('Number of arguments = '..argc);\n"
                  "hello03 = 42;");
    
    
    /*
     * Get the global variable with the given name. This then pushes the value
     * on top of the stack.
     */
    lua_getglobal(L, "hello03");
    
    /* We can then use conversion functions to convert the Lua value into a C value.
     * The value -1 means the item at the top of the stack. Negative indexes count
     * from the top of the stack downward, and positive values from the bottom of the
     * stack upward. */
    n = lua_tointeger(L, -1);
    printf("hello03 = %d\n", n);
    
    /* We can also convert that value to a string (as strings and numbers are broadly
     * interchangeable, as in most scripting languages). This function will reserve
     * some memory inside the VM and clean it up later with garbage collection */
    str = lua_tostring(L, -1);
    printf("hello03 = '%s'\n", str);
    
    /* The tointeger()/tostring() functions don't pop the value off the stack, we
     * we have to clean it up manually. */
    lua_pop(L, 1);
  
    
    fprintf(stderr, "Exiting...\n");
    lua_close(L);
    return 0;
}
