#include <lua.h>
#include <lauxlib.h>
#include <string.h>

#include "sokol/sokol_gfx.h"

#define UNIFORM_TYPE_FLOAT 1
#define UNIFORM_TYPE_INT 2

static void
set_uniform_float(lua_State *L, int index, uint8_t *buffer, int offset, int n) {
	if (n <= 1) {
		float v = luaL_checknumber(L, index);
		memcpy(buffer + offset, &v, sizeof(float));
	} else {
		luaL_checktype(L, index, LUA_TTABLE);
		if (lua_rawlen(L, index) != n) {
			luaL_error(L, "Need table size %d", n);
		}
		int i;
		float * v = (float *)(buffer + offset);
		for (i=0;i<n;i++) {
			if (lua_rawgeti(L, index, i+1) != LUA_TNUMBER) {
				luaL_error(L, "Invalid value in table[%d]", i+1);
			}
			float tmp = lua_tonumber(L, -1);
			memcpy(v, &tmp, sizeof(float));
			v++;
			lua_pop(L, 1);
		}
	}
}

static void
set_uniform_int(lua_State *L, int index, uint8_t *buffer, int offset, int n) {
	if (n <= 1) {
		int v = luaL_checkinteger(L, index);
		memcpy(buffer + offset, &v, sizeof(int));
	} else {
		luaL_checktype(L, index, LUA_TTABLE);
		if (lua_rawlen(L, index) != n) {
			luaL_error(L, "Need table size %d", n);
		}
		int i;
		int *v = (int *)(buffer + offset);
		for (i=0;i<n;i++) {
			if (lua_rawgeti(L, index, i+1) != LUA_TNUMBER) {
				luaL_error(L, "Invalid value in table[%d]", i+1);
			}
			int tmp = lua_tointeger(L, -1);
			memcpy(v, &tmp, sizeof(int));
			++v;
			lua_pop(L, 1);
		}
	}
}

static int
luniform_set(lua_State *L) {
	uint8_t * p = (uint8_t *)lua_touserdata(L, 1);
	if (p == NULL || lua_getiuservalue(L, 1, 1) != LUA_TTABLE)
		return luaL_error(L, "Invalid uniform setter, call init first");
	lua_pushvalue(L, 2);	// key
	if (lua_rawget(L, -2) != LUA_TNUMBER)
		return luaL_error(L, "No uniform %s", lua_tostring(L, 2));
	// ud key value desc meta
	uint32_t meta = lua_tointeger(L, -1);
	lua_pop(L, 2);
	int type = meta >> 28;
	int offset = (meta >> 14) & 0x3fff;
	int n = meta & 0x3fff;
	switch (type) {
	case UNIFORM_TYPE_FLOAT:
		set_uniform_float(L, 3, p, offset, n);
		break;
	case UNIFORM_TYPE_INT:
		set_uniform_int(L, 3, p, offset, n);
		break;
	default:
		return luaL_error(L, "Invalid uniform setter typeid %s (%d)", lua_tostring(L, 2), type);
	}
	return 0;
}

static void
set_key(lua_State *L, int index, uint8_t *u, int size) {
	const char * key = lua_tostring(L, -2);
	if (lua_getfield(L, -1, "offset") != LUA_TNUMBER) {
		luaL_error(L, "Missing .%s .offset", key);
	}
	int offset = luaL_checkinteger(L, -1);
	lua_pop(L, 1);
	if (offset < 0 || offset > 0x3fff)
		luaL_error(L, "Invalid .%s offset = %d", key, offset);
	
	int t = lua_getfield(L, -1, "n");
	int n = 1;
	if (t != LUA_TNUMBER && t != LUA_TNIL) {
		luaL_error(L, "Invalid type .%s .n (%s)", key, lua_typename(L, t));
	}
	if (t == LUA_TNUMBER) {
		n = luaL_checkinteger(L, -1);
	}
	if (n < 1 || n > 0x3fff)
		luaL_error(L, "Invalid .%s n = %d", key, n);
	lua_pop(L, 1);
	if (lua_getfield(L, -1, "type") != LUA_TSTRING) {
		luaL_error(L, "Missing .%s .type", key);
	}
	const char *type = lua_tostring(L, -1);
	int tid = 0;
	int typesize = 0;
	if (strcmp(type, "float") == 0) {
		tid = UNIFORM_TYPE_FLOAT;
		typesize = sizeof(float);
	} else if (strcmp(type, "int") == 0) {
		tid = UNIFORM_TYPE_INT;
		typesize = sizeof(int);
	} else {
		luaL_error(L, "Invalid .%s .type %s", key, type);
	}
	lua_pop(L, 1);
	int offset_end = offset + n * typesize;
	if (offset_end > size)
		luaL_error(L, "Invalid uniform .%s (offset %d,n %d)", key, offset, n);
	int i;
	for (i=offset;i<offset_end;i++) {
		if (u[i] != 0)
			luaL_error(L, "Overlap uniform .%s (offset %d,n %d)", key, offset, n);
		u[i] = 1;
	}
	uint32_t info = (tid << 28) | (offset << 14) | n;
	lua_pushvalue(L, -2);
	lua_pushinteger(L, info);
	lua_rawset(L, index);
}

static void
check_uniform(lua_State *L, uint8_t *buffer, int size) {
	int i;
	for (i=0;i<size;i++) {
		if (buffer[i] == 0)
			luaL_error(L, "Uniform is not complete");
	}
}

static void
get_typeinfo(lua_State *L, int index, uint8_t *u, int size) {
	lua_newtable(L);	// typeinfo for uniform
	int typeinfo = lua_gettop(L);
	
	// iter desc table 2
	lua_pushnil(L);
	while (lua_next(L, index) != 0) {
		int t = lua_type(L, -2);
		if (t != LUA_TSTRING) {
			if (t == LUA_TNUMBER && lua_tointeger(L, -2) == 1) {
				// size of uniform
				lua_pop(L, 1);
			} else {
				luaL_error(L, "Init error : none string key");
			}
		}
		else if (lua_type(L, -1) != LUA_TTABLE) {
			luaL_error(L, "Init error : invalid .%s", lua_tostring(L, -2));
		} else {
			set_key(L, typeinfo, u, size);
			lua_pop(L, 1);
		}
	}
	check_uniform(L, u, size);
	memset(u, 0, size);
}

static int
luniform_apply(lua_State *L) {
	uint8_t * buffer = (uint8_t *)luaL_checkudata(L, 1, "SOKOL_UNIFORM");
	int size = lua_rawlen(L, 1);
	int slot = luaL_checkinteger(L, 2);
	sg_apply_uniforms(slot, &(sg_range){ buffer, size });
	return 0;
}

int
luniform_new(lua_State *L) {
	luaL_checktype(L, 1, LUA_TTABLE);
	if (lua_geti(L, 1, 1) != LUA_TNUMBER) {
		return luaL_error(L, "Invalid ._size of uniform");
	}
	int size = lua_tointeger(L, -1);
	lua_pop(L, 1);
//	if (size <= 0 || size % 16 != 0) {
//		return luaL_error(L, "Invalid the _size (%d) of uniform should align to 16", size);
//	}
	int align_size = (size + 15) / 16 * 16;
	void * u = lua_newuserdatauv(L, align_size, 1);
	memset(u, 0, align_size);

	get_typeinfo(L, 1, u, size);
	lua_setiuservalue(L, -2, 1);

	if (luaL_newmetatable(L, "SOKOL_UNIFORM")) {
		luaL_Reg l[] = {
			{ "__index", NULL },
			{ "__newindex", luniform_set },
			{ "apply", luniform_apply },
			{ NULL, NULL },
		};
		luaL_setfuncs(L, l, 0);

		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");
	}
	lua_setmetatable(L, -2);
	return 1;
}
