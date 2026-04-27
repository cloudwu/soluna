#include "solunaapi.h"

#include <lauxlib.h>

struct soluna_api {
	int version;

$API_DECL$
};

static struct soluna_api API;

$API_IMPL$

struct lua_api;
struct sokol_api;

struct extlua_apis {
	struct lua_api * lua;
	struct sokol_api * sokol;
	struct soluna_api * soluna;
};

void
solunaapi_init(lua_State *L) {
	struct extlua_apis *apis = *(struct extlua_apis **)lua_getextraspace(L);
	if (apis == NULL || apis->soluna == NULL || apis->soluna->version != SOLUNA_EXT_API_VERSION) {
		int version = (apis != NULL && apis->soluna != NULL) ? apis->soluna->version : 0;
		luaL_error(L, "soluna ext api version mismatch, expected %d got %d", SOLUNA_EXT_API_VERSION, version);
	}
	API = *apis->soluna;
}
