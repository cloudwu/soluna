#include <lua.h>
#include <lauxlib.h>

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
register_libs(lua_State *L, lua_State *dL) {
	if (lua_gettop(dL) == 0 || lua_type(dL, 1) != LUA_TTABLE) {
		luaL_error(L, "Invalid external libs, maybe lua version mismatch");
	}
	int n = count_table(dL, 1);
	luaL_Reg *l = lua_newuserdatauv(L, sizeof(luaL_Reg) * n, 0);
	lua_pushcfunction(dL, get_reg);
	lua_pushlightuserdata(dL, (void *)l);
	lua_pushvalue(dL, 1);
	if (lua_pcall(dL, 2, 0, 0) != LUA_OK) {
		lua_pushstring(L, lua_tostring(dL, -1));
		lua_error(L);
	}
	lua_createtable(L, 0, n);
	int i;
	for (i=0;i<n;i++) {
		luaL_requiref (L, l[i].name, l[i].func, 0);
		lua_setfield(L, -2, l[i].name);
	}
}

static int
register_libs_(lua_State *L) {
	lua_State *dL = (lua_State *)lua_touserdata(L, 1);
	register_libs(L, dL);
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
	lua_pushlightuserdata(L, dL);
	int ok = lua_pcall(L, 1, 1, 0);
	lua_close(dL);
	if (ok != LUA_OK) {
		lua_error(L);
	}
	return 1;
}

int
luaopen_extlua(lua_State *L) {
	luaL_Reg l[] = {
		{ "load", load_libs },
		{ NULL, NULL },
	};
	luaL_newlib(L, l);
	return 1;
}
