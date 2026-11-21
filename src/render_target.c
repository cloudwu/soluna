#include <lua.h>
#include <lauxlib.h>
#include <math.h>

#include "sokol/sokol_gfx.h"
#include "sokol/sokol_app.h"
#include "sokol/sokol_glue.h"

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
		} else if (strcmp(key, "clear") == 0) {
			action->colors[idx].load_action = SG_LOADACTION_CLEAR;
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
		float a = action->colors[idx].clear_value.a;
		action->colors[idx].clear_value.r = ((c & 0xff0000) >> 16) / 255.0f * a;
		action->colors[idx].clear_value.g = ((c & 0x00ff00) >> 8) / 255.0f * a;
		action->colors[idx].clear_value.b = ((c & 0x0000ff)) / 255.0f * a;
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

void
render_target_size(struct render_target *p, int width, int height, int flip) {
	p->width = width;
	p->height = height;
	p->inv_width = width ? 1.0f / width : 0;
	p->inv_height = height ? 1.0f / height : 0;
	p->framesize[0] = 2 * p->inv_width;
	p->framesize[1] = flip ? 2 * p->inv_height : -2 * p->inv_height;
	p->framesize[2] = -1.0f;
	p->framesize[3] = flip ? -1.0f : 1.0f;
}

void
render_target_swapchain(struct render_target *p) {
	p->pass.swapchain = sglue_swapchain();
	float dpi_scale = sapp_dpi_scale();
	float safe_scale = dpi_scale > 0.0f ? dpi_scale : 1.0f;
	int logical_w = (int)((float)p->pass.swapchain.width / safe_scale + 0.5f);
	int logical_h = (int)((float)p->pass.swapchain.height / safe_scale + 0.5f);
	p->dpi_scale = safe_scale;
	render_target_size(p, logical_w, logical_h, 0);
}

void
render_target_scissor(struct render_target *rt) {
	int fb_w = rt->swapchain ? rt->pass.swapchain.width : rt->width;
	int fb_h = rt->swapchain ? rt->pass.swapchain.height : rt->height;
	struct render_clip r = { 0, 0, fb_w, fb_h };
	if (rt->clip_n > 0) {
		r = rt->clip[rt->clip_n - 1];
		float scale = rt->dpi_scale;
		if (scale > 0.0f && scale != 1.0f) {
			if (r.w <= 0 || r.h <= 0) {
				r = (struct render_clip){ 0, 0, 0, 0 };
			} else {
				int x0 = (int)floorf((float)r.x * scale);
				int y0 = (int)floorf((float)r.y * scale);
				int x1 = (int)ceilf((float)(r.x + r.w) * scale);
				int y1 = (int)ceilf((float)(r.y + r.h) * scale);
				if (x0 < 0)
					x0 = 0;
				if (y0 < 0)
					y0 = 0;
				if (x1 > fb_w)
					x1 = fb_w;
				if (y1 > fb_h)
					y1 = fb_h;
				r.x = x0;
				r.y = y0;
				r.w = x1 > x0 ? x1 - x0 : 0;
				r.h = y1 > y0 ? y1 - y0 : 0;
			}
		}
	}
	sg_apply_scissor_rect(r.x, r.y, r.w, r.h, rt->framesize[1] < 0);
}

int
lpass_new(lua_State *L) {
	struct render_target * p = lua_newuserdatauv(L, sizeof(*p), 1);
	memset(p, 0, sizeof(*p));
	luaL_checktype(L, 1, LUA_TTABLE);
	if (lua_getfield(L, 1, "swapchain") == LUA_TBOOLEAN && lua_toboolean(L, -1)) {
		p->swapchain = true;
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
		if (p->swapchain) {
			return luaL_error(L, "swapchain not allows with attachment");
		}
		read_attachments(L, &p->pass.attachments);
		lua_setiuservalue(L, 2, 1);
		lua_getfield(L, 1, "width");
		int w = luaL_checkinteger(L, -1);
		lua_pop(L, 1);
		lua_getfield(L, 1, "height");
		int h = luaL_checkinteger(L, -1);
		p->dpi_scale = 1.0f;
		render_target_size(p, w, h, !sg_query_features().origin_top_left);
	} else if (!p->swapchain) {
		return luaL_error(L, "missing swapchain");
	}
	lua_pop(L, 1);
	return 1;
}
