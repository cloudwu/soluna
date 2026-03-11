#include <lua.h>
#include <lauxlib.h>

LUA_API void luaapi_init(lua_State *L);

static int
lhello(lua_State *L) {
	lua_pushstring(L, "Hello World");
	return 1;
}

static int
luaopen_foobar(lua_State *L) {
	luaL_Reg l[] = {
		{ "hello", lhello },
		{ NULL, NULL },
	};
	luaL_newlib(L, l);
	return 1;
}

__declspec(dllexport) int
extlua_init(lua_State *L) {
	luaapi_init(L);
	luaL_Reg l[] = {
		{ "ext.foobar", luaopen_foobar },
		{ NULL, NULL },
	};
	luaL_newlib(L, l);
	return 1;
}
