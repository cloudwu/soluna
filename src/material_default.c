#include <lua.h>
#include <lauxlib.h>
#include <assert.h>
#include <stdint.h>

#include "sokol/sokol_gfx.h"
#include "texquad.glsl.h"
#include "srbuffer.h"
#include "batch.h"
#include "spritemgr.h"
#include "material_util.h"
#include "render_bindings.h"
#include "tmpbuffer.h"

struct inst_object {
	float x, y;
	float sr_index;
	uint32_t offset;
	uint32_t u;
	uint32_t v;
};

struct material_default {
	sg_pipeline pip;
	sg_buffer inst;
	struct soluna_render_bindings *bind;
	vs_params_t *uniform;
	struct sr_buffer *srbuffer;
	struct sprite_bank *bank;
	struct tmp_buffer tmp;
};

static void
submit(lua_State *L, void *m_, struct draw_primitive *prim, int n) {
	struct material_default *m = (struct material_default *)m_;
	struct sprite_rect *rect = m->bank->rect;
	struct inst_object *tmp = TMPBUFFER_PTR(struct inst_object, &m->tmp);
	int i;
	for (i=0;i<n;i++) {
		struct draw_primitive *p = &prim[i];
		
		// calc scale/rot index
		int sr_index = srbuffer_add(m->srbuffer, p->sr);
		if (sr_index < 0) {
			// todo: support multiply srbuffer
			luaL_error(L, "sr buffer is full");
		}
		tmp[i].x = (float)p->x / 256.0f;
		tmp[i].y = (float)p->y / 256.0f;
		tmp[i].sr_index = (float)sr_index;
		
		int index = p->sprite - 1;
		assert(index >= 0);
		struct sprite_rect *r = &rect[index];
		tmp[i].offset = r->off;
		tmp[i].u = r->u;
		tmp[i].v = r->v;
	}
	sg_append_buffer(m->inst, &(sg_range) { tmp , n * sizeof(tmp[0]) });
}

static int
lmaterial_default_submit(lua_State *L) {
	struct material_default *m = (struct material_default *)luaL_checkudata(L, 1, "SOLUNA_MATERIAL_DEFAULT");
	int batch_n = TMPBUFFER_SIZE(struct inst_object, &m->tmp);
	util_submit_material(L, batch_n, m, submit);
	return 0;
}

static inline int
lmaterial_default_draw_(lua_State *L, int ex) {
	struct material_default *m = (struct material_default *)luaL_checkudata(L, 1, "SOLUNA_MATERIAL_DEFAULT");
//	struct draw_primitive *prim = lua_touserdata(L, 2);
	int prim_n = luaL_checkinteger(L, 3);
//	int tex_id = luaL_checkinteger(L, 4);

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
lmaterial_default_draw(lua_State *L) {
	return lmaterial_default_draw_(L, 0);
}

static int
lmaterial_default_draw_ex(lua_State *L) {
	return lmaterial_default_draw_(L, 1);
}

static void
init_pipeline(struct material_default *p) {
	sg_pipeline_desc desc = {
		.layout.attrs = {
			[ATTR_texquad_position].format = SG_VERTEXFORMAT_FLOAT3,
			[ATTR_texquad_offset].format = SG_VERTEXFORMAT_UINT,
			[ATTR_texquad_u].format = SG_VERTEXFORMAT_UINT,
			[ATTR_texquad_v].format = SG_VERTEXFORMAT_UINT,
        },
	};
	p->pip = util_make_pipeline(&desc, texquad_shader_desc, "default-pipeline", 1);
}

static int
lnew_material_default(lua_State *L) {
	luaL_checktype(L, 1, LUA_TTABLE);
	struct material_default *m = (struct material_default *)lua_newuserdatauv(L, sizeof(*m), 5);
	init_pipeline(m);
	util_ref_object(L, &m->inst, 1, "inst_buffer", "SOKOL_BUFFER", 0);
	util_ref_object(L, &m->bind, 2, "bindings", "SOKOL_BINDINGS", 1);
	util_ref_object(L, &m->uniform, 3, "uniform", "SOKOL_UNIFORM", 1);
	util_ref_object(L, &m->srbuffer, 4, "sr_buffer", "SOLUNA_SRBUFFER", 1);
	tmp_buffer_init(L, &m->tmp, 5, "tmp_buffer");
	if (lua_getfield(L, 1, "sprite_bank") != LUA_TLIGHTUSERDATA) {
		return luaL_error(L, "Missing .sprite_bank");
	}
	m->bank = lua_touserdata(L, -1);
	lua_pop(L, 1);
	
	if (luaL_newmetatable(L, "SOLUNA_MATERIAL_DEFAULT")) {
		luaL_Reg l[] = {
			{ "__index", NULL },
			{ "submit", lmaterial_default_submit },
			{ "draw", DRAWFUNC(lmaterial_default_draw) },
			{ NULL, NULL },
		};
		luaL_setfuncs(L, l, 0);

		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");
	}
	lua_setmetatable(L, -2);
	return 1;
}

int
luaopen_material_default(lua_State *L) {
	luaL_checkversion(L);
	luaL_Reg l[] = {
		{ "new", lnew_material_default },
		{ "instance_size", NULL },
		{ NULL, NULL },
	};
	luaL_newlib(L, l);
	
	lua_pushinteger(L, sizeof(struct inst_object));
	lua_setfield(L, -2, "instance_size");
	return 1;
}
