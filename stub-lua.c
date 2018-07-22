#define LUAAPI
#include "stub-lua.h"
#include <dlfcn.h>



int stublua_init(void)
{
    void *lib = NULL;
    
    {
        static const char *possible_names[] = {
            "liblua5.3.so",
            "liblua5.3.so.0",
            "liblua5.3.so.0.0.0",
            "liblua.5.3.5.dylib",
            "liblua.5.3.dylib",
            "liblua5.3.dylib",
            "liblua.dylib",
            0
        };
        unsigned i;
        for (i=0; possible_names[i]; i++) {
            lib = dlopen(possible_names[i], RTLD_LAZY);
            if (lib) {
                break;
            } else {
                ;
            }
        }
        
        if (lib == NULL) {
            fprintf(stderr, "liblua: failed to load libpcap shared library\n");
            fprintf(stderr, "    HINT: you must install Lua library\n");
        }
    }
    
#define DOLINK(name) \
    name = dlsym(lib, #name); \
    if (name == NULL) fprintf(stderr, "liblua: %s: failed\n", #name);
    
    DOLINK(lua_version);
    
    
    DOLINK(lua_close)
    DOLINK(lua_getfield)
    DOLINK(lua_getglobal)
    DOLINK(lua_gettop)
    DOLINK(lua_newthread)
    DOLINK(lua_newuserdata)
    DOLINK(lua_pcallk)
    DOLINK(lua_pushcclosure)
    DOLINK(lua_pushinteger)
    DOLINK(lua_pushlstring)
    DOLINK(lua_pushnumber)
    DOLINK(lua_pushstring)
    DOLINK(lua_pushvalue)
    DOLINK(lua_resume)
    DOLINK(lua_setfield)
    DOLINK(lua_setglobal)
    DOLINK(lua_settop)
    DOLINK(lua_toboolean)
    DOLINK(lua_tointegerx)
    DOLINK(lua_tolstring)
    DOLINK(lua_tonumberx)
    DOLINK(lua_type)
    DOLINK(lua_typename)
    DOLINK(lua_version)
    DOLINK(lua_xmove)
    DOLINK(lua_yieldk)
    
    DOLINK(luaL_checkinteger)
    DOLINK(luaL_checklstring)
    DOLINK(luaL_checkudata)
    DOLINK(luaL_loadbufferx)
    DOLINK(luaL_loadfilex)
    DOLINK(luaL_loadstring)
    DOLINK(luaL_newmetatable)
    DOLINK(luaL_newstate)
    DOLINK(luaL_openlibs)
    DOLINK(luaL_ref)
    DOLINK(luaL_setfuncs)
    DOLINK(luaL_setmetatable)
    DOLINK(luaL_unref)
    
    return 0;
}
