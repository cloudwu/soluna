#include <lua.h>
#include <lauxlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "sokol/sokol_gfx.h"
#include "sokol/sokol_glue.h"
#include "sokol/sokol_app.h"
#include "texquad.glsl.h"
#include "srbuffer.h"
#include "sprite_submit.h"
#include "batch.h"
#include "spritemgr.h"
#include "render_blit.h"

#define UNIFORM_MAX 4
#define BINDINGNAME_MAX 32

struct buffer {
	sg_buffer handle;
	struct sg_buffer_usage usage;
};

struct image {
	sg_image img;
	int size;
};

struct sampler {
	sg_sampler handle;
};

struct offscreen_state {
	bool enabled;
	int width;
	int height;
	sg_image image;
	sg_view color_view;
	sg_view sample_view;
	sg_sampler sampler;
	sg_pass pass;
};

static struct offscreen_state OFFSCREEN;

static void
offscreen_destroy(void) {
	if (OFFSCREEN.color_view.id) {
		sg_destroy_view(OFFSCREEN.color_view);
	}
	if (OFFSCREEN.sample_view.id) {
		sg_destroy_view(OFFSCREEN.sample_view);
	}
	if (OFFSCREEN.sampler.id) {
		sg_destroy_sampler(OFFSCREEN.sampler);
	}
	if (OFFSCREEN.image.id) {
		sg_destroy_image(OFFSCREEN.image);
	}
	OFFSCREEN = (struct offscreen_state){0};
}

static void
offscreen_apply_viewport(int canvas_width, int canvas_height) {
	if (canvas_width <= 0 || canvas_height <= 0 || OFFSCREEN.width <= 0 || OFFSCREEN.height <= 0) {
		sg_apply_viewport(0, 0, canvas_width, canvas_height, true);
		return;
	}
	const float canvas_aspect = (float)canvas_width / (float)canvas_height;
	const float content_aspect = (float)OFFSCREEN.width / (float)OFFSCREEN.height;
	int vp_w = canvas_width;
	int vp_h = canvas_height;
	int vp_x = 0;
	int vp_y = 0;
	if (content_aspect > canvas_aspect) {
		vp_w = canvas_width;
		vp_h = (int)(vp_w / content_aspect);
		vp_y = (canvas_height - vp_h) / 2;
	} else if (content_aspect < canvas_aspect) {
		vp_h = canvas_height;
		vp_w = (int)(vp_h * content_aspect);
		vp_x = (canvas_width - vp_w) / 2;
	}
	sg_apply_viewport(vp_x, vp_y, vp_w, vp_h, true);
}

static struct sg_buffer_usage
get_buffer_type(lua_State *L, int index) {
	if (lua_getfield(L, index, "type") != LUA_TSTRING) {
		luaL_error(L, "Need .type");
	}
	const char * str = lua_tostring(L, -1);
	struct sg_buffer_usage usage = { 0 };
	if (strcmp(str, "vertex") == 0) {
		usage.vertex_buffer = true;
	} else if (strcmp(str, "index") == 0) {
		usage.index_buffer = true;
	} else if (strcmp(str, "storage") == 0) {
	    usage.storage_buffer = true;
	} else {
		luaL_error(L, "Invalid buffer .type = %s", str);
	}
	lua_pop(L, 1);
	return usage;
}

static void
get_buffer_usage(lua_State *L, int index, struct sg_buffer_usage *usage) {
	if (lua_getfield(L, index, "usage") != LUA_TSTRING) {
		if (lua_isnil(L, -1)) {
			lua_pop(L, 1);
			usage->immutable = true;
			return;
		}
		luaL_error(L, "Invalid .usage");
	}
	const char * str = lua_tostring(L, -1);
	if (strcmp(str, "stream") == 0) {
		usage->stream_update = true;
	} else if (strcmp(str, "dynamic") == 0) {
		usage->dynamic_update = true;
	} else if (strcmp(str, "immutable") == 0) {
		usage->immutable = true;
	} else {
		luaL_error(L, "Invalid buffer .usage = %s", str);
	}
	lua_pop(L, 1);
}

static const void *
get_buffer_data(lua_State *L, int index, size_t *sz) {
	int t = lua_getfield(L, index, "data");
	if (t == LUA_TNIL) {
		// no ptr
		lua_pop(L, 1);
		if (lua_getfield(L, index, "size") != LUA_TNUMBER) {
			luaL_error(L, "No .data and .size");
		}
		*sz = luaL_checkinteger(L, -1);
		lua_pop(L, 1);
		return NULL;
	}
	size_t size = 0;
	if (lua_getfield(L, index, "size") == LUA_TNUMBER) {
		size = luaL_checkinteger(L, -1);
	}
	lua_pop(L, 1);
	if (t == LUA_TLIGHTUSERDATA) {
		if (size == 0) {
			luaL_error(L, "lightuserdata for .data without .size");
		}
		*sz = size;
		const void * ptr = lua_touserdata(L, -1);
		lua_pop(L, 1);
		return ptr;
	}
	else if (t == LUA_TUSERDATA) {
		size_t rawlen = lua_rawlen(L, -1);
		if (size > 0 && size != rawlen)
			luaL_error(L, "size of userdata %d != %d", rawlen, size);
		const void * ptr = lua_touserdata(L, -1);
		lua_pop(L, 1);
		*sz = size;
		return ptr;
	} else if (t == LUA_TSTRING) {
		size_t rawlen;
		const void * ptr = (const void *)lua_tolstring(L, -1, &rawlen);
		if (size > 0 && size != rawlen)
			luaL_error(L, "size of string %d != %d", rawlen, size);
		lua_pop(L, 1);
		*sz = rawlen;
		return ptr;
	}
	luaL_error(L, "Invalid .data type = %s", lua_typename(L, t));
	*sz = 0;
	return NULL;
}

static int
lbuffer_update(lua_State *L) {
	if (lua_gettop(L) == 1)
		return 0;
	struct buffer *p = (struct buffer *)luaL_checkudata(L, 1, "SOKOL_BUFFER");
	size_t sz;
	const void *ptr;
	switch (lua_type(L, 2)) {
	case LUA_TSTRING:
		ptr = (const void *)lua_tolstring(L, 2, &sz);
		break;
	case LUA_TUSERDATA:
		ptr = (const void *)lua_touserdata(L, 2);
		sz = lua_rawlen(L, 2);
		if (lua_isinteger(L, 3)) {
			int usersize = lua_tointeger(L, 3);
			if (usersize > sz)
				return luaL_error(L, "Invalid size %d > %d", usersize, sz);
			sz = usersize;
		}
		break;
	case LUA_TLIGHTUSERDATA:
		ptr = (const void *)lua_touserdata(L, 2);
		sz = luaL_checkinteger(L, 3);
		break;
	default:
		return luaL_error(L, "Invalid data type %s", lua_typename(L, lua_type(L, 2)));
	}
	sg_update_buffer(p->handle, &(sg_range) { ptr, sz });
	return 0;
}

static int
lbuffer_ref(lua_State *L) {
	struct buffer *p = (struct buffer *)lua_touserdata(L, 1);
	luaL_checktype(L, 2, LUA_TLIGHTUSERDATA);
	sg_buffer *ref = (sg_buffer *)lua_touserdata(L, 2);
	*ref = p->handle;
	return 0;
}

static int
lbuffer_tostring(lua_State *L) {
	struct buffer *p = (struct buffer *)lua_touserdata(L, 1);
	const char * name = "Invalid";
	if (p->usage.vertex_buffer) {
		name = "VB";
	} else if (p->usage.index_buffer) {
		name = "IB";
	} else if (p->usage.storage_buffer) {
		name = "SB";
	}
	lua_pushfstring(L, "[%s:%d]", name, p->handle.id);
	return 1;
}

static int
lbuffer(lua_State *L) {
	luaL_checktype(L, 1, LUA_TTABLE);
	struct buffer * p = (struct buffer *)lua_newuserdatauv(L, sizeof(*p), 0);
	p->usage = get_buffer_type(L, 1);
	get_buffer_usage(L, 1, &p->usage);
	size_t sz;
	const void *ptr = get_buffer_data(L, 1, &sz);
	if (p->usage.immutable && ptr == NULL) {
		return luaL_error(L, "immutable buffer needs init data");
	}
	const char *label = NULL;
	if (lua_getfield(L, 1, "label") == LUA_TSTRING) {
		label = lua_tostring(L, -1);
	}
	lua_pop(L, 1);
	p->handle = sg_make_buffer(&(sg_buffer_desc) {
		.size = sz,
		.usage = p->usage,
		.label = label,
	    .data.ptr = ptr,
		.data.size = ptr == NULL ? 0 : sz,
	});
		
	if (luaL_newmetatable(L, "SOKOL_BUFFER")) {
		luaL_Reg l[] = {
			{ "__index", NULL },
			{ "__call", lbuffer_ref },
			{ "__tostring", lbuffer_tostring },
			{ "update", lbuffer_update },
			{ NULL, NULL },
		};
		luaL_setfuncs(L, l, 0);

		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");
	}
	lua_setmetatable(L, -2);

	return 1;
}

struct pass {
	sg_pass_action pass_action;
};

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

static int
lpass_begin(lua_State *L) {
	struct pass * p = (struct pass *)luaL_checkudata(L, 1, "SOKOL_PASS");
	if (OFFSCREEN.enabled) {
		OFFSCREEN.pass.action = p->pass_action;
		sg_begin_pass(&OFFSCREEN.pass);
	} else {
		sg_begin_pass(&(sg_pass){ .action = p->pass_action, .swapchain = sglue_swapchain() });
	}
	return 0;
}

static int
lpass_end(lua_State *L) {
	if (OFFSCREEN.enabled) {
		sg_end_pass();
		int canvas_width = sapp_width();
		int canvas_height = sapp_height();
		sg_pass display_pass = {
			.action = OFFSCREEN.pass.action,
			.swapchain = sglue_swapchain(),
		};
		sg_begin_pass(&display_pass);
		offscreen_apply_viewport(canvas_width, canvas_height);
		soluna_render_blit(OFFSCREEN.sample_view, OFFSCREEN.sampler);
		sg_end_pass();
	} else {
		sg_end_pass();
	}
	return 0;
}

static int
lpass_new(lua_State *L) {
	struct pass * p = lua_newuserdatauv(L, sizeof(*p), 0);
	memset(p, 0, sizeof(*p));
	luaL_checktype(L, 1, LUA_TTABLE);
	sg_pass_action *action = &p->pass_action;
	// todo : store action
	
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
	if (luaL_newmetatable(L, "SOKOL_PASS")) {
		luaL_Reg l[] = {
			{ "__index", NULL },
			{ "begin", lpass_begin },
			{ "finish", lpass_end },	// end is a reserved keyword in lua, use finish instead
			{ NULL, NULL },
		};
		luaL_setfuncs(L, l, 0);
		
		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");
	}
	lua_setmetatable(L, -2);
	return 1;
}

static bool
offscreen_create(int width, int height, sg_filter filter) {
	offscreen_destroy();
	if (width <= 0 || height <= 0) {
		return false;
	}
	OFFSCREEN.image = sg_make_image(&(sg_image_desc){
		.width = width,
		.height = height,
		.pixel_format = SG_PIXELFORMAT_RGBA8,
		.usage.color_attachment = true,
		.label = "soluna-offscreen-image",
	});
	if (OFFSCREEN.image.id == SG_INVALID_ID) {
		offscreen_destroy();
		return false;
	}
	OFFSCREEN.color_view = sg_make_view(&(sg_view_desc){
		.color_attachment.image = OFFSCREEN.image,
	});
	if (OFFSCREEN.color_view.id == SG_INVALID_ID) {
		offscreen_destroy();
		return false;
	}
	OFFSCREEN.sample_view = sg_make_view(&(sg_view_desc){
		.texture.image = OFFSCREEN.image,
	});
	if (OFFSCREEN.sample_view.id == SG_INVALID_ID) {
		offscreen_destroy();
		return false;
	}
	OFFSCREEN.sampler = sg_make_sampler(&(sg_sampler_desc){
		.min_filter = filter,
		.mag_filter = filter,
		.wrap_u = SG_WRAP_CLAMP_TO_EDGE,
		.wrap_v = SG_WRAP_CLAMP_TO_EDGE,
		.label = "soluna-offscreen-sampler",
	});
	if (OFFSCREEN.sampler.id == SG_INVALID_ID) {
		offscreen_destroy();
		return false;
	}
	OFFSCREEN.pass = (sg_pass){
		.attachments = {
			.colors[0] = OFFSCREEN.color_view,
		},
	};
	OFFSCREEN.width = width;
	OFFSCREEN.height = height;
	OFFSCREEN.enabled = true;
	return true;
}

static int
lsubmit(lua_State *L) {
	sg_commit();
	return 0;
}

static int
limage_update(lua_State *L) {
	struct image *p = (struct image *)luaL_checkudata(L, 1, "SOKOL_IMAGE");
	// todo: support subimage
	void *buffer = lua_touserdata(L, 2);
	if (buffer == NULL)
		return luaL_error(L, "Need data");
	sg_image_data data = {
		.mip_levels[0].ptr = buffer,
		.mip_levels[0].size = p->size,
	};
	sg_update_image(p->img, &data);
	return 0;
}

static int
get_pixel_format(lua_State *L, const char * type, int *pixel_size) {
	if (strcmp(type, "RGBA8") == 0) {
		*pixel_size = 4;
		return SG_PIXELFORMAT_RGBA8;
	} else if (strcmp(type, "R8") == 0) {
		*pixel_size = 1;
		return SG_PIXELFORMAT_R8;
	}
	return luaL_error(L, "Invalid pixel format %s", type);
}

static int
limage_ref(lua_State *L) {
	struct image *p = (struct image *)lua_touserdata(L, 1);
	luaL_checktype(L, 2, LUA_TLIGHTUSERDATA);
	sg_image *ref = (sg_image *)lua_touserdata(L, 2);
	*ref = p->img;
	return 0;
}

static int
limage(lua_State *L) {
	luaL_checktype(L, 1, LUA_TTABLE);
	sg_image_desc img = { .usage.dynamic_update = true };
	if (lua_getfield(L, 1, "width") != LUA_TNUMBER) {
		return luaL_error(L, "Need .width");
	}
	img.width = luaL_checkinteger(L, -1);
	lua_pop(L, 1);
	if (lua_getfield(L, 1, "height") != LUA_TNUMBER) {
		return luaL_error(L, "Need .height");
	}
	img.height = luaL_checkinteger(L, -1);
	lua_pop(L, 1);
	if (lua_getfield(L, 1, "label") == LUA_TSTRING) {
		img.label = lua_tostring(L, -1);
	}
	lua_pop(L, 1);
	int pixel_size = 4;
	if (lua_getfield(L, 1, "pixel_format") == LUA_TSTRING) {
		img.pixel_format = get_pixel_format(L, lua_tostring(L, -1), &pixel_size);
	} else {
		img.pixel_format = SG_PIXELFORMAT_RGBA8;
		pixel_size = 4;
	}
	lua_pop(L, 1);
	// todo: type, render_target, num_slices, num_mipmaps, pixel_format, etc
	struct image * p = (struct image *)lua_newuserdatauv(L, sizeof(*p), 0);
	memset(p, 0, sizeof(*p));
	if (luaL_newmetatable(L, "SOKOL_IMAGE")) {
		luaL_Reg l[] = {
			{ "__index", NULL },
			{ "__call", limage_ref },
			{ "update", limage_update },
			{ NULL, NULL },
		};
		luaL_setfuncs(L, l, 0);

		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");
	}
	lua_setmetatable(L, -2);
	p->img = sg_make_image(&img);
	p->size = img.width * img.height * pixel_size;
	return 1;
}

static int
lsampler_ref(lua_State *L) {
	struct sampler *p = (struct sampler *)lua_touserdata(L, 1);
	luaL_checktype(L, 2, LUA_TLIGHTUSERDATA);
	sg_sampler *ref = (sg_sampler *)lua_touserdata(L, 2);
	*ref = p->handle;
	return 0;
}

static int
lsampler(lua_State *L) {
	luaL_checktype(L, 1, LUA_TTABLE);
	struct sampler * s = (struct sampler *)lua_newuserdatauv(L, sizeof(*s), 0);
	struct sg_sampler_desc desc = { 0 };
	if (lua_getfield(L, 1, "label") == LUA_TSTRING) {
		desc.label = lua_tostring(L, -1);
	}
	lua_pop(L, 1);
	// todo : set filter , etc
	s->handle = sg_make_sampler(&desc);
	
	if (luaL_newmetatable(L, "SOKOL_SAMPLER")) {
		lua_pushcfunction(L, lsampler_ref ),
		lua_setfield(L, -2, "__call");
	}
	lua_setmetatable(L, -2);
	
	return 1;
}

static int
ldraw(lua_State *L) {
	int base = luaL_checkinteger(L, 1);
	int n = luaL_checkinteger(L, 2);
	int inst = luaL_checkinteger(L, 3);
	sg_draw(base, n, inst);
	return 0;
}

static int
lsrbuffer_add(lua_State *L) {
	struct sr_buffer *b = (struct sr_buffer *)luaL_checkudata(L, 1, "SOLUNA_SRBUFFER");
	float scale = luaL_checknumber(L, 2);
	float rot = luaL_checknumber(L, 3);

	struct draw_primitive tmp;
	sprite_set_sr(&tmp, scale, rot);
	int index = srbuffer_add(b, tmp.sr);
	if (index < 0)
		return 0;
	lua_pushinteger(L, index);
	return 1;
}

static int
lsrbuffer_ptr(lua_State *L) {
	struct sr_buffer *b = (struct sr_buffer *)luaL_checkudata(L, 1, "SOLUNA_SRBUFFER");
	int sz;
	void * ptr = srbuffer_commit(b, &sz);
	if (ptr == NULL)
		return 0;
	lua_pushlightuserdata(L, ptr);
	lua_pushinteger(L, sz);
	return 2;
}

static int
lsrbuffer(lua_State *L) {
	int n = luaL_checkinteger(L, 1);
	size_t sz = srbuffer_size(n);
	struct sr_buffer *b = (struct sr_buffer *)lua_newuserdatauv(L, sz, 0);
	srbuffer_init(b, n);
	if (luaL_newmetatable(L, "SOLUNA_SRBUFFER")) {
		luaL_Reg l[] = {
			{ "__index", NULL },
			{ "add", lsrbuffer_add },
			{ "ptr", lsrbuffer_ptr },
			{ NULL, NULL },
		};
		luaL_setfuncs(L, l, 0);

		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");
	}
	lua_setmetatable(L, -2);
	return 1;
}

static sg_filter
filter_from_string(const char *str) {
	if (str == NULL) {
		return SG_FILTER_LINEAR;
	}
	if (strcmp(str, "nearest") == 0) {
		return SG_FILTER_NEAREST;
	}
	return SG_FILTER_LINEAR;
}

static int
loffscreen_setup(lua_State *L) {
	if (lua_isnoneornil(L, 1)) {
		offscreen_destroy();
		return 0;
	}
	int width = luaL_checkinteger(L, 1);
	int height = luaL_optinteger(L, 2, 0);
	if (width <= 0 || height <= 0) {
		offscreen_destroy();
		return 0;
	}
	const char *filter_str = luaL_optstring(L, 3, "linear");
	sg_filter filter = filter_from_string(filter_str);
	if (!offscreen_create(width, height, filter)) {
		return luaL_error(L, "failed to setup offscreen render target");
	}
	return 0;
}

struct inst_object {
	float x, y;
	float sr_index;
};

struct sprite_object {
	uint32_t off;
	uint32_t u;
	uint32_t v;
};

static int
lbuffer_size(lua_State *L) {
	const char * name = luaL_checkstring(L, 1);
	int n = luaL_optinteger(L, 2, 1);
	size_t sz = 0;
	if (strcmp(name, "srbuffer") == 0) {
		sz = sizeof(struct sr_mat);
	} else if (strcmp(name, "inst") == 0) {
		sz = sizeof(struct inst_object);
	} else if (strcmp(name, "sprite") == 0) {
		sz = sizeof(struct sprite_object);
	} else {
		return luaL_error(L, "Invalid buffer type %s", name);
	}
	lua_pushinteger(L, sz * n);
	return 1;
}

static int
ltmp_buffer(lua_State *L) {
	size_t sz = luaL_optinteger(L, 1, 128 * 1024);
	lua_newuserdatauv(L, sz, 0);
	return 1;
}

int lbindings_new(lua_State *L);
int lview_new(lua_State *L);
int luniform_new(lua_State *L);

int
luaopen_render(lua_State *L) {
	luaL_checkversion(L);
	luaL_Reg l[] = {
		{ "offscreen_setup", loffscreen_setup },
		{ "pass", lpass_new },
		{ "submit", lsubmit },
		{ "image", limage },
		{ "buffer", lbuffer },
		{ "sampler", lsampler },
		{ "draw", ldraw },
		{ "srbuffer", lsrbuffer },
		{ "buffer_size", lbuffer_size },
		{ "bindings", lbindings_new },
		{ "view", lview_new },
		{ "uniform", luniform_new },
		{ "tmp_buffer", ltmp_buffer },
		{ NULL, NULL },
	};
	luaL_newlib(L, l);
	return 1;
}
