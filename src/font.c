#include "font_manager.h"
#include "truetype.h"

#include <lua.h>
#include <lauxlib.h>

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>

#include "luabuffer.h"
#include "stb/stb_image_write.h"

static struct {
	struct font_manager *mgr;
} G;

static struct font_manager* 
getF(lua_State *L) {
	if (G.mgr == NULL)
		luaL_error(L, "Init font manager first");
	return G.mgr;
}

static int
lsubmit(lua_State *L){
	struct font_manager *F = getF(L);
	int dirty = font_manager_flush(F);
	lua_pushboolean(L, dirty);
	return 1;
}

static int
limport(lua_State *L) {
	struct font_manager *F = getF(L);
	size_t sz;
	void* fontdata = (void *)luaL_getbuffer(L, &sz);
	font_manager_import(F, fontdata, sz);
	return 0;
}

static int
lname(lua_State *L) {
	struct font_manager *F = getF(L);
	const char* family = luaL_checkstring(L, 1);
	const int fontid = font_manager_addfont_with_family(F, family);
	if (fontid > 0){
		lua_pushinteger(L, fontid);
		return 1;
	}
	return 0;
}

static int
ltexture(lua_State *L) {
	struct font_manager *F = getF(L);
	int size = 0;
	const void * ptr = font_manager_texture(F, &size);
	lua_pushlightuserdata(L, (void *)ptr);
	lua_pushinteger(L, size * size);
	return 2;
}

static int
ltexture_write(lua_State *L) {
	struct font_manager *F = getF(L);
	int size = 0;
	const void * ptr = font_manager_texture(F, &size);
	const char * filename = luaL_checkstring(L, 1);
	if (!stbi_write_png(filename, size, size, 1, ptr, size)) {
		return luaL_error(L, "Write %s failed", filename);
	}
	return 0;
}

static int
ltouch(lua_State *L) {
	struct font_manager *F = getF(L);
	int fontid = luaL_checkinteger(L, 1);
	int codepoint = luaL_checkinteger(L, 2);
	struct font_glyph tmp1, tmp2;
	font_manager_glyph(F, fontid, codepoint, 16, &tmp1, &tmp2);
	return 0;
}

static int
lcobj(lua_State *L) {
	struct font_manager *F = getF(L);
	lua_pushlightuserdata(L, (void *)F);
	return 1;
}

static int
limport_icon(lua_State *L) {
	struct font_manager *F = getF(L);
	luaL_checktype(L, 1, LUA_TUSERDATA);
	void *data = lua_touserdata(L, 1);
	size_t sz = lua_rawlen(L, 1);
	int n = sz / FONT_MANAGER_GLYPHSIZE / FONT_MANAGER_GLYPHSIZE;
	if (n * FONT_MANAGER_GLYPHSIZE * FONT_MANAGER_GLYPHSIZE != sz)
		return luaL_error(L, "Invalid icon bundle size");
	font_manager_icon_init(F, n, data);
	return 0;
}

static int
lsize(lua_State *L) {
	struct font_manager *F = getF(L);
	int font_id = luaL_checkinteger(L, 1);
	if (font_id <= 0) {
		return luaL_error(L, "Invalid font_id %d", font_id);
	}
	int size = luaL_checkinteger(L, 2);
	int ascent, descent, lineGap;
	font_manager_fontheight(F, font_id, size, &ascent, &descent, &lineGap);
	if (!lua_istable(L, 3)) {
		lua_settop(L, 2);
		lua_createtable(L, 0, 3);
	} else {
		lua_settop(L, 3);
	}
	lua_pushinteger(L, ascent);
	lua_setfield(L, 3, "ascent");
	lua_pushinteger(L, descent);
	lua_setfield(L, 3, "descent");
	lua_pushinteger(L, lineGap);
	lua_setfield(L, 3, "lineGap");
	return 1;
}

#define MAX_FONTNAME 256

static int
llist(lua_State *L) {
	struct font_manager *F = getF(L);
	int i = 1;
	lua_newtable(L);
	char buf[MAX_FONTNAME];
	for (;;) {
		int r = font_manager_enum_fontname(F, i, buf, MAX_FONTNAME);
		if (r <= 0)
			break;
		if (r >= MAX_FONTNAME) {
			char *tmp = (char *)malloc(r + 1);
			font_manager_enum_fontname(F, i, tmp, r+1);
			lua_pushlstring(L, tmp, r);
			free(tmp);
		} else {
			lua_pushlstring(L, buf, r);
		}
		lua_seti(L, -2, i);
		++i;
	}
	return 1;
}

int
luaopen_font(lua_State *L) {
	luaL_checkversion(L);
	
	luaL_Reg l[] = {
		{ "texture",			ltexture },
		{ "texture_write",		ltexture_write },
		{ "touch",				ltouch },	// for debug
		{ "import",				limport },
		{ "name",				lname },
		{ "size",				lsize },
		{ "submit",				lsubmit },
		{ "cobj",				lcobj },
		{ "texture_size",		NULL },
		{ "import_icon",		limport_icon },
		{ "list",				llist },
		{ NULL, 				NULL },
	};
	
	luaL_newlib(L, l);

	lua_pushinteger(L, FONT_MANAGER_TEXSIZE);
	lua_setfield(L, -2, "texture_size");

	return 1;
}

void soluna_openlibs(lua_State *L);

static int
luavm_init(lua_State *L) {
	soluna_openlibs(L);
	const char* data = (const char*)lua_touserdata(L, 1);
	size_t size = (size_t)lua_tointeger(L, 2);
	const char* chunkname = (const char*)lua_touserdata(L, 3);
	if (luaL_loadbuffer(L, data, size, chunkname) != LUA_OK) {
		return lua_error(L);
	}
	lua_call(L, 0, 0);
	return 0;
}

static int
fontm_init(lua_State *L) {
	if (G.mgr != NULL) {
		return luaL_error(L, "Do not init font manager twice");
	}
	struct font_manager* F = (struct font_manager *)malloc(font_manager_sizeof());
	if (F == NULL) {
		return luaL_error(L, "not enough memory");
	}
	size_t sz;
	const char * src = luaL_checklstring(L, 1, &sz);

	lua_State* managerL = luaL_newstate();
	if (!managerL) {
		free(F);
		return luaL_error(L, "not enough memory");
	}
	lua_pushcfunction(managerL, luavm_init);
	lua_pushlightuserdata(managerL, (void *)src);
	lua_pushinteger(managerL, sz);
	lua_pushlightuserdata(managerL, (void*)luaL_checkstring(L, 2));
	if (lua_pcall(managerL, 3, 0, 0) != LUA_OK) {
		lua_pushstring(L, lua_tostring(managerL, -1));
		lua_close(managerL);
		free(F);
		return lua_error(L);
	}
	font_manager_init(F, managerL);
	
	G.mgr = F;	
	return 0;
}

static int
fontm_shutdown(lua_State *L) {
	struct font_manager* F = G.mgr;
	if (F == NULL)
		return 0;
	G.mgr = NULL;
	void* managerL = font_manager_shutdown(F);
	if (managerL) {
		lua_close((lua_State*)managerL);
	}
	free(F);
	return 0;
}

int
luaopen_font_manager(lua_State *L) {
	luaL_checkversion(L);
	luaL_Reg l[] = {
		{ "init", fontm_init },
		{ "shutdown", fontm_shutdown },
		{ NULL, NULL },
	};
	luaL_newlib(L, l);

	return 1;
}
