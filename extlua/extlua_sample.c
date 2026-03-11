#include <lua.h>
#include <lauxlib.h>

LUA_API void luaapi_init(lua_State *L);

#if defined(_WIN32)
#define EXTLUA_EXPORT __declspec(dllexport)
#else
#define EXTLUA_EXPORT __attribute__((visibility("default")))
#endif

static int
lhello(lua_State *L) {
	lua_pushstring(L, "Hello World From Sample");
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

EXTLUA_EXPORT int
extlua_init(lua_State *L) {
	luaapi_init(L);
	luaL_Reg l[] = {
		{ "ext.foobar", luaopen_foobar },
		{ NULL, NULL },
	};
	luaL_newlib(L, l);
	return 1;
}
