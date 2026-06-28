#include <stdint.h>
#include <string.h>

#include <lua.h>
#include <lauxlib.h>

#include "sokol/sokol_gfx.h"
#include "batch.h"
#include "spritemgr.h"
#include "srbuffer.h"
#include "render_bindings.h"
#include "material_util.h"
#include "extlua/materialapi.h"

#define STREAM_FIX_INV_SCALE (1.0f / 256.0f)

union material_func {
	void *ptr;
	material_shader_desc_func shader_desc;
	material_pipeline_desc_func pipeline_desc;
	material_submit_one_func submit_one;
	material_uniform_one_func uniform_one;
};

struct material_external {
	int material_id;
	int instance_size;
	int vertex_count;
	int base_element;
	int uniform_slot;
	int item_uniform_slot;
	int texture_view_slot;
	int has_texture_views;
	sg_pipeline pip;
	sg_buffer inst;
	struct render_bindings *bind;
	void *uniform;
	size_t uniform_size;
	void *item_uniform;
	size_t item_uniform_size;
	struct sr_buffer *srbuffer;
	struct sprite_bank *bank;
	material_submit_one_func submit_one;
	material_uniform_one_func uniform_one;
};

static void
read_sprite_rect(struct sprite_rect *r, struct material_sprite_rect *out) {
	out->texture = r->texid;
	out->u = (float)(r->u >> 16);
	out->v = (float)(r->v >> 16);
	out->w = (float)(r->u & 0xffffu);
	out->h = (float)(r->v & 0xffffu);
	out->ox = (float)((r->off >> 16) & 0xffffu) - 0x8000;
	out->oy = (float)(r->off & 0xffffu) - 0x8000;
}

static void
decode_item(lua_State *L, struct material_external *m, struct draw_primitive *prim, int index, int add_sr, struct material_item *item) {
	struct draw_primitive *pos = &prim[index * 2];
	struct draw_primitive *payload_prim = pos + 1;
	struct draw_primitive_external *ext = (struct draw_primitive_external *)payload_prim;
	if (pos->sprite != -m->material_id) {
		luaL_error(L, "Invalid material marker");
	}
	memset(item, 0, sizeof(*item));
	item->x = (float)pos->x * STREAM_FIX_INV_SCALE;
	item->y = (float)pos->y * STREAM_FIX_INV_SCALE;
	item->transform_index = -1;
	item->sprite = ext->sprite;
	item->texture = -1;
	memcpy(item->data, (char *)ext + sizeof(*ext), MATERIAL_DATA_SIZE);
	if (add_sr) {
		item->transform_index = srbuffer_add(m->srbuffer, pos->sr);
		if (item->transform_index < 0) {
			luaL_error(L, "sr buffer is full");
		}
	}
	if (item->sprite >= 0) {
		if (m->bank == NULL || item->sprite >= m->bank->n) {
			luaL_error(L, "Invalid material sprite %d", item->sprite);
		}
		read_sprite_rect(&m->bank->rect[item->sprite], &item->rect);
		item->texture = item->rect.texture;
	}
}

static void
set_texture_view(lua_State *L, struct material_external *m, int tex) {
	if (!m->has_texture_views || m->texture_view_slot < 0 || tex < 0) {
		return;
	}
	if (lua_getiuservalue(L, 1, 6) != LUA_TTABLE) {
		luaL_error(L, "Missing material texture views");
	}
	lua_geti(L, -1, tex + 1);
	struct view *v = (struct view *)luaL_checkudata(L, -1, "SOKOL_VIEW");
	m->bind->bindings.views[m->texture_view_slot] = v->view;
	lua_pop(L, 2);
}

static int
lmaterial_external_reset(lua_State *L) {
	struct material_external *m = (struct material_external *)luaL_checkudata(L, 1, "SOLUNA_MATERIAL_EXTERNAL");
	m->bind->base = 0;
	return 0;
}

static int
lmaterial_external_submit(lua_State *L) {
	struct material_external *m = (struct material_external *)luaL_checkudata(L, 1, "SOLUNA_MATERIAL_EXTERNAL");
	struct draw_primitive *prim = (struct draw_primitive *)lua_touserdata(L, 2);
	int prim_n = luaL_checkinteger(L, 3);
	if (prim == NULL || prim_n <= 0) {
		return 0;
	}
	void *instance = lua_newuserdatauv(L, (size_t)m->instance_size, 0);
	int i;
	for (i = 0; i < prim_n; i++) {
		struct material_item item;
		decode_item(L, m, prim, i, 1, &item);
		memset(instance, 0, (size_t)m->instance_size);
		material_error err = m->submit_one(&item, instance);
		if (err != NULL) {
			return luaL_error(L, "%s", err);
		}
		sg_append_buffer(m->inst, &(sg_range){ instance, (size_t)m->instance_size });
	}
	lua_pop(L, 1);
	return 0;
}

static void
draw_one(struct material_external *m, int ex) {
	if (ex) {
		sg_apply_bindings(&m->bind->bindings);
		sg_draw_ex(m->base_element, m->vertex_count, 1, 0, m->bind->base);
	} else {
		size_t offset = (size_t)m->bind->base * (size_t)m->instance_size;
		m->bind->bindings.vertex_buffer_offsets[0] += offset;
		sg_apply_bindings(&m->bind->bindings);
		sg_draw(m->base_element, m->vertex_count, 1);
		m->bind->bindings.vertex_buffer_offsets[0] -= offset;
	}
	m->bind->base++;
}

static int
lmaterial_external_draw_(lua_State *L, int ex) {
	struct material_external *m = (struct material_external *)luaL_checkudata(L, 1, "SOLUNA_MATERIAL_EXTERNAL");
	struct draw_primitive *prim = (struct draw_primitive *)lua_touserdata(L, 2);
	int prim_n = luaL_checkinteger(L, 3);
	int tex = luaL_optinteger(L, 4, -1);
	if (prim == NULL || prim_n <= 0) {
		return 0;
	}
	set_texture_view(L, m, tex);
	sg_apply_pipeline(m->pip);
	if (m->uniform != NULL) {
		sg_apply_uniforms(m->uniform_slot, &(sg_range){ m->uniform, m->uniform_size });
	}
	int i;
	for (i = 0; i < prim_n; i++) {
		if (m->uniform_one != NULL) {
			struct material_item item;
			decode_item(L, m, prim, i, 0, &item);
			memset(m->item_uniform, 0, m->item_uniform_size);
			material_error err = m->uniform_one(&item, m->item_uniform);
			if (err != NULL) {
				return luaL_error(L, "%s", err);
			}
			sg_apply_uniforms(m->item_uniform_slot, &(sg_range){ m->item_uniform, m->item_uniform_size });
		}
		draw_one(m, ex);
	}
	return 0;
}

static int
lmaterial_external_draw(lua_State *L) {
	return lmaterial_external_draw_(L, 0);
}

static int
lmaterial_external_draw_ex(lua_State *L) {
	return lmaterial_external_draw_(L, 1);
}

static int
lmaterial_external_reset_closure(lua_State *L) {
	lua_pushvalue(L, lua_upvalueindex(1));
	lua_insert(L, 1);
	return lmaterial_external_reset(L);
}

static int
lmaterial_external_submit_closure(lua_State *L) {
	lua_pushvalue(L, lua_upvalueindex(1));
	lua_insert(L, 1);
	return lmaterial_external_submit(L);
}

static int
lmaterial_external_draw_closure(lua_State *L) {
	lua_pushvalue(L, lua_upvalueindex(1));
	lua_insert(L, 1);
	return lmaterial_external_draw(L);
}

static int
lmaterial_external_draw_closure_ex(lua_State *L) {
	lua_pushvalue(L, lua_upvalueindex(1));
	lua_insert(L, 1);
	return lmaterial_external_draw_ex(L);
}

static void *
check_lightuserdata_field(lua_State *L, int index, const char *key) {
	if (lua_getfield(L, index, key) != LUA_TLIGHTUSERDATA) {
		luaL_error(L, "Missing .%s", key);
	}
	void *ptr = lua_touserdata(L, -1);
	lua_pop(L, 1);
	if (ptr == NULL) {
		luaL_error(L, "Invalid .%s", key);
	}
	return ptr;
}

static void *
optional_lightuserdata_field(lua_State *L, int index, const char *key) {
	void *ptr = NULL;
	if (lua_getfield(L, index, key) == LUA_TLIGHTUSERDATA) {
		ptr = lua_touserdata(L, -1);
	}
	lua_pop(L, 1);
	return ptr;
}

static int
optboolean_field(lua_State *L, int index, const char *key, int defv) {
	int r = defv;
	int t = lua_getfield(L, index, key);
	if (t == LUA_TBOOLEAN) {
		r = lua_toboolean(L, -1);
	} else if (t != LUA_TNIL) {
		luaL_error(L, "Invalid .%s", key);
	}
	lua_pop(L, 1);
	return r;
}

static int
checkinteger_field(lua_State *L, int index, const char *key) {
	if (lua_getfield(L, index, key) != LUA_TNUMBER) {
		luaL_error(L, "Missing .%s", key);
	}
	int r = lua_tointeger(L, -1);
	lua_pop(L, 1);
	return r;
}

static int
optinteger_field(lua_State *L, int index, const char *key, int defv) {
	int r = defv;
	int t = lua_getfield(L, index, key);
	if (t == LUA_TNUMBER) {
		r = lua_tointeger(L, -1);
	} else if (t != LUA_TNIL) {
		luaL_error(L, "Invalid .%s", key);
	}
	lua_pop(L, 1);
	return r;
}

static int
checktable_field(lua_State *L, int index, const char *key) {
	if (lua_getfield(L, index, key) != LUA_TTABLE) {
		luaL_error(L, "Missing .%s", key);
	}
	return lua_gettop(L);
}

static void
init_pipeline(lua_State *L, struct material_external *m, int material_index, int hooks_index) {
	union material_func shader = {
		.ptr = check_lightuserdata_field(L, hooks_index, "shader"),
	};
	union material_func pipeline = {
		.ptr = optional_lightuserdata_field(L, hooks_index, "pipeline"),
	};
	sg_pipeline_desc desc;
	memset(&desc, 0, sizeof(desc));
	if (pipeline.pipeline_desc != NULL) {
		pipeline.pipeline_desc(&desc);
	}
	const char *label = NULL;
	if (lua_getfield(L, material_index, "label") == LUA_TSTRING) {
		label = lua_tostring(L, -1);
	}
	lua_pop(L, 1);
	m->pip = util_make_pipeline(&desc, shader.shader_desc, label != NULL ? label : "external-material-pipeline", optboolean_field(L, material_index, "blend", 1));
}

static void
ref_uniform(lua_State *L, int material_index, int uv_index, const char *key, void **ptr, size_t *size) {
	if (lua_getfield(L, 1, key) == LUA_TNIL) {
		lua_pop(L, 1);
		*ptr = NULL;
		*size = 0;
		return;
	}
	luaL_checkudata(L, -1, "SOKOL_UNIFORM");
	*ptr = lua_touserdata(L, -1);
	*size = lua_rawlen(L, -1);
	lua_pushvalue(L, -1);
	lua_setiuservalue(L, material_index, uv_index);
	lua_pop(L, 1);
}

static int
lnew_material_external(lua_State *L) {
	luaL_checktype(L, 1, LUA_TTABLE);
	struct material_external *m = (struct material_external *)lua_newuserdatauv(L, sizeof(*m), 7);
	memset(m, 0, sizeof(*m));
	int material_index = lua_gettop(L);
	m->material_id = checkinteger_field(L, 1, "id");
	if (m->material_id <= 0) {
		return luaL_error(L, "Invalid material id %d", m->material_id);
	}
	m->instance_size = checkinteger_field(L, 1, "instance_size");
	if (m->instance_size <= 0) {
		return luaL_error(L, "Invalid instance size %d", m->instance_size);
	}
	m->vertex_count = optinteger_field(L, 1, "vertex_count", 4);
	m->base_element = optinteger_field(L, 1, "base_element", 0);
	m->uniform_slot = optinteger_field(L, 1, "uniform_slot", 0);
	m->item_uniform_slot = optinteger_field(L, 1, "item_uniform_slot", 1);
	m->texture_view_slot = optinteger_field(L, 1, "texture_view_slot", -1);
	int hooks_index = checktable_field(L, 1, "hooks");
	union material_func submit = {
		.ptr = check_lightuserdata_field(L, hooks_index, "submit"),
	};
	m->submit_one = submit.submit_one;
	union material_func uniform = {
		.ptr = optional_lightuserdata_field(L, hooks_index, "uniform"),
	};
	m->uniform_one = uniform.uniform_one;
	init_pipeline(L, m, 1, hooks_index);
	lua_pop(L, 1);
	util_ref_object(L, &m->inst, 1, "inst_buffer", "SOKOL_BUFFER", 0);
	util_ref_object(L, &m->bind, 2, "bindings", "SOKOL_BINDINGS", 1);
	util_ref_object(L, &m->srbuffer, 3, "sr_buffer", "SOLUNA_SRBUFFER", 1);
	ref_uniform(L, material_index, 4, "uniform", &m->uniform, &m->uniform_size);
	ref_uniform(L, material_index, 5, "item_uniform", &m->item_uniform, &m->item_uniform_size);
	if (m->uniform_one != NULL && m->item_uniform == NULL) {
		return luaL_error(L, "Missing .item_uniform");
	}
	if (lua_getfield(L, 1, "sprite_bank") == LUA_TLIGHTUSERDATA) {
		m->bank = (struct sprite_bank *)lua_touserdata(L, -1);
	}
	lua_pop(L, 1);
	if (lua_getfield(L, 1, "texture_views") == LUA_TTABLE) {
		m->has_texture_views = 1;
		lua_setiuservalue(L, material_index, 6);
	} else {
		lua_pop(L, 1);
	}
	luaL_newmetatable(L, "SOLUNA_MATERIAL_EXTERNAL");
	lua_setmetatable(L, -2);
	lua_newtable(L);
	int result_index = lua_gettop(L);
	lua_pushvalue(L, material_index);
	lua_pushcclosure(L, lmaterial_external_reset_closure, 1);
	lua_setfield(L, result_index, "reset");
	lua_pushvalue(L, material_index);
	lua_pushcclosure(L, lmaterial_external_submit_closure, 1);
	lua_setfield(L, result_index, "submit");
	lua_pushvalue(L, material_index);
	lua_pushcclosure(L, DRAWFUNC(lmaterial_external_draw_closure), 1);
	lua_setfield(L, result_index, "draw");
	return 1;
}

int
luaopen_material_external(lua_State *L) {
	luaL_checkversion(L);
	luaL_Reg l[] = {
		{ "new", lnew_material_external },
		{ NULL, NULL },
	};
	luaL_newlib(L, l);
	return 1;
}
