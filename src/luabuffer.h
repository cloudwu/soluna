#ifndef soluna_luabuffer_h
#define soluna_luabuffer_h

#include <stdint.h>
#include <lua.h>
#include <lauxlib.h>

static inline uint8_t const *
luaL_getbuffer(lua_State *L, size_t *sz) {
	uint8_t const * ret = NULL;
	switch (lua_type(L, 1)) {
	case LUA_TFUNCTION: {
		lua_pushvalue(L, 1);
		lua_call(L, 0, 3);
		ret = (uint8_t const*)lua_touserdata(L, -3);
		*sz = (size_t)luaL_checkinteger(L, -2);
		lua_copy(L, -1, 1);
		int t = lua_type(L, 1);
		if (t == LUA_TUSERDATA || t == LUA_TTABLE)
			lua_toclose(L, 1);
		lua_pop(L, 3);
		break;
	}
	case LUA_TSTRING:
		ret = (uint8_t const *)luaL_checklstring(L, 1, sz);
		break;
	default:
		luaL_error(L, "Invalid buffer type %s", lua_typename(L, lua_type(L, 1)));
	}
	return ret;
}

#endif
