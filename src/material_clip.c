#include <lua.h>
#include <lauxlib.h>
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#include "sokol/sokol_gfx.h"
#include "batch.h"
#include "material_util.h"
#include "render_target.h"

#define PIXEL_SCALE 256.0f

enum scissor_command {
	SCISSOR_PUSH = 1,
	SCISSOR_POP = 2,
};

struct scissor_payload {
	struct draw_primitive_external header;
	int command;
	float w;
	float h;
};

struct scissor_primitive {
	struct draw_primitive pos;
	union {
		struct draw_primitive dummy;
		struct scissor_payload scissor;
	} u;
};

static int material_id = 0;

static struct render_clip
intersect_scissor(struct render_clip a, struct render_clip b) {
	int x0 = a.x > b.x ? a.x : b.x;
	int y0 = a.y > b.y ? a.y : b.y;
	int x1 = a.x + a.w < b.x + b.w ? a.x + a.w : b.x + b.w;
	int y1 = a.y + a.h < b.y + b.h ? a.y + a.h : b.y + b.h;
	struct render_clip r = {
		x0,
		y0,
		x1 > x0 ? x1 - x0 : 0,
		y1 > y0 ? y1 - y0 : 0,
	};
	return r;
}

static void
sr_matrix(uint32_t sr, float m[4]) {
	uint32_t scale_fix = sr >> 12;
	float scale = 1.0f;
	if (scale_fix != 0) {
		if (scale_fix >= 0xff000) {
			scale = (float)(scale_fix & 0xfff) * (1.0f / 4096.0f);
		} else {
			scale = (float)scale_fix * (1.0f / 256.0f) + 1.0f;
		}
	}
	uint32_t rot_fix = sr & 0xfff;
	if (rot_fix == 0) {
		m[0] = scale;
		m[1] = 0.0f;
		m[2] = 0.0f;
		m[3] = scale;
	} else {
		const float pi = 3.1415927f;
		float rot = (float)rot_fix * (pi / 2048.0f);
		float cosr = cosf(rot) * scale;
		float sinr = sinf(rot) * scale;
		m[0] = cosr;
		m[1] = -sinr;
		m[2] = sinr;
		m[3] = cosr;
	}
}

static void
point_bounds(float base_x, float base_y, const float m[4], float x, float y, float *min_x, float *min_y, float *max_x, float *max_y) {
	float tx = base_x + x * m[0] + y * m[1];
	float ty = base_y + x * m[2] + y * m[3];
	if (tx < *min_x)
		*min_x = tx;
	if (tx > *max_x)
		*max_x = tx;
	if (ty < *min_y)
		*min_y = ty;
	if (ty > *max_y)
		*max_y = ty;
}

static struct render_clip
primitive_scissor(struct draw_primitive *pos, struct scissor_payload *payload) {
	float base_x = (float)pos->x / PIXEL_SCALE;
	float base_y = (float)pos->y / PIXEL_SCALE;
	float m[4];
	sr_matrix(pos->sr, m);
	float min_x = base_x;
	float min_y = base_y;
	float max_x = base_x;
	float max_y = base_y;
	point_bounds(base_x, base_y, m, payload->w, 0.0f, &min_x, &min_y, &max_x, &max_y);
	point_bounds(base_x, base_y, m, 0.0f, payload->h, &min_x, &min_y, &max_x, &max_y);
	point_bounds(base_x, base_y, m, payload->w, payload->h, &min_x, &min_y, &max_x, &max_y);
	struct render_clip r = {
		(int)floorf(min_x),
		(int)floorf(min_y),
		(int)ceilf(max_x) - (int)floorf(min_x),
		(int)ceilf(max_y) - (int)floorf(min_y),
	};
	return r;
}

static int
lmaterial_clip_reset(lua_State *L) {
	return 0;
}

static int
lmaterial_clip_submit(lua_State *L) {
	return 0;
}

static int
lmaterial_clip_draw(lua_State *L) {
	struct draw_primitive *prim = lua_touserdata(L, 2);
	int prim_n = luaL_checkinteger(L, 3);
	struct render_target *rt = (struct render_target *)lua_touserdata(L, 5);
	int i;
	for (i=0;i<prim_n;i++) {
		struct draw_primitive *pos = &prim[i*2];
		assert(pos->sprite == -material_id);
		struct scissor_payload *payload = (struct scissor_payload *)&prim[i*2+1];
		switch (payload->command) {
		case SCISSOR_PUSH: {
			if (rt->clip_n >= RENDER_CLIP_MAX)
				return luaL_error(L, "Too many nested clip commands");
			struct render_clip current = { 0, 0, rt->width, rt->height };
			if (rt->clip_n > 0)
				current = rt->clip[rt->clip_n - 1];
			rt->clip[rt->clip_n++] = intersect_scissor(current, primitive_scissor(pos, payload));
			render_target_scissor(rt);
			break;
		}
		case SCISSOR_POP:
			if (rt->clip_n <= 0)
				return luaL_error(L, "Clip close without open");
			--rt->clip_n;
			render_target_scissor(rt);
			break;
		default:
			return luaL_error(L, "Invalid clip command %d", payload->command);
		}
	}
	return 0;
}

static int
lnew_material_clip(lua_State *L) {
	luaL_checktype(L, 1, LUA_TTABLE);
	lua_newuserdatauv(L, 0, 0);

	if (luaL_newmetatable(L, "SOLUNA_MATERIAL_CLIP")) {
		luaL_Reg l[] = {
			{ "__index", NULL },
			{ "reset", lmaterial_clip_reset },
			{ "submit", lmaterial_clip_submit },
			{ "draw", lmaterial_clip_draw },
			{ NULL, NULL },
		};
		luaL_setfuncs(L, l, 0);

		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");
	}
	lua_setmetatable(L, -2);
	return 1;
}

static int
lset_material_id(lua_State *L) {
	int id = luaL_checkinteger(L, 1);
	if (id <= 0) {
		return luaL_error(L, "Invalid clip material id %d", id);
	}
	material_id = id;
	return 0;
}

static void
push_command(lua_State *L, int command, float w, float h) {
	if (material_id <= 0) {
		luaL_error(L, "Clip material is not registered");
	}
	struct scissor_primitive prim;
	memset(&prim, 0, sizeof(prim));
	prim.pos.sprite = -material_id;
	prim.u.scissor.header.sprite = -1;
	prim.u.scissor.command = command;
	prim.u.scissor.w = w;
	prim.u.scissor.h = h;
	lua_pushlstring(L, (const char *)&prim, sizeof(prim));
}

static int
lrect(lua_State *L) {
	int n = lua_gettop(L);
	if (n == 0) {
		push_command(L, SCISSOR_POP, 0.0f, 0.0f);
		return 1;
	}
	if (n != 2)
		return luaL_error(L, "Invalid clip rect arguments");
	float w = luaL_checknumber(L, 1);
	float h = luaL_checknumber(L, 2);
	if (w < 0 || h < 0)
		return luaL_error(L, "Invalid clip rect size %f x %f", w, h);
	push_command(L, SCISSOR_PUSH, w, h);
	return 1;
}

int
luaopen_material_clip(lua_State *L) {
	luaL_checkversion(L);
	luaL_Reg l[] = {
		{ "set_material_id", lset_material_id },
		{ "new", lnew_material_clip },
		{ "rect", lrect },
		{ NULL, NULL },
	};
	luaL_newlib(L, l);
	return 1;
}
