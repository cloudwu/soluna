#include <lua.h>
#include <lauxlib.h>
#include <stdlib.h>
#include <string.h>

struct lua_api;
extern struct lua_api * extlua_api();

struct extraspace {
	struct lua_api * api;
};

static int
get_reg(lua_State *L) {
	luaL_Reg *l = (luaL_Reg *)lua_touserdata(L, 1);
	lua_pushnil(L);
	int i = 0;
	while (lua_next(L, 2) != 0) {
		l[i].name = lua_tostring(L, -2);
		l[i].func = lua_tocfunction(L, -1);
		if (l[i].name == NULL || l[i].func == NULL) {
			return luaL_error(L, "Invalid reg table");
		}
		lua_pop(L, 1);
		++i;
	}
	return 0;
}

static int
count_table(lua_State *L, int idx) {
	idx = lua_absindex(L, idx);
	lua_pushnil(L);
	int n = 0;
	while (lua_next(L, idx) != 0) {
		++n;
		lua_pop(L, 1);
	}
	return n;
}

static void
register_libs(lua_State *L, lua_State *dL, int loadonly) {
	if (lua_gettop(dL) == 0 || lua_type(dL, 1) != LUA_TTABLE) {
		luaL_error(L, "Invalid external libs, maybe lua version mismatch");
	}
	int n = count_table(dL, 1);
	int tbl_index = lua_gettop(L);
	luaL_Reg *l = lua_newuserdatauv(L, sizeof(luaL_Reg) * n, 0);
	lua_pushcfunction(dL, get_reg);
	lua_pushlightuserdata(dL, (void *)l);
	lua_pushvalue(dL, 1);
	if (lua_pcall(dL, 2, 0, 0) != LUA_OK) {
		lua_pushstring(L, lua_tostring(dL, -1));
		lua_error(L);
	}
	int i;
	if (loadonly) {
		for (i=0;i<n;i++) {
			lua_pushcfunction(L, l[i].func);
			lua_setfield(L, tbl_index, l[i].name);
		}
	} else {
		for (i=0;i<n;i++) {
			luaL_requiref (L, l[i].name, l[i].func, 0);
			lua_setfield(L, tbl_index, l[i].name);
		}
	}
	lua_settop(L, tbl_index);
}

static int
register_libs_(lua_State *L) {
	luaL_checktype(L, 1, LUA_TTABLE);
	lua_State *dL = (lua_State *)lua_touserdata(L, 2);
	int loadonly = lua_toboolean(L, 3);
	lua_settop(L, 1);
	register_libs(L, dL, loadonly);
	return 1;
}

static int
load_libs(lua_State *L) {
	lua_State *dL = luaL_newstate();
	struct extraspace * ex = (struct extraspace *)lua_getextraspace(dL);
	ex->api = extlua_api();
	lua_CFunction init = lua_tocfunction(L, 1);
	if (init == NULL)
		return luaL_error(L, "Need C function");
	init(dL);
	lua_pushcfunction(L, register_libs_);
	lua_newtable(L);
	lua_pushlightuserdata(L, dL);
	int ok = lua_pcall(L, 2, 1, 0);
	lua_close(dL);
	if (ok != LUA_OK) {
		lua_error(L);
	}
	return 1;
}

struct preload_extlib {
	int n;
	luaL_Reg *l;
};

static struct preload_extlib PRELOAD;

static void
preload_lib(lua_State *L, lua_CFunction init, int result_index) {
	lua_State *dL = luaL_newstate();
	struct extraspace * ex = (struct extraspace *)lua_getextraspace(dL);
	ex->api = extlua_api();
	init(dL);
	lua_pushcfunction(L, register_libs_);
	lua_pushvalue(L, result_index);
	lua_pushlightuserdata(L, dL);
	lua_pushboolean(L, 1);	// loadonly
	int ok = lua_pcall(L, 3, 1, 0);
	lua_close(dL);
	if (ok != LUA_OK) {
		lua_error(L);
	}
}

static int
cmpreg(const void *a, const void *b) {
	const luaL_Reg *ra = (const luaL_Reg *)a;
	const luaL_Reg *rb = (const luaL_Reg *)b;
	return strcmp(ra->name, rb->name);
}

static int
preload_libs(lua_State *L) {
	luaL_checktype(L, 1, LUA_TTABLE);
	if (PRELOAD.l != NULL) {
		return luaL_error(L, "Already preload");
	}
	lua_newtable(L);
	int result_index = lua_gettop(L);
	int i=0;
	while (lua_geti(L, 1, ++i) != LUA_TNIL) {
		lua_CFunction init = lua_tocfunction(L, -1);
		if (init == NULL)
			return luaL_error(L, "Invalid init function at %d", i);
		lua_pop(L, 1);
		preload_lib(L, init, result_index);
	}
	lua_pop(L, 1);
	int n = count_table(L, result_index);
	struct luaL_Reg *l = lua_newuserdatauv(L, sizeof(luaL_Reg) * n, 1);
	
	// ref name strings
	lua_pushvalue(L, result_index);
	lua_setiuservalue(L, -2, 1);
	
	lua_pushcfunction(L, get_reg);
	lua_pushvalue(L, -2);	// l
	lua_pushvalue(L, result_index);
	lua_call(L, 2, 0);

	qsort(l, n, sizeof(luaL_Reg), cmpreg);

	PRELOAD.l = l;
	PRELOAD.n = n;
	
	lua_setfield(L, LUA_REGISTRYINDEX, "EXTLIBS");

	return 1;
}

static lua_CFunction
find_func(luaL_Reg *l, int n, const char *name) {
	int begin = 0;
	int end = n;
	while (begin < end) {
		int mid = (begin + end) / 2;
		int c = strcmp(name, l[mid].name);
		if (c == 0)
			return l[mid].func;
		else if (c < 0) {
			end = mid;
		} else {
			begin = mid + 1;
		}
	}
	return NULL;
}

static int
searcher(lua_State *L) {
	if (PRELOAD.l == NULL || PRELOAD.n == 0)
		return 0;
	if (lua_gettop(L) == 0) {
		// test preload table
		lua_pushboolean(L, 1);
		return 1;
	}
	const char * name = lua_tostring(L, 1);
	lua_CFunction func = find_func(PRELOAD.l, PRELOAD.n, name);
	if (func == NULL) {
		lua_pushfstring(L, "No preload extlua '%s'", name);
		return 1;
	}
	lua_pushcfunction(L, func);
	return 1;
}

int
luaopen_extlua(lua_State *L) {
	luaL_Reg l[] = {
		{ "load", load_libs },
		{ "preload", preload_libs },
		{ "searcher", searcher },
		{ NULL, NULL },
	};
	luaL_newlib(L, l);
	return 1;
}
