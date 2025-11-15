#include <lua.h>
#include <lauxlib.h>

#include "material_util.h"

void
util_ref_object(lua_State *L, void *ptr, int uv_index, const char *key, const char *luatype, int direct) {
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

void
util_submit_material(lua_State *L, int batch_n, void *mat, util_submit_func submit) {
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

sg_pipeline
util_make_pipeline(sg_pipeline_desc *desc, util_shader_desc_func func, const char *what, int blend) {
	sg_shader shd = sg_make_shader(func(sg_query_backend()));
	if (sg_query_shader_state(shd) != SG_RESOURCESTATE_VALID) {
		fprintf(stderr, "Failed to create shader for %s!\n", what);
	}
	desc->shader = shd;
	if (desc->primitive_type == 0) {
		desc->primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP;
	}
	desc->label = what;
	if (desc->layout.buffers[0].step_func == 0) {
		desc->layout.buffers[0].step_func = SG_VERTEXSTEP_PER_INSTANCE;
	}
	if (blend) {
		desc->colors[0].blend = (sg_blend_state) {
			.enabled = true,
			.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
			.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
			.src_factor_alpha = SG_BLENDFACTOR_ONE,
			.dst_factor_alpha = SG_BLENDFACTOR_ZERO
		};
	}
	sg_pipeline pip = sg_make_pipeline(desc);
	if (sg_query_pipeline_state(pip) != SG_RESOURCESTATE_VALID) {
		fprintf(stderr, "failed to create pipeline %s\n", what);
	}
	return pip;
}
