/*
    hello06.c - threads/coroutines

 This example show:
    * newstate with custom allocation function
    * the size of things, how much memory they take
 
*/
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lua/lua.h"
#include "lua/lauxlib.h"
#include "lua/lualib.h"

static size_t bytes_allocated = 0;
static size_t count_allocations = 0;
static size_t count_frees = 0;

/*
 * This allocation function counts the number of bytes allocated so that we can
 * track how much memory is being used.
 */
static void *my_alloc(void *userdata, void *ptr, size_t old_size, size_t new_size)
{
    bytes_allocated += new_size;
    bytes_allocated -= old_size;
    
    (void)userdata;
    
    if (new_size == 0) {
        count_frees++;
        free(ptr);
        return NULL;
    } else {
        count_allocations++;
        return realloc(ptr, new_size);
    }
}



int main(int argc, char *argv[])
{
    lua_State *L;
    lua_State *L2;
    size_t old_count, old_allocations, old_frees;
    
    fprintf(stderr, "Running: hello06\n");
    fprintf(stderr, "Creating interpreter instance/VM\n");
    
    /*
     * Create an instance with my own allocation function, instead of the default
     * one normally used.
     */
    old_count = bytes_allocated;
    old_allocations = count_allocations;
    old_frees = count_frees;
    L = lua_newstate(my_alloc, 0);
    printf("newstate  = %6u bytes, %4u allocs, %4u frees\n",
           (unsigned)(bytes_allocated - old_count),
           (unsigned)(count_allocations - old_allocations),
           (unsigned)(count_frees - old_frees));
    
    /*
     * How much memory it takes to register all the standard libraries
     */
    old_count = bytes_allocated;
    old_allocations = count_allocations;
    old_frees = count_frees;
    luaL_openlibs(L);
    printf("openlibs  = %6u bytes, %4u allocs, %4u frees\n",
           (unsigned)(bytes_allocated - old_count),
           (unsigned)(count_allocations - old_allocations),
           (unsigned)(count_frees - old_frees));

    /*
     * How much memory it takes to create a new thread
     */
    old_count = bytes_allocated;
    old_allocations = count_allocations;
    old_frees = count_frees;
    L2 = lua_newthread(L);
    printf("newthread = %6u bytes, %4u allocs, %4u frees\n",
           (unsigned)(bytes_allocated - old_count),
           (unsigned)(count_allocations - old_allocations),
           (unsigned)(count_frees - old_frees));

    /*
     * Load a script
     */
    old_count = bytes_allocated;
    old_allocations = count_allocations;
    old_frees = count_frees;
    luaL_loadstring(L, "xx=5; print(xx);");
    printf("loadstring= %6u bytes, %4u allocs, %4u frees\n",
           (unsigned)(bytes_allocated - old_count),
           (unsigned)(count_allocations - old_allocations),
           (unsigned)(count_frees - old_frees));
    
    /*
     * Run a script
     */
    old_count = bytes_allocated;
    old_allocations = count_allocations;
    old_frees = count_frees;
    lua_pcall(L, 0, 0, 0);
    printf("pcall     = %6u bytes, %4u allocs, %4u frees\n",
           (unsigned)(bytes_allocated - old_count),
           (unsigned)(count_allocations - old_allocations),
           (unsigned)(count_frees - old_frees));
    
    /*
     * Collct garbage LUA_GCCOLLECT
     */
    old_count = bytes_allocated;
    old_allocations = count_allocations;
    old_frees = count_frees;
    lua_gc(L, LUA_GCCOLLECT, 0);
    printf("gc        = %6u bytes, %4u allocs, %4u frees\n",
           (unsigned)(bytes_allocated - old_count),
           (unsigned)(count_allocations - old_allocations),
           (unsigned)(count_frees - old_frees));

    fprintf(stderr, "Exiting...\n");
    lua_close(L);
    return 0;
}
