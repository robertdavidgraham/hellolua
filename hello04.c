/*
    hello04.c - Objects and classes
 
 This example shows:
    * how to create a userdata object
    * how to construct a class
    * how to create an instance of that class
    * how to finalize before garbage collection

 As in many scripting languages, an 'object' in Lua is just a table/map. Like
 in many scripting languages, a 'class' is represented by some sort of metatable
 containing functions that's associated with the table that is the object.
 
 In this example, we create a Lua 'class' that will wrap the standard FILE i/o
 functions (fwrite(), fread(), etc.). We also need to create a global object
 that will wrap 'fopen()', as this function is not a method of the class,
 but instead a factory the creates new objects of this class.
 
 The 'io' library within Lua already has this functionality, and you can read
 the source code for how they did it. What I'm doing here is trying to write it
 in a way that's perhaps a bit more clear.
*/
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lua/lua.h"
#include "lua/lauxlib.h"
#include "lua/lualib.h"


/**
 * This is the internal name of our "FILE *" class. This name is only intended to be
 * relavent to the C side of things. The VM has a hidden table called the "registry"
 * which stores the metatable under this name. Every time we create an instance
 * of this class, we'll grab this metatable from this registry and attach it to the object.
 *
 * The reason this is relavent is for "type checking", so that on the C side of things
 * we can verify that table input is the proper table. A common cybersecurity/hacking
 * exploit is "type confusion". If we do it right, there should be no confusion.
 */
static const char * MY_FILE_CLASS = "My File Class";


/**
 * This is a simple wrapper around the file pointer. We store the filename
 * so we can print error mesages with it. Also, the filename is a useful
 * flag to indicate this isn't a stdin/stdout/stderr, which we shouldn't close.
 */
struct FileWrapper
{
    FILE *fp;
    char *filename;
};


/* This is the wrapper around 'fopen()' that creates our FILE * object. Like all
 * Lua-callable C functions, it takes a VM pointer as input and returns an integer.
 * As we explained in the last example, the VM will have a 'stack' inside it where
 * the parameters to this function are stored. */
static int cfunc_open(lua_State *L)
{
    const char *filename;
    const char *mode;
    struct FileWrapper *wrapper;
    FILE *fp;

    /* Grab the first parameter. As explained in previous examples, we use 'checkstring()'
     * here to grab the parameter instead of 'tostring()', to verify the input variable
     * exists and is a string. Otherwise, execution stops here (using longjump()) and
     * an error is set. Also, '1' is the index of the first parameter to the function. */
    filename = luaL_checkstring(L, 1);
    
    /* Lua supports optional parameters. A third option (instad of 'tostring()' or
     * 'checkstring()' is to use 'optstring()'. If second parameter (index is '2' on the stack)
     * exists and is a string, then the value is retrieved, otherwise the default
     * value is retrieved instead. */
    mode = luaL_optstring(L, 2, "r");

    /* Now that we have our two input parameters, let's call the raw C function */
    fp = fopen(filename, mode);
    
    /* If there is an error opening the file, return 'nil'. Lua supports multiple
     * parameters on return, so we can also return a string error message and
     * a numeric error code. The return value of '3' indicates there will be
     * three return values. */
    if (fp == NULL) {
        lua_pushnil(L);
        lua_pushstring(L, strerror(errno));
        lua_pushinteger(L, errno);
        return 3;
    }
    
    /* Create a wrapper object. This allocates memory within the VM that will
     * be garbage collected at the right time. */
    wrapper = (struct FileWrapper *)lua_newuserdata(L, sizeof(*wrapper));
    
    /* Fill in the wrapper object */
    wrapper->fp = fp;
    wrapper->filename = strdup(filename);
    
    /* Now assign our metatable of methods to the object. It's at this point that the
     * 'type' of the object becomes established. */
    luaL_setmetatable(L, MY_FILE_CLASS);

    /* at this point, the wrapper object is still on the top of the stack,
     * so return the number '1' indicating one return value */
    return 1;
}

/* This is the wrapper function around the 'fclose()' method. We register this function
 * twice, once as the "close()" method on the object, and again as the "__gc()" finalize
 * method. Lua will call the "__gc()" method on the object before freeing the memory,
 * giving us a chance to "finalize" thie object and close the file handle, if
 * we havne't already manually closed it. In addition, there are subtleties where
 * finalize methods can be called at the wrong time, so when we close the file
 * handle, we have to also set it to NULL to prevent it from being closed again.
 */
static int cfunc_close(lua_State *L)
{
    struct FileWrapper *wrapper;
    
    /* As demonstrated elsewhere, we call "check" functions to not only retrieve
     * the function parameters from the stack, but also to check that they are
     * legitimate and raise an error if they aren't. This 'check' function also
     * goes the extra step and verifies the object has the right metatable -- in
     * otherwise checking the C internal type of the object, after a fashion. */
    wrapper = luaL_checkudata(L, 1, MY_FILE_CLASS);
    
    /* If the file is still open, then close it and free memory */
    if (wrapper != NULL && wrapper->fp != NULL && wrapper->filename != NULL) {
        fprintf(stderr, "Closing the file: %s\n", wrapper->filename);
        fclose(wrapper->fp);
        free(wrapper->filename);
    }
    
    /* Mark the file as closed, so other attempts to read/write it will fail,
     * and we won't attempt to close it again */    
    wrapper->fp = NULL;
    wrapper->filename = 0;
    
    return 0;
}


static int cfunc_read(lua_State *L)
{
    struct FileWrapper *wrapper;
    size_t bytes_to_read;
    size_t bytes_read;
    char *buf;

    /* The first parameter must be a file object. Note the error checking
     * here. We use "checkudata" to verify the type of object, that this is
     * indeed a file and not something else. We then verify the object is
     * still open. If errors occur, a "longjump" will happen at this point
     * back to the interpreter, which will push the appropriate error message
     * on the stack */
    wrapper = luaL_checkudata(L, 1, MY_FILE_CLASS);
    if (wrapper == NULL || wrapper->fp == NULL) {
        luaL_error(L, "attempt to use a closed file");
        lua_assert(wrapper->f);
    }
    
    /* The second parameter must be the number of bytes to read
     * from the file. Note again the error check to verify the
     * type of object is an integer. */
    bytes_to_read = (size_t)luaL_checkinteger(L, 2);
    
    /* Create a temporary buffer to hold incoming data */
    buf = malloc(bytes_to_read);
    
    /* Now do the read of the data. This can return fewer bytes than we asked
     * when files are smaller than the desired amount, or zero if we've reached
     * the end of the file */
    bytes_read = fread(buf, sizeof(char), bytes_to_read, wrapper->fp);
    
    /* If we've reached the end of the file, then make sure a 'nil' is returned
     * instead of data */
    if (bytes_read == 0) {
        free(buf);
        return 0; /* cause return of nil */
    }

    /* Now push the resulting buffer onto the stack and return */
    lua_pushlstring(L, buf, bytes_read);
    free(buf);
    return 1;
}


int main(int argc, char *argv[])
{
    lua_State *L;
    int i;
    int x;
    
    fprintf(stderr, "Running: hello04\n");
    fprintf(stderr, "Creating interpreter instance\n");
    L = luaL_newstate();
    luaL_openlibs(L);

    /*
     * Create a type/class that we'll call "My File". I only implement the
     * 'close()', 'read()', and finalize methods for simplicity, though
     * implementing the rest should be straightforward.
     */
    {
        static const luaL_Reg my_file_methods[] = {
            {"close", cfunc_close},
            //{"flush", f_flush},
            {"read", cfunc_read},
            //{"seek", f_seek},
            //{"setvbuf", f_setvbuf},
            //{"write", f_write},
            {"__gc", cfunc_close},
            //{"__tostring", f_tostring},*/
            {NULL, NULL}
        };
        
        /* Creates a the concept of a type called "My File Class" */
        luaL_newmetatable(L, MY_FILE_CLASS);
    
        /* Sets the "__index" method to point to itself. That's so we only have
         * have to create one table, instead of two tables, one consisting
         * of the meta methods like __gc, and the other containing the
         * class methods like read() and close(). */
        lua_pushvalue(L, -1);  /* duplicate top value */
        lua_setfield(L, -2, "__index");  /* metatable.__index = metatable */
        
        /* Set all the methods for this type */
        luaL_setfuncs(L, my_file_methods, 0);
        
        /* Now clean up the stack */
        lua_pop(L, 1);
    }
    
    /*
     * Create a global object that contains the global/static/factory
     * for the FILE stuff, namely:
     * FILE.open
     * FILE.stdin
     * FILE.stdout
     * FILE.stderr
     */
    {
        static const luaL_Reg my_file_globals[] = {
            {"open", cfunc_open},
            //{"popen", cfunc_popen},
            //{"tmpfile", cfunc_tmpfile},
            {NULL, NULL}
        };
    
        struct FileWrapper *wrapper;
        lua_newtable(L);
        
        /* Add the global functions */
        luaL_setfuncs(L, my_file_globals, 0);
        
        /* Add the standard file handles */
        wrapper = (struct FileWrapper *)lua_newuserdata(L, sizeof(*wrapper));
        wrapper->fp = stderr;
        wrapper->filename = 0;
        luaL_setmetatable(L, MY_FILE_CLASS);
        lua_setfield(L, -2, "stderr");
        
        wrapper = (struct FileWrapper *)lua_newuserdata(L, sizeof(*wrapper));
        wrapper->fp = stdin;
        wrapper->filename = 0;
        luaL_setmetatable(L, MY_FILE_CLASS);
        lua_setfield(L, -2, "stdin");
        
        
        wrapper = (struct FileWrapper *)lua_newuserdata(L, sizeof(*wrapper));
        wrapper->fp = stdout;
        wrapper->filename = 0;
        luaL_setmetatable(L, MY_FILE_CLASS);
        lua_setfield(L, -2, "stdout");

        
        /* Now finally create a global object called "FILE" that points to this table,
         * so that "f = FILE.open()" will open a file, and that "f = FILE.stdin" will
         * get <stdin>, and so forth.
         */
        lua_setglobal(L, "FILE");
        
    }

    /* Run "hello04.lua" or "hello04-noclose.lua" to try it out */
    for (i=1; i<argc; i++) {
        const char *filename = argv[i];
        
        x = luaL_dofile(L, filename);
        if (x != LUA_OK) {
            fprintf(stderr, "error: %s: %s\n", filename, lua_tostring(L, -1));
            lua_close(L);
            return 0;
        }
    }
    
    /* Notice when running "hello04.lua" script, the file is closed in the script
     * before it reaches this point. However, in "hello04-noclose.lua" script,
     * we don't close the file. Therefore, it gets closed when we do garbage
     * collection at lua_close(), which prints a message after Exiting... */
    fprintf(stderr, "Exiting...\n");
    lua_close(L);
    return 0;
}
