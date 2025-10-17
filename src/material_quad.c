#include <lua.h>
#include <lauxlib.h>
#include <assert.h>
#include <string.h>

#include "sokol/sokol_gfx.h"
#include "colorquad.glsl.h"
#include "srbuffer.h"
#include "batch.h"
#include "spritemgr.h"
#include "material_util.h"
#include "render_bindings.h"
#include "tmpbuffer.h"

struct color {
	unsigned char channel[4];
};

struct quad {
	struct draw_primitive_external header;
	float w;
	float h;
	struct color c;
};

struct inst_object {
	float x, y;
	float w, h;
	int sr_index;
	struct color c;
};

struct material_quad {
	sg_pipeline pip;
	sg_buffer inst;
	struct soluna_render_bindings *bind;
	vs_params_t *uniform;
	struct sr_buffer *srbuffer;
	struct tmp_buffer tmp;
};

static void
submit(lua_State *L, void *m_, struct draw_primitive *prim, int n) {
	struct material_quad *m = (struct material_quad *)m_;
	struct inst_object *tmp = TMPBUFFER_PTR(struct inst_object, &m->tmp);
	int i;
	for (i=0;i<n;i++) {
		struct draw_primitive *p = &prim[i*2];
		assert(p->sprite == -MATERIAL_QUAD);
		
		struct quad * q = (struct quad *)&prim[i*2+1];
		
		// calc scale/rot index
		int sr_index = srbuffer_add(m->srbuffer, p->sr);
		if (sr_index < 0) {
			// todo: support multiply srbuffer
			luaL_error(L, "sr buffer is full");
		}
		struct inst_object *inst = &tmp[i];
		inst->x = (float)p->x / 256.0f;
		inst->y = (float)p->y / 256.0f;
		inst->w = q->w;
		inst->h = q->h;
		inst->sr_index = sr_index;
		inst->c = q->c;
	}
	sg_append_buffer(m->inst, &(sg_range) { tmp , n * sizeof(tmp[0]) });
}

static int
lmateraial_quad_submit(lua_State *L) {
	struct material_quad *m = (struct material_quad *)luaL_checkudata(L, 1, "SOLUNA_MATERIAL_QUAD");
	int batch_n = TMPBUFFER_SIZE(struct inst_object, &m->tmp);
	submit_material(L, batch_n, m, submit);
	return 0;
}

static inline int
lmateraial_quad_draw_(lua_State *L, int ex) {
	struct material_quad *m = (struct material_quad *)luaL_checkudata(L, 1, "SOLUNA_MATERIAL_QUAD");
//	struct draw_primitive *prim = lua_touserdata(L, 2);
	int prim_n = luaL_checkinteger(L, 3);
	if (prim_n <= 0)
		return 0;
	
	sg_apply_pipeline(m->pip);
	sg_apply_uniforms(UB_vs_params, &(sg_range){ m->uniform, sizeof(vs_params_t) });
	if (ex) {
		sg_apply_bindings(&m->bind->bindings);
		sg_draw_ex(0, 4, prim_n, 0, m->bind->base);
	} else {
		size_t base = m->bind->base * sizeof(struct inst_object);
		m->bind->bindings.vertex_buffer_offsets[0] += base;
		sg_apply_bindings(&m->bind->bindings);
		sg_draw(0, 4, prim_n);
		m->bind->bindings.vertex_buffer_offsets[0] -= base;
	}

	m->bind->base += prim_n;

	return 0;
}

static int
lmateraial_quad_draw(lua_State *L) {
	return lmateraial_quad_draw_(L, 0);
}

static int
lmateraial_quad_draw_ex(lua_State *L) {
	return lmateraial_quad_draw_(L, 1);
}

static void
init_pipeline(struct material_quad *m) {
	sg_shader shd = sg_make_shader(colorquad_shader_desc(sg_query_backend()));
  if (sg_query_shader_state(shd) != SG_RESOURCESTATE_VALID) {
    fprintf(stderr, "Failed to create shader for quad material!\n");
  }

	m->pip = sg_make_pipeline(&(sg_pipeline_desc) {
		.layout = {
			.buffers[0].step_func = SG_VERTEXSTEP_PER_INSTANCE,
			.attrs = {
					[ATTR_colorquad_position].format = SG_VERTEXFORMAT_FLOAT4,
					[ATTR_colorquad_idx].format = SG_VERTEXFORMAT_UINT,
					[ATTR_colorquad_c].format = SG_VERTEXFORMAT_UBYTE4N,
				}
        },
		.colors[0].blend = (sg_blend_state) {
			.enabled = true,
			.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
			.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
			.src_factor_alpha = SG_BLENDFACTOR_ONE,
			.dst_factor_alpha = SG_BLENDFACTOR_ZERO
		},
        .shader = shd,
		.primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
        .label = "colorquad-pipeline"
    });
  if (sg_query_pipeline_state(m->pip) != SG_RESOURCESTATE_VALID) {
    fprintf(stderr, "failed to create pipeline for colorquad\n");
  }
}

static int
lnew_material_quad(lua_State *L) {
	luaL_checktype(L, 1, LUA_TTABLE);
	struct material_quad *m = (struct material_quad *)lua_newuserdatauv(L, sizeof(*m), 5);
	ref_object(L, &m->inst, 1, "inst_buffer", "SOKOL_BUFFER", 0);
	ref_object(L, &m->bind, 2, "bindings", "SOKOL_BINDINGS", 1);
	ref_object(L, &m->uniform, 3, "uniform", "SOKOL_UNIFORM", 1);
	ref_object(L, &m->srbuffer, 4, "sr_buffer", "SOLUNA_SRBUFFER", 1);
	tmp_buffer_init(L, &m->tmp, 5, "tmp_buffer");
	init_pipeline(m);

	if (luaL_newmetatable(L, "SOLUNA_MATERIAL_QUAD")) {
		luaL_Reg l[] = {
			{ "__index", NULL },
			{ "submit", lmateraial_quad_submit },
			{ "draw", DRAWFUNC(lmateraial_quad_draw) },
			{ NULL, NULL },
		};
		luaL_setfuncs(L, l, 0);

		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");
	}
	lua_setmetatable(L, -2);
	return 1;
}

struct quad_primitive {
	struct draw_primitive pos;
	union {
		struct draw_primitive dummy;
		struct quad q;
	} u;
};

static int
lquad(lua_State *L) {
	struct quad_primitive prim;
	prim.pos.x = 0;
	prim.pos.y = 0;
	prim.pos.sr = 0;
	prim.pos.sprite = -MATERIAL_QUAD;
	prim.u.q.w = luaL_checkinteger(L, 1);
	prim.u.q.h = luaL_checkinteger(L, 2);
	uint32_t color = luaL_checkinteger(L, 3);
	if (!(color & 0xff000000))
		color |= 0xff000000;
	prim.u.q.header.sprite = -1;
	prim.u.q.c.channel[0] = (color >> 16) & 0xff;
	prim.u.q.c.channel[1] = (color >> 8) & 0xff;
	prim.u.q.c.channel[2] = color & 0xff;
	prim.u.q.c.channel[3] = (color >> 24) & 0xff;
	lua_pushlstring(L, (const char *)&prim, sizeof(prim));
	return 1;
}

int
luaopen_material_quad(lua_State *L) {
	luaL_checkversion(L);
	luaL_Reg l[] = {
		{ "quad", lquad },
		{ "new", lnew_material_quad },
		{ "instance_size", NULL },
		{ NULL, NULL },
	};
	luaL_newlib(L, l);

	lua_pushinteger(L, sizeof(struct inst_object));
	lua_setfield(L, -2, "instance_size");
	
	return 1;
}
