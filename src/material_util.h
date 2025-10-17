#ifndef soluna_material_util_h
#define soluna_material_util_h

#include <lua.h>
#include <lauxlib.h>

#define MATERIAL_TEXT_NORMAL 1
#define MATERIAL_QUAD 2
#define MATERIAL_MASK 3

static inline void
ref_object(lua_State *L, void *ptr, int uv_index, const char *key, const char *luatype, int direct) {
	if (lua_getfield(L, 1, key) != LUA_TUSERDATA)
		luaL_error(L, "Invalid key .%s", key);
	void *obj = luaL_checkudata(L, -1, luatype);
	lua_pushvalue(L, -1);
	// ud, object, object
	lua_setiuservalue(L, -3, uv_index);
	if (!direct) {
		lua_pushlightuserdata(L, ptr);
		lua_call(L, 1, 0);
	} else {
		lua_pop(L, 1);
		void **ref = (void **)ptr;
		*ref = obj;
	}
}

static inline void
submit_material(lua_State *L, int batch_n, void *mat, void (*submit)(lua_State *, void *, struct draw_primitive *, int)) {
	struct draw_primitive *prim = lua_touserdata(L, 2);
	int prim_n = luaL_checkinteger(L, 3);
	int i = 0;
	for (;;) {
		int n = prim_n - i;
		if (n > batch_n) {
			submit(L, mat, prim, batch_n);
			i += batch_n;
			prim += batch_n;
		} else {
			submit(L, mat, prim, n);
			i += batch_n;
			break;
		}
	}
}

#endif