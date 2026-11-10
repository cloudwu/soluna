#include <lua.h>
#include <lauxlib.h>
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#include "sokol/sokol_app.h"
#include "sokol/sokol_gfx.h"
#include "batch.h"
#include "material_util.h"

#define PIXEL_SCALE 256.0f
#define CLIP_STACK_MAX 64

enum scissor_command {
	SCISSOR_PUSH = 1,
	SCISSOR_POP = 2,
};

struct scissor_rect {
	int x;
	int y;
	int w;
	int h;
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

struct clip {
	struct scissor_rect rect;
	int depth;
};

struct material_clip {
	float *uniform;
	int depth;
	int stack_n;
	struct clip stack[CLIP_STACK_MAX];
};

static int material_id = 0;

static struct scissor_rect
full_scissor(struct material_clip *m) {
	float sx = m->uniform[0];
	float sy = m->uniform[1];
	int w = sx == 0.0f ? 0 : (int)floorf(2.0f / sx + 0.5f);
	int h = sy == 0.0f ? 0 : (int)floorf(2.0f / fabsf(sy) + 0.5f);
	struct scissor_rect r = { 0, 0, w, h };
	return r;
}

static struct scissor_rect
intersect_scissor(struct scissor_rect a, struct scissor_rect b) {
	int x0 = a.x > b.x ? a.x : b.x;
	int y0 = a.y > b.y ? a.y : b.y;
	int x1 = a.x + a.w < b.x + b.w ? a.x + a.w : b.x + b.w;
	int y1 = a.y + a.h < b.y + b.h ? a.y + a.h : b.y + b.h;
	struct scissor_rect r = {
		x0,
		y0,
		x1 > x0 ? x1 - x0 : 0,
		y1 > y0 ? y1 - y0 : 0,
	};
	return r;
}

static struct scissor_rect
current_scissor(struct material_clip *m) {
	int n = m->stack_n - 1;
	return n >= 0 && m->stack[n].depth == m->depth ? m->stack[n].rect : full_scissor(m);
}

static void
apply_scissor(struct material_clip *m) {
	struct scissor_rect r = current_scissor(m);
	float scale = m->depth > 0 ? 1.0f : sapp_dpi_scale();
	if (scale <= 0.0f)
		scale = 1.0f;
	int x0 = (int)floorf((float)r.x * scale);
	int y0 = (int)floorf((float)r.y * scale);
	int x1 = (int)ceilf((float)(r.x + r.w) * scale);
	int y1 = (int)ceilf((float)(r.y + r.h) * scale);
	sg_apply_scissor_rect(x0, y0, x1 > x0 ? x1 - x0 : 0, y1 > y0 ? y1 - y0 : 0, m->uniform[1] < 0);
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

static struct scissor_rect
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
	struct scissor_rect r = {
		(int)floorf(min_x),
		(int)floorf(min_y),
		(int)ceilf(max_x) - (int)floorf(min_x),
		(int)ceilf(max_y) - (int)floorf(min_y),
	};
	return r;
}

static int
lmaterial_clip_reset(lua_State *L) {
	struct material_clip *m = (struct material_clip *)luaL_checkudata(L, 1, "SOLUNA_MATERIAL_CLIP");
	m->stack_n = 0;
	return 0;
}

static int
lmaterial_clip_apply(lua_State *L) {
	struct material_clip *m = (struct material_clip *)luaL_checkudata(L, 1, "SOLUNA_MATERIAL_CLIP");
	m->depth = luaL_optinteger(L, 2, 0);
	apply_scissor(m);
	return 0;
}

static int
lmaterial_clip_submit(lua_State *L) {
	return 0;
}

static int
lmaterial_clip_draw(lua_State *L) {
	struct material_clip *m = (struct material_clip *)luaL_checkudata(L, 1, "SOLUNA_MATERIAL_CLIP");
	struct draw_primitive *prim = lua_touserdata(L, 2);
	int prim_n = luaL_checkinteger(L, 3);
	int i;
	for (i=0;i<prim_n;i++) {
		struct draw_primitive *pos = &prim[i*2];
		assert(pos->sprite == -material_id);
		struct scissor_payload *payload = (struct scissor_payload *)&prim[i*2+1];
		switch (payload->command) {
		case SCISSOR_PUSH: {
			if (m->stack_n >= CLIP_STACK_MAX)
				return luaL_error(L, "Too many nested clip commands");
			struct scissor_rect next = intersect_scissor(current_scissor(m), primitive_scissor(pos, payload));
			m->stack[m->stack_n++] = (struct clip) { next, m->depth };
			apply_scissor(m);
			break;
		}
		case SCISSOR_POP:
			if (m->stack_n <= 0 || m->stack[m->stack_n - 1].depth != m->depth)
				return luaL_error(L, "Clip close without open");
			--m->stack_n;
			apply_scissor(m);
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
	struct material_clip *m = (struct material_clip *)lua_newuserdatauv(L, sizeof(*m), 1);
	memset(m, 0, sizeof(*m));
	util_ref_object(L, &m->uniform, 1, "uniform", "SOKOL_UNIFORM", 1);

	if (luaL_newmetatable(L, "SOLUNA_MATERIAL_CLIP")) {
		luaL_Reg l[] = {
			{ "__index", NULL },
			{ "reset", lmaterial_clip_reset },
			{ "apply", lmaterial_clip_apply },
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
