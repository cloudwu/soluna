#include <lua.h>
#include <lauxlib.h>

#include "sokol/sokol_gfx.h"
#include "blit.glsl.h"
#include "render_bindings.h"
#include "material_util.h"

struct material_blit {
	sg_pipeline pip;
	struct soluna_render_bindings *bind;
};

static int
lmaterial_blit_draw(lua_State *L) {
	struct material_blit *m = (struct material_blit *)luaL_checkudata(L, 1, "SOLUNA_MATERIAL_BLIT");
	sg_apply_pipeline(m->pip);
	sg_apply_bindings(&m->bind->bindings);
	sg_draw(0, 4, 1);
	return 0;
}

static void
init_pipeline(struct material_blit *p) {
	sg_pipeline_desc desc = {
		.layout.buffers[0].step_func = SG_VERTEXSTEP_PER_VERTEX,
	};
	p->pip = util_make_pipeline(&desc, blit_shader_desc, "blit-pipeline", 0);
}

static int
lnew_material_blit(lua_State *L) {
	luaL_checktype(L, 1, LUA_TTABLE);
	struct material_blit *m = (struct material_blit *)lua_newuserdatauv(L, sizeof(*m), 1);
	init_pipeline(m);
	util_ref_object(L, &m->bind, 1, "bindings", "SOKOL_BINDINGS", 1);
	
	if (luaL_newmetatable(L, "SOLUNA_MATERIAL_BLIT")) {
		luaL_Reg l[] = {
			{ "__index", NULL },
			{ "draw", lmaterial_blit_draw },
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
luaopen_material_blit(lua_State *L) {
	luaL_checkversion(L);
	
	luaL_Reg l[] = {
		{ "new", lnew_material_blit },
		{ NULL, NULL },
	};
	luaL_newlib(L, l);

	return 1;
}
