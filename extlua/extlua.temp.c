#include <lua.h>
#include <lauxlib.h>
#include <stdarg.h>
#include <assert.h>
#include <stdio.h>

struct lua_api {
	int version;

$API_DECL$
};

static struct lua_api API;

$API_IMPL$

LUA_API
const char *lua_pushfstring (lua_State *L, const char *fmt, ...) {
	const char *ret;
	va_list argp;
	va_start(argp, fmt);
	ret = API.lua_pushvfstring(L, fmt, argp);
	va_end(argp);
	return ret;
}

LUA_API int
lua_gc (lua_State *L, int what, ...) {
	va_list argp;
	va_start(argp, what);
	int p1 = va_arg(argp, int);
	int p2 = va_arg(argp, int);
	int p3 = va_arg(argp, int);
	va_end(argp);
	return API.lua_gc(L, what, p1, p2, p3);
}

LUA_API int
luaL_error(lua_State *L, const char *fmt, ...) {
	va_list argp;
	va_start(argp, fmt);
	luaL_where(L, 1);
	lua_pushvfstring(L, fmt, argp);
	va_end(argp);
	lua_concat(L, 2);
	return lua_error(L);
}

static void
stub_luaL_checkversion_ (lua_State *L, lua_Number ver, size_t sz) {
}

static void stub_lua_createtable (lua_State *L, int narr, int nrec) {
}

static void stub_luaL_setfuncs (lua_State *L, const luaL_Reg *l, int nup) {
}

struct extraspace {
	struct lua_api * api;
};

LUA_API void
luaapi_init(lua_State *L) {
	struct extraspace * ex = (struct extraspace *)lua_getextraspace(L);
	struct lua_api * api = ex->api;
	if (api->version == LUA_VERSION_NUM) {
		API = *api;
		return;
	}
	// stub for luaL_newlib
	API.luaL_checkversion_ = stub_luaL_checkversion_;
	API.lua_createtable = stub_lua_createtable;
	API.luaL_setfuncs = stub_luaL_setfuncs;
}
