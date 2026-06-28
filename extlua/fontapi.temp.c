#include "fontapi.h"

#include <lauxlib.h>

struct font_api {
	int version;

$API_DECL$
};

static struct font_api API;

$API_IMPL$
struct lua_api;
struct material_api;

struct extlua_apis {
	struct lua_api *lua;
	struct material_api *material;
	struct font_api *font;
};

void
fontapi_init(lua_State *L) {
	struct extlua_apis *apis = *(struct extlua_apis **)lua_getextraspace(L);
	if (apis == NULL || apis->font == NULL || apis->font->version != FONT_API_VERSION) {
		int version = (apis != NULL && apis->font != NULL) ? apis->font->version : 0;
		luaL_error(L, "font api version mismatch, expected %d got %d", FONT_API_VERSION, version);
	}
	API = *apis->font;
}
