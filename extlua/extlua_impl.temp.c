#include <lua.h>
#include <lauxlib.h>

struct lua_api {
	int version;

$API_DECL$
};

struct lua_api *
extlua_api() {
	static struct lua_api api = {
		LUA_VERSION_NUM,

$API_STRUCT$
	};
	return &api;
}
