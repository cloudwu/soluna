#include <lua.h>
#include <lauxlib.h>
#include <stdlib.h>
#include <string.h>

FILE * fopen_utf8(const char *filename, const char *mode);

static int
lfile_exist(lua_State *L) {
	const char *filename = luaL_checkstring(L, 1);
	const char *mode = "rb";
	FILE *f = fopen_utf8(filename, mode);
	if (f == NULL)
		return 0;
	fclose(f);
	lua_pushboolean(L, 1);
	return 1;
}

static void *
external_free(void *ud, void *ptr, size_t osize, size_t nsize) {
	free(ptr);
	return NULL;
}

static int
lfile_load(lua_State *L) {
	const char *filename = luaL_checkstring(L, 1);
	const char *mode = luaL_optstring(L, 2, "rb");
	FILE *f = fopen_utf8(filename, mode);
	if (f == NULL)
		return 0;
	fseek(f, 0, SEEK_END);
	size_t sz = ftell(f);
	fseek(f, 0, SEEK_SET);
	char * buffer = (char *)malloc(sz+1);
	if (buffer == NULL) {
		fclose(f);
		return luaL_error(L, "lfile_load_string : Out of memory");
	}
	buffer[sz] = 0;
	size_t rd = fread(buffer, 1, sz, f);
	fclose(f);
	
	if (rd != sz) {
		free(buffer);
		return luaL_error(L, "Read %s fail", filename);
	}
	lua_pushexternalstring(L, buffer, sz, external_free, NULL);
	return 1;
}

int
luaopen_soluna_file(lua_State *L) {
	luaL_checkversion(L);
	luaL_Reg l[] = {
		{ "exist", lfile_exist },
		{ "load", lfile_load },
		{ NULL, NULL },
	};
	luaL_newlib(L, l);
	return 1;
}
