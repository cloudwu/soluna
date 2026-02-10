#include <lua.h>
#include <lauxlib.h>
#include <assert.h>
#include <stdint.h>
#include <math.h>

#include "sokol/sokol_gfx.h"
#include "perspective_quad.glsl.h"
#include "batch.h"
#include "spritemgr.h"
#include "material_util.h"
#include "render_bindings.h"
#include "tmpbuffer.h"

#define PQUAD_CORNER_N 4
#define PQUAD_INFO_CORNER_MASK 0x3u
#define PQUAD_POS_FIX_SCALE 256.0f
#define PQUAD_POS_FIX_INV_SCALE (1.0f / PQUAD_POS_FIX_SCALE)

struct color {
	unsigned char channel[4];
};

struct pquad_meta {
	struct draw_primitive_external header;
	uint32_t info;
	float q;
	struct color color;
};

struct corner_primitive {
	struct draw_primitive pos;
	union {
		struct draw_primitive dummy;
		struct pquad_meta meta;
	} u;
};

struct inst_object {
	float pos_h0[3];
	float pos_h1[3];
	float pos_h2[3];
	float uv_rect[4];
	float q[4];
	struct color color;
};

struct material_perspective_quad {
	sg_pipeline pip;
	sg_buffer inst;
	struct soluna_render_bindings *bind;
	vs_params_t *uniform;
	struct sprite_bank *bank;
	struct tmp_buffer tmp;
};

#define PQUAD_EPSILON 0.000001f

static void
set_position_homography(const float pos[PQUAD_CORNER_N][2], struct inst_object *inst) {
	const float x0 = pos[0][0], y0 = pos[0][1];
	const float x1 = pos[1][0], y1 = pos[1][1];
	const float x2 = pos[2][0], y2 = pos[2][1];
	const float x3 = pos[3][0], y3 = pos[3][1];

	const float sx = x0 - x1 + x3 - x2;
	const float sy = y0 - y1 + y3 - y2;
	const float dx1 = x1 - x3;
	const float dx2 = x2 - x3;
	const float dy1 = y1 - y3;
	const float dy2 = y2 - y3;
	const float det = dx1 * dy2 - dx2 * dy1;

	float m31 = 0.0f;
	float m32 = 0.0f;
	if (fabsf(det) > PQUAD_EPSILON) {
		m31 = (sx * dy2 - sy * dx2) / det;
		m32 = (sy * dx1 - sx * dy1) / det;
	}

	/* M = [m11 m12 m13; m21 m22 m23; m31 m32 1] */
	const float m11 = x1 - x0 + m31 * x1;
	const float m12 = x2 - x0 + m32 * x2;
	const float m13 = x0;
	const float m21 = y1 - y0 + m31 * y1;
	const float m22 = y2 - y0 + m32 * y2;
	const float m23 = y0;

	inst->pos_h0[0] = m11;
	inst->pos_h0[1] = m21;
	inst->pos_h0[2] = m31;
	inst->pos_h1[0] = m12;
	inst->pos_h1[1] = m22;
	inst->pos_h1[2] = m32;
	inst->pos_h2[0] = m13;
	inst->pos_h2[1] = m23;
	inst->pos_h2[2] = 1.0f;
}

static inline int
perspective_quad_count(lua_State *L, int prim_n) {
	if (prim_n % PQUAD_CORNER_N != 0) {
		luaL_error(L, "Invalid perspective quad primitive count %d", prim_n);
		return 0;
	}
	return prim_n / PQUAD_CORNER_N;
}

static void
submit(lua_State *L, void *m_, struct draw_primitive *prim, int n) {
	struct material_perspective_quad *m = (struct material_perspective_quad *)m_;
	struct inst_object *tmp = TMPBUFFER_PTR(struct inst_object, &m->tmp);
	struct sprite_rect *rect = m->bank->rect;
	int out_n = perspective_quad_count(L, n);
	int i;
	for (i=0;i<out_n;i++) {
		struct inst_object *inst = &tmp[i];
		int base = i * PQUAD_CORNER_N;
		int j;
		int sprite = -1;
		float pos[PQUAD_CORNER_N][2];
		for (j=0;j<PQUAD_CORNER_N;j++) {
			struct draw_primitive *p = &prim[(base + j) * 2];
			assert(p->sprite == -MATERIAL_PERSPECTIVE_QUAD);
			struct pquad_meta *meta = (struct pquad_meta *)&prim[(base + j) * 2 + 1];
			uint32_t meta_flags = meta->info & ~PQUAD_INFO_CORNER_MASK;
			if (j == 0) {
				sprite = meta->header.sprite;
				if (meta_flags != 0) {
					luaL_error(L, "Invalid perspective quad stream flags 0x%x", meta_flags);
				}
				inst->color = meta->color;
			} else if (meta->header.sprite != sprite || meta_flags != 0) {
				luaL_error(L, "Invalid perspective quad stream");
			}

			uint32_t corner = meta->info & PQUAD_INFO_CORNER_MASK;
			if (corner >= PQUAD_CORNER_N)
				luaL_error(L, "Invalid perspective quad corner %u", corner);

			pos[corner][0] = (float)p->x * PQUAD_POS_FIX_INV_SCALE;
			pos[corner][1] = (float)p->y * PQUAD_POS_FIX_INV_SCALE;

			float q = meta->q;
			if (q <= PQUAD_EPSILON) {
				q = PQUAD_EPSILON;
			}
			inst->q[corner] = q;
		}
		set_position_homography(pos, inst);

		struct sprite_rect *r = &rect[sprite];
		float u = (float)(r->u >> 16);
		float v = (float)(r->v >> 16);
		float uw = (float)(r->u & 0xffff);
		float vh = (float)(r->v & 0xffff);
		inst->uv_rect[0] = u;
		inst->uv_rect[1] = v;
		inst->uv_rect[2] = uw;
		inst->uv_rect[3] = vh;
	}
	sg_append_buffer(m->inst, &(sg_range) { tmp , out_n * sizeof(tmp[0]) });
}

static int
lmaterial_perspective_quad_submit(lua_State *L) {
	struct material_perspective_quad *m = (struct material_perspective_quad *)luaL_checkudata(L, 1, "SOLUNA_MATERIAL_PERSPECTIVE_QUAD");
	int inst_batch_n = TMPBUFFER_SIZE(struct inst_object, &m->tmp);
	if (inst_batch_n < 1) {
		return luaL_error(L, "Perspective quad tmp buffer is too small");
	}
	int batch_n = inst_batch_n * PQUAD_CORNER_N;
	util_submit_material(L, batch_n, m, submit);
	return 0;
}

static inline int
lmaterial_perspective_quad_draw_(lua_State *L, int ex) {
	struct material_perspective_quad *m = (struct material_perspective_quad *)luaL_checkudata(L, 1, "SOLUNA_MATERIAL_PERSPECTIVE_QUAD");
	int prim_n = luaL_checkinteger(L, 3);
	if (prim_n <= 0)
		return 0;
	int quad_n = perspective_quad_count(L, prim_n);

	sg_apply_pipeline(m->pip);
	sg_apply_uniforms(UB_vs_params, &(sg_range){ m->uniform, sizeof(vs_params_t) });

	if (ex) {
		sg_apply_bindings(&m->bind->bindings);
		sg_draw_ex(0, 4, quad_n, 0, m->bind->base);
	} else {
		size_t base = m->bind->base * sizeof(struct inst_object);
		m->bind->bindings.vertex_buffer_offsets[0] += base;
		sg_apply_bindings(&m->bind->bindings);
		sg_draw(0, 4, quad_n);
		m->bind->bindings.vertex_buffer_offsets[0] -= base;
	}
	m->bind->base += quad_n;
	return 0;
}

static int
lmaterial_perspective_quad_draw(lua_State *L) {
	return lmaterial_perspective_quad_draw_(L, 0);
}

static int
lmaterial_perspective_quad_draw_ex(lua_State *L) {
	return lmaterial_perspective_quad_draw_(L, 1);
}

static void
init_pipeline(struct material_perspective_quad *p) {
	sg_pipeline_desc desc = {
		.layout.attrs = {
			[ATTR_perspective_quad_pos_h0].format = SG_VERTEXFORMAT_FLOAT3,
			[ATTR_perspective_quad_pos_h1].format = SG_VERTEXFORMAT_FLOAT3,
			[ATTR_perspective_quad_pos_h2].format = SG_VERTEXFORMAT_FLOAT3,
			[ATTR_perspective_quad_uv_rect].format = SG_VERTEXFORMAT_FLOAT4,
			[ATTR_perspective_quad_q].format = SG_VERTEXFORMAT_FLOAT4,
			[ATTR_perspective_quad_color].format = SG_VERTEXFORMAT_UBYTE4N,
		},
	};
	p->pip = util_make_pipeline(&desc, perspective_quad_shader_desc, "perspective-quad-pipeline", 1);
}

static int
lnew_material_perspective_quad(lua_State *L) {
	luaL_checktype(L, 1, LUA_TTABLE);
	struct material_perspective_quad *m = (struct material_perspective_quad *)lua_newuserdatauv(L, sizeof(*m), 5);
	init_pipeline(m);
	util_ref_object(L, &m->inst, 1, "inst_buffer", "SOKOL_BUFFER", 0);
	util_ref_object(L, &m->bind, 2, "bindings", "SOKOL_BINDINGS", 1);
	util_ref_object(L, &m->uniform, 3, "uniform", "SOKOL_UNIFORM", 1);
	if (lua_getfield(L, 1, "sprite_bank") != LUA_TLIGHTUSERDATA) {
		return luaL_error(L, "Missing .sprite_bank");
	}
	m->bank = lua_touserdata(L, -1);
	lua_pop(L, 1);
	tmp_buffer_init(L, &m->tmp, 4, "tmp_buffer");

	if (luaL_newmetatable(L, "SOLUNA_MATERIAL_PERSPECTIVE_QUAD")) {
		luaL_Reg l[] = {
			{ "__index", NULL },
			{ "submit", lmaterial_perspective_quad_submit },
			{ "draw", DRAWFUNC(lmaterial_perspective_quad_draw) },
			{ NULL, NULL },
		};
		luaL_setfuncs(L, l, 0);

		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");
	}
	lua_setmetatable(L, -2);
	return 1;
}

static inline float
get_number_field(lua_State *L, int index, const char *field, float defv) {
	float v;
	lua_getfield(L, index, field);
	v = luaL_optnumber(L, -1, defv);
	lua_pop(L, 1);
	return v;
}

static void
get_quad(lua_State *L, int index, float quad[8]) {
	if (lua_getfield(L, index, "quad") == LUA_TTABLE) {
		int i;
		for (i=0;i<8;i++) {
			lua_geti(L, -1, i + 1);
			quad[i] = luaL_checknumber(L, -1);
			lua_pop(L, 1);
		}
		lua_pop(L, 1);
		return;
	}
	lua_pop(L, 1);

	float w = get_number_field(L, index, "w", 0.0f);
	float h = get_number_field(L, index, "h", 0.0f);
	float ox = get_number_field(L, index, "ox", 0.0f);
	float oy = get_number_field(L, index, "oy", 0.0f);
	if (w == 0 || h == 0) {
		luaL_error(L, "Perspective quad sprite needs .quad or .w/.h");
		return;
	}
	quad[0] = -ox; quad[1] = -oy;
	quad[2] = w - ox; quad[3] = -oy;
	quad[4] = -ox; quad[5] = h - oy;
	quad[6] = w - ox; quad[7] = h - oy;
}

static void
get_q(lua_State *L, int index, float q[4]) {
	int i;
	if (lua_getfield(L, index, "q") != LUA_TTABLE) {
		for (i=0;i<4;i++) {
			q[i] = 1.0f;
		}
		lua_pop(L, 1);
		return;
	}
	for (i=0;i<4;i++) {
		lua_geti(L, -1, i + 1);
		q[i] = luaL_optnumber(L, -1, 1.0f);
		lua_pop(L, 1);
	}
	lua_pop(L, 1);
}

static struct color
get_color(lua_State *L, int index) {
	uint32_t color;
	struct color c;
	lua_getfield(L, index, "color");
	color = (uint32_t)luaL_optinteger(L, -1, 0xffffffff);
	if (!(color & 0xff000000))
		color |= 0xff000000;
	c.channel[0] = (color >> 16) & 0xff;
	c.channel[1] = (color >> 8) & 0xff;
	c.channel[2] = color & 0xff;
	c.channel[3] = (color >> 24) & 0xff;
	lua_pop(L, 1);
	return c;
}

static int
lperspective_quad_sprite(lua_State *L) {
	int sprite = luaL_checkinteger(L, 1) - 1;
	luaL_checktype(L, 2, LUA_TTABLE);

	float quad[8];
	get_quad(L, 2, quad);
	float scale_x = get_number_field(L, 2, "scale_x", 1.0f);
	float scale_y = get_number_field(L, 2, "scale_y", 1.0f);
	float shear_x = get_number_field(L, 2, "shear_x", 0.0f);
	float shear_y = get_number_field(L, 2, "shear_y", 0.0f);

	float q[4];
	get_q(L, 2, q);
	struct color color = get_color(L, 2);

	struct corner_primitive prim[PQUAD_CORNER_N];
	int i;
	for (i=0;i<PQUAD_CORNER_N;i++) {
		float x = quad[i * 2];
		float y = quad[i * 2 + 1];
		float tx = x * scale_x + y * shear_x;
		float ty = x * shear_y + y * scale_y;

		prim[i].pos.x = (int32_t)(tx * PQUAD_POS_FIX_SCALE);
		prim[i].pos.y = (int32_t)(ty * PQUAD_POS_FIX_SCALE);
		prim[i].pos.sr = 0;
		prim[i].pos.sprite = -MATERIAL_PERSPECTIVE_QUAD;

		prim[i].u.meta.header.sprite = sprite;
		prim[i].u.meta.info = (uint32_t)i;
		prim[i].u.meta.q = q[i];
		prim[i].u.meta.color = color;
	}
	lua_pushlstring(L, (const char *)prim, sizeof(prim));
	return 1;
}

int
luaopen_material_perspective_quad(lua_State *L) {
	luaL_checkversion(L);
	luaL_Reg l[] = {
		{ "new", lnew_material_perspective_quad },
		{ "sprite", lperspective_quad_sprite },
		{ "instance_size", NULL },
		{ NULL, NULL },
	};
	luaL_newlib(L, l);

	lua_pushinteger(L, sizeof(struct inst_object));
	lua_setfield(L, -2, "instance_size");

	return 1;
}
