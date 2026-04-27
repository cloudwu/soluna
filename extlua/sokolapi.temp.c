#include <lua.h>

#include "sokol/sokol_gfx.h"

struct sokol_api {
	int version;

$API_DECL$
};

static struct sokol_api API;

$API_IMPL$

struct lua_api;
struct soluna_api;

struct extlua_apis {
	struct lua_api * lua;
	struct sokol_api * sokol;
	struct soluna_api * soluna;
};

void
sokolapi_init(lua_State *L) {
	struct extlua_apis *apis = *(struct extlua_apis **)lua_getextraspace(L);
	API = *apis->sokol;
}
