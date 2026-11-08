#include <lua.h>
#include <lauxlib.h>
#include <string.h>

#include "batch.h"

struct target_primitive {
	struct draw_primitive pos;
	union {
		struct draw_primitive dummy;
		struct draw_primitive_external target;
	} u;
};

static int material_id = 0;

static int
lset_material_id(lua_State *L) {
	int id = luaL_checkinteger(L, 1);
	if (id <= 0) {
		return luaL_error(L, "Invalid target material id %d", id);
	}
	material_id = id;
	return 0;
}

static int
ltarget(lua_State *L) {
	if (material_id <= 0) {
		return luaL_error(L, "Target material is not registered");
	}
	struct target_primitive prim;
	memset(&prim, 0, sizeof(prim));
	prim.pos.sprite = -material_id;
	prim.u.target.sprite = luaL_optinteger(L, 1, 0) - 1;
	lua_pushlstring(L, (const char *)&prim, sizeof(prim));
	return 1;
}

static int
lsprite(lua_State *L) {
	struct target_primitive *prim = (struct target_primitive *)lua_touserdata(L, 1);
	int i = luaL_checkinteger(L, 2);
	lua_pushinteger(L, prim[i].u.target.sprite + 1);
	return 1;
}

int
luaopen_material_target(lua_State *L) {
	luaL_checkversion(L);
	luaL_Reg l[] = {
		{ "set_material_id", lset_material_id },
		{ "target", ltarget },
		{ "sprite", lsprite },
		{ NULL, NULL },
	};
	luaL_newlib(L, l);
	return 1;
}
