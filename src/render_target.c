#include <lua.h>
#include <lauxlib.h>

#include "sokol/sokol_gfx.h"
#include "sokol/sokol_glue.h"
#include "sokol/sokol_app.h"

#include "render_target.h"
#include <string.h>

static int
read_color_action(lua_State *L, int index, sg_pass_action *action, int idx) {
	char key[] = { 'c', 'o', 'l', 'o' , 'r' , '0' + idx, '\0' };
	int t = lua_getfield(L, index, key);
	if (t == LUA_TNIL) {
		lua_pop(L, 1);
		return 0;
	}
	if (idx >= SG_MAX_COLOR_ATTACHMENTS)
		return luaL_error(L, "Too many color attachments %d >= %d", idx , SG_MAX_COLOR_ATTACHMENTS);
	if ( t == LUA_TSTRING ) {
		const char * key = lua_tostring(L, -1);
		if (strcmp(key, "load") == 0) {
			action->colors[idx].load_action = SG_LOADACTION_LOAD;
		} else if (strcmp(key, "dontcare") == 0 ) {
			action->colors[idx].load_action = SG_LOADACTION_DONTCARE;
		} else {
			return luaL_error(L, "Invalid load action (%d) = %s", idx, key);
		}
	} else {
		uint32_t c = luaL_checkinteger(L, -1);
		if (c <= 0xffffff) {
			action->colors[idx].clear_value.a = 1.0f;
		} else {
			action->colors[idx].clear_value.a = ((c & 0xff000000) >> 24) / 255.0f;
		}
		action->colors[idx].clear_value.r = ((c & 0xff0000) >> 16) / 255.0f;
		action->colors[idx].clear_value.g = ((c & 0x00ff00) >> 8) / 255.0f;
		action->colors[idx].clear_value.b = ((c & 0x0000ff)) / 255.0f;
		action->colors[idx].load_action = SG_LOADACTION_CLEAR;
	}
	lua_pop(L, 1);
	return 1;
}

static void
read_attachments(lua_State *L, sg_attachments *attachments) {
	luaL_checktype(L, -1, LUA_TTABLE);
	int i;
	for (i = 0; i < SG_MAX_COLOR_ATTACHMENTS; i++) {
		char key[] = { 'c', 'o', 'l', 'o' , 'r' , '0' + i, '\0' };
		if (lua_getfield(L, -1, key) == LUA_TNIL) {
			lua_pop(L, 1);
			break;
		}
		luaL_checkudata(L, -1, "SOKOL_VIEW");
		lua_call(L, 0, 1);
		sg_view *color_v = (sg_view *)lua_touserdata(L, -1);
		if (color_v == NULL) {
			luaL_error(L, "Invalid %s view", key);
		}
		attachments->colors[i] = *color_v;
		lua_pop(L, 1);
	}

	if (lua_getfield(L, -1, "depth_stencil") != LUA_TNIL) {
		luaL_checkudata(L, -1, "SOKOL_VIEW");
		lua_call(L, 0, 1);
		sg_view *ds_v = (sg_view *)lua_touserdata(L, -1);
		if (ds_v == NULL) {
			luaL_error(L, "Invalid depth_stencil view");
		}
		attachments->depth_stencil = *ds_v;
	}
	lua_pop(L, 1);
}

int
lpass_begin(lua_State *L) {
	struct render_target * p = (struct render_target *)lua_touserdata(L, 1);
	if (p->pass.swapchain.invalid) {
		p->pass.swapchain = sglue_swapchain();
		sg_begin_pass(&p->pass);
		p->pass.swapchain.invalid = true;
		p->width = p->pass.swapchain.width;
		p->height = p->pass.swapchain.height;
		if (p->width) {
			p->inv_width = 1.0f / p->width;
		} else {
			p->inv_width = 0;
		}
		if (p->height) {
			p->inv_height = 1.0f / p->height;
		} else {
			p->inv_height = 0;
		}
	} else {
		sg_begin_pass(&p->pass);
	}
	return 0;
}

int
lpass_end(lua_State *L) {
	sg_end_pass();
	return 0;
}

int
lpass_new(lua_State *L) {
	struct render_target * p = lua_newuserdatauv(L, sizeof(*p), 0);
	memset(p, 0, sizeof(*p));
	luaL_checktype(L, 1, LUA_TTABLE);
	if (lua_getfield(L, 1, "swapchain") == LUA_TBOOLEAN && lua_toboolean(L, -1)) {
		p->pass.swapchain.invalid = true;
	}
	lua_pop(L, 1);

	sg_pass_action *action = &p->pass.action;
	
	int i = 0;
	while (read_color_action(L, 1, action, i)) {
		++i;
	}
	if (lua_getfield(L, 1, "depth") == LUA_TNIL) {
		action->depth.load_action = SG_LOADACTION_DONTCARE;
	} else {
		float depth = luaL_checknumber(L, -1);
		action->depth.load_action = SG_LOADACTION_CLEAR;
		action->depth.clear_value = depth;
	}
	lua_pop(L, 1);
	if (lua_getfield(L, 1, "stencil") == LUA_TNIL) {
		action->depth.load_action = SG_LOADACTION_DONTCARE;
	} else {
		int s = luaL_checkinteger(L, -1);
		if (s < 0 || s > 255)
			return luaL_error(L, "Invalid stencil %d", s);
		action->depth.load_action = SG_LOADACTION_CLEAR;
		action->depth.clear_value = s;
	}
	lua_pop(L, 1);
	if (lua_getfield(L, 1, "attachment") != LUA_TNIL) {
		if (p->pass.swapchain.invalid) {
			return luaL_error(L, "swapchain not allows with attachment");
		}
		read_attachments(L, &p->pass.attachments);
	} else if (!p->pass.swapchain.invalid) {
		return luaL_error(L, "missing swapchain");
	}
	lua_pop(L, 1);
	return 1;
}
