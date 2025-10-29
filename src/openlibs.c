#define linit_c
#define LUA_LIB

#include <stddef.h>
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

void soluna_embed(lua_State* L);

void
soluna_openlibs(lua_State *L) {
	// ignore env. vars.
    lua_pushboolean(L, 1);
    lua_setfield(L, LUA_REGISTRYINDEX, "LUA_NOENV");
	luaL_openlibs(L);
    soluna_embed(L);
}
