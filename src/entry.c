#define SOKOL_IMPL

#include <lua.h>
#include <lauxlib.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <locale.h>

#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__)

#define SOKOL_D3D11

#elif defined(__APPLE__)

#define SOKOL_METAL

#elif defined(__EMSCRIPTEN__)

#define SOKOL_WGPU

#elif defined(__linux__)

#define SOKOL_GLCORE

#else

#error Unsupport platform

#endif

#include "version.h"

#define FRAME_CALLBACK 1
#define CLEANUP_CALLBACK 2
#define EVENT_CALLBACK 3
#define CALLBACK_COUNT 3

#include "sokol/sokol_app.h"
#include "sokol/sokol_gfx.h"
#include "sokol/sokol_glue.h"
#include "sokol/sokol_log.h"
#include "sokol/sokol_args.h"
#include "loginfo.h"
#include "appevent.h"
#include "ime_state.h"

#if defined(__APPLE__)
#include "platform/macos/soluna_macos_ime.h"
#endif
#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__)
#include "platform/windows/soluna_windows_ime.h"
#endif
#if defined(__linux__)
#include "platform/linux/soluna_linux_ime.h"
#endif
#if defined(__EMSCRIPTEN__)
#include "platform/wasm/soluna_wasm_ime.h"
#endif



static void app_event(const sapp_event* ev);

#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__)

#define PLATFORM "windows"

#elif defined(__APPLE__)

#define PLATFORM "macos"

#elif defined(__EMSCRIPTEN__)

#define PLATFORM "wasm"

#elif defined(__linux__)

#define PLATFORM "linux"

#else

#define PLATFORM "unknown"

#endif

struct app_context {
	lua_State *L;
	lua_State *quitL;
	int (*send_log)(void *ud, unsigned int id, void *data, uint32_t sz);
	void *send_log_ud;
	void *mqueue;
};

static struct app_context *CTX = NULL;

struct soluna_ime_rect_state g_soluna_ime_rect = { 0.0f, 0.0f, 0.0f, 0.0f, false };

void soluna_emit_char(uint32_t codepoint, uint32_t modifiers, bool repeat);

struct soluna_message {
	const char *type;
	union {
		int p[2];
		uint64_t u64;
	} v;
};

static inline struct soluna_message *
message_create(const char *type, int p1, int p2) {
	struct soluna_message *msg = (struct soluna_message *)malloc(sizeof(*msg));
	msg->type = type;
	msg->v.p[0] = p1;
	msg->v.p[1] = p2;
	return msg;
}

static inline struct soluna_message *
message_create64(const char *type, uint64_t p) {
	struct soluna_message *msg = (struct soluna_message *)malloc(sizeof(*msg));
	msg->type = type;
	msg->v.u64 = p;
	return msg;
}

static inline void
message_release(struct soluna_message *msg) {
	free(msg);
}

void
soluna_emit_char(uint32_t codepoint, uint32_t modifiers, bool repeat) {
	sapp_event ev;
	memset(&ev, 0, sizeof(ev));
	ev.type = SAPP_EVENTTYPE_CHAR;
	ev.frame_count = sapp_frame_count();
	ev.char_code = codepoint;
	ev.modifiers = modifiers;
	ev.key_repeat = repeat;
	app_event(&ev);
}

static int
lmessage_send(lua_State *L) {
	luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
	luaL_checktype(L, 2, LUA_TLIGHTUSERDATA);
	int (*send_message)(void *ud, void *p) = lua_touserdata(L, 1);
	void *send_message_ud = lua_touserdata(L, 2);
	const char * what = NULL;
	if (lua_type(L, 3) == LUA_TSTRING) {
		what = lua_tostring(L, 3);
	} else {
		luaL_checktype(L, 3, LUA_TLIGHTUSERDATA);
		what = (const char *)lua_touserdata(L, 3);
	}
	int64_t p1 = luaL_optinteger(L, 4, 0);
	struct soluna_message * msg = NULL;
	if (lua_isnoneornil(L, 5)) {
		msg = message_create64(what, p1);
	} else {
		int p2 = luaL_checkinteger(L, 5);
		msg = message_create(what, (int)p1, p2);
	}
	int fail = send_message(send_message_ud, msg);
	if (fail) {
		message_release(msg);
	}
	lua_pushboolean(L, !fail);
	return 1;
}

static int
lmessage_unpack(lua_State *L) {
	luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
	struct soluna_message *m = (struct soluna_message *)lua_touserdata(L,1);
	lua_pushstring(L, m->type);
	lua_pushinteger(L, m->v.p[0]);
	lua_pushinteger(L, m->v.p[1]);
	lua_pushinteger(L, m->v.u64);
	message_release(m);
	return 4;
}

static int
lquit_signal(lua_State *L) {
	if (CTX) {
		CTX->quitL = CTX->L;
		CTX->L = NULL;
	}
	return 0;
}

static int
levent_unpack(lua_State *L) {
	luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
	struct event_message em;
	app_event_unpack(&em, lua_touserdata(L, 1));
	lua_pushlightuserdata(L, (void *)em.typestr);
	lua_pushinteger(L, em.p1);
	lua_pushinteger(L, em.p2);
	lua_pushinteger(L, em.p3);
	return 4;
}

static int
lset_window_title(lua_State *L) {
	if (CTX == NULL || lua_type(L, 1) != LUA_TSTRING)
		return 0;
	const char * text = lua_tostring(L, 1);
	sapp_set_window_title(text);
	return 0;
}

struct icon_pixels {
	uint8_t *ptr;
};

static void
icon_free_pixels(struct icon_pixels *payloads, int count) {
	for (int i = 0; i < count; ++i) {
		free(payloads[i].ptr);
	}
}

static int
icon_get_int(lua_State *L, int index, const char *field, const char *fallback) {
	int abs_index = lua_absindex(L, index);
	int value = 0;
	int type = lua_getfield(L, abs_index, field);
	if (type == LUA_TNUMBER) {
		value = (int)lua_tointeger(L, -1);
		lua_pop(L, 1);
		return value;
	}
	lua_pop(L, 1);
	if (fallback) {
		type = lua_getfield(L, abs_index, fallback);
		if (type == LUA_TNUMBER) {
			value = (int)lua_tointeger(L, -1);
			lua_pop(L, 1);
			return value;
		}
		lua_pop(L, 1);
	}
	luaL_error(L, "icon missing %s", field);
	return 0;
}

static void
icon_copy_image(lua_State *L, int index, sapp_image_desc *dst, struct icon_pixels *payload) {
	int abs_index = lua_absindex(L, index);
	int width = icon_get_int(L, abs_index, "w", "width");
	int height = icon_get_int(L, abs_index, "h", "height");
	if (width <= 0 || height <= 0) {
		luaL_error(L, "icon size (%d * %d) must be positive", width, height);
	}

	size_t stride = 0;
	if (lua_getfield(L, abs_index, "stride") == LUA_TNUMBER) {
		stride = (size_t)lua_tointeger(L, -1);
	}
	lua_pop(L, 1);

	size_t explicit_size = 0;
	if (lua_getfield(L, abs_index, "size") == LUA_TNUMBER) {
		explicit_size = (size_t)lua_tointeger(L, -1);
	}
	lua_pop(L, 1);

	int type = lua_getfield(L, abs_index, "data");
	const uint8_t *src = NULL;
	size_t src_size = 0;
	if (type == LUA_TSTRING) {
		src = (const uint8_t *)lua_tolstring(L, -1, &src_size);
	} else if (type == LUA_TUSERDATA) {
		src = (const uint8_t *)lua_touserdata(L, -1);
		src_size = lua_rawlen(L, -1);
	} else if (type == LUA_TLIGHTUSERDATA) {
		src = (const uint8_t *)lua_touserdata(L, -1);
		src_size = explicit_size;
	} else {
		lua_pop(L, 1);
		luaL_error(L, "icon.data must be buffer");
	}
	lua_pop(L, 1);
	if (src == NULL) {
		luaL_error(L, "icon data missing");
	}

	size_t row_bytes = (size_t)width * 4;
	if (stride == 0) {
		stride = row_bytes;
	}
	if (stride < row_bytes) {
		luaL_error(L, "icon stride < width");
	}
	if (!(type == LUA_TLIGHTUSERDATA && explicit_size == 0)) {
		size_t required = stride * (size_t)height;
		if (src_size < required) {
			luaL_error(L, "icon buffer too small");
		}
	}

	size_t copy_size = row_bytes * (size_t)height;
	uint8_t *copy = (uint8_t *)malloc(copy_size);
	if (copy == NULL) {
		luaL_error(L, "icon alloc fail");
	}

	if (stride == row_bytes) {
		memcpy(copy, src, copy_size);
	} else {
		const uint8_t *s = src;
		uint8_t *d = copy;
		int y;
		for (y = 0; y < height; ++y) {
			memcpy(d, s, row_bytes);
			s += stride;
			d += row_bytes;
		}
	}

	dst->width = width;
	dst->height = height;
	dst->pixels.ptr = copy;
	dst->pixels.size = copy_size;
	payload->ptr = copy;
}

static int
lset_icon(lua_State *L) {
	if (lua_isnoneornil(L, 1))
		return 0;

	luaL_checktype(L, 1, LUA_TTABLE);

	sapp_icon_desc desc;
	memset(&desc, 0, sizeof(desc));
	struct icon_pixels payloads[SAPP_MAX_ICONIMAGES];
	memset(payloads, 0, sizeof(payloads));
	int count = 0;
	int abs_index = lua_absindex(L, 1);

	if (lua_getfield(L, abs_index, "data") != LUA_TNIL) {
		lua_pop(L, 1);
		icon_copy_image(L, abs_index, &desc.images[count], &payloads[count]);
		++count;
	} else {
		lua_pop(L, 1);
		int len = (int)lua_rawlen(L, abs_index);
		for (int i = 1; i <= len; ++i) {
			if (count >= SAPP_MAX_ICONIMAGES) {
				icon_free_pixels(payloads, count);
				luaL_error(L, "too many icon images");
			}
			lua_rawgeti(L, abs_index, i);
			luaL_checktype(L, -1, LUA_TTABLE);
			icon_copy_image(L, -1, &desc.images[count], &payloads[count]);
			++count;
			lua_pop(L, 1);
		}
	}

	if (count > 0) {
		sapp_set_icon(&desc);
	}

	icon_free_pixels(payloads, count);
	return 0;
}

static int
lset_ime_rect(lua_State *L) {
#if defined(__APPLE__) || defined(_MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__) || defined(__linux__) || defined(__EMSCRIPTEN__)
	if (lua_isnoneornil(L, 1)) {
		g_soluna_ime_rect.valid = false;
#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__)
		soluna_win32_apply_ime_rect();
#elif defined(__APPLE__)
		soluna_macos_hide_ime_label();
#elif defined(__linux__)
		soluna_linux_on_rect_cleared();
#elif defined(__EMSCRIPTEN__)
		soluna_wasm_hide();
#endif
		return 0;
	}
	g_soluna_ime_rect.x = (float)luaL_checknumber(L, 1);
	g_soluna_ime_rect.y = (float)luaL_checknumber(L, 2);
	g_soluna_ime_rect.w = (float)luaL_checknumber(L, 3);
	g_soluna_ime_rect.h = (float)luaL_checknumber(L, 4);
	g_soluna_ime_rect.valid = true;
#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__)
	soluna_win32_apply_ime_rect();
#elif defined(__APPLE__)
	soluna_macos_apply_ime_rect();
#elif defined(__linux__)
	soluna_linux_update_spot();
#elif defined(__EMSCRIPTEN__)
	soluna_wasm_apply_rect();
#endif
#endif
	return 0;
}

static int
lset_ime_font(lua_State *L) {
	const char *name = NULL;
	float size = 0.0f;
	int top = lua_gettop(L);
	if (top == 0) {
#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__)
		soluna_win32_reset_ime_font();
#endif
#if defined(__APPLE__)
		soluna_macos_set_ime_font(NULL, 0.0f);
#endif
#if defined(__EMSCRIPTEN__)
		soluna_wasm_set_font(NULL, 0.0f);
#endif
		return 0;
	}
	if (top == 1) {
		size = (float)luaL_checknumber(L, 1);
	} else {
		if (!lua_isnoneornil(L, 1)) {
			if (lua_type(L, 1) != LUA_TSTRING) {
				return luaL_error(L, "set_ime_font expects string font name");
			}
			name = lua_tostring(L, 1);
		}
		size = (float)luaL_checknumber(L, 2);
	}
	if (size < 0.0f) {
		size = 0.0f;
	}
#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__)
	soluna_win32_set_ime_font(name, size);
#endif
#if defined(__APPLE__)
	soluna_macos_set_ime_font(name, size);
#endif
#if defined(__EMSCRIPTEN__)
	soluna_wasm_set_font(name, size);
#endif
	return 0;
}

static int
lclose_window(lua_State *L) {
	sapp_quit();
	return 0;
}

static int
lmqueue(lua_State *L) {
	if (CTX == NULL || CTX->mqueue == NULL) {
		return luaL_error(L, "Not init mqueue");
	}
	lua_pushlightuserdata(L, CTX->mqueue);
	return 1;
}

static int
lversion(lua_State *L) {
	lua_pushinteger(L, SOLUNA_API_VERSION);
	lua_pushstring(L, SOLUNA_HASH_VERSION);
	return 2;
}

static void
desc_get_boolean(lua_State *L, bool *r, int index, const char * key) {
	if (lua_getfield(L, index, key) == LUA_TBOOLEAN) {
		*r = lua_toboolean(L, -1);
	} else {
		luaL_checktype(L, -1, LUA_TNIL);
	}
	lua_pop(L, 1);
}

static void
desc_get_int(lua_State *L, int *r, int index, const char * key) {
	if (lua_getfield(L, index, key) == LUA_TNUMBER) {
		*r = lua_tointeger(L, -1);
	} else {
		luaL_checktype(L, -1, LUA_TNIL);
	}
	lua_pop(L, 1);
}

static void
desc_get_string(lua_State *L, const char **r, int index, const char * key) {
	if (lua_getfield(L, index, key) == LUA_TSTRING) {
		*r = lua_tostring(L, -1);
	} else {
		luaL_checktype(L, -1, LUA_TNIL);
	}
	lua_pop(L, 1);
}

static int
linit_desc(lua_State *L) {
	luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
	luaL_checktype(L, 2, LUA_TTABLE);
	sapp_desc *d = lua_touserdata(L, 1);
	desc_get_boolean(L, &d->high_dpi, 2, "high_dpi");
	desc_get_boolean(L, &d->fullscreen, 2, "fullscreen");
	desc_get_int(L, &d->width, 2, "width");
	desc_get_int(L, &d->height, 2, "height");
	desc_get_string(L, &d->window_title, 2, "window_title");

	return 0;
}

int
luaopen_soluna_app(lua_State *L) {
	luaL_checkversion(L);
	luaL_Reg l[] = {
		{ "mqueue", lmqueue },
		{ "unpackmessage", lmessage_unpack },
		{ "sendmessage", lmessage_send },
		{ "unpackevent", levent_unpack },
		{ "set_window_title", lset_window_title },
		{ "set_icon", lset_icon },
		{ "set_ime_rect", lset_ime_rect },
		{ "set_ime_font", lset_ime_font },
		{ "quit", lquit_signal },
		{ "close_window", lclose_window },
		{ "platform", NULL },
		{ "version", lversion },
		{ "init_desc", linit_desc },
		{ NULL, NULL },
	};
	luaL_newlib(L, l);
	lua_pushliteral(L, PLATFORM);
	lua_setfield(L, -2, "platform");

	return 1;
}

static void
log_func(const char* tag, uint32_t log_level, uint32_t log_item, const char* message, uint32_t line_nr, const char* filename, void* user_data) {
	if (CTX == NULL || CTX->send_log == NULL) {
		fprintf(stderr, "%s (%d) : %s\n", filename, line_nr, message);
		return;
	}
	struct log_info *msg = (struct log_info *)malloc(sizeof(*msg));
	if (tag) {
		strncpy(msg->tag, tag, sizeof(msg->tag));
		msg->tag[sizeof(msg->tag)-1] = 0;
	} else {
		msg->tag[0] = 0;
	}
	msg->log_level = log_level;
	msg->log_item = log_item;
	msg->line_nr = line_nr;
	if (message) {
		strncpy(msg->message, message, sizeof(msg->message));
		msg->message[sizeof(msg->message)-1] = 0;
	} else {
		msg->message[0] = 0;
	}
	msg->filename = filename;
	CTX->send_log(CTX->send_log_ud, 0, msg, sizeof(*msg));
}

void soluna_openlibs(lua_State *L);

static const char *code = "local embed = require 'soluna.embedsource' ; local f = load(embed.runtime.main()) ; return f(...)";

static int
pmain(lua_State *L) {
	char** argv = (char **)lua_touserdata(L, 1);
	soluna_openlibs(L);
	int n = sargs_num_args();
	luaL_checkstack(L, n+1, NULL);
	int i;
	lua_newtable(L);
	int arg_table = lua_gettop(L);
	for (i=0;i<n;i++) {
		const char *k = sargs_key_at(i);
		const char *v = sargs_value_at(i);
		if (v[0] == 0) {
			lua_pushstring(L, argv[i+1]);
		} else {
			lua_pushstring(L, v);
			lua_setfield(L, arg_table, k);
		}
	}
	int arg_n = lua_gettop(L) - arg_table + 1;
	if (luaL_loadstring(L, code) != LUA_OK) {
		return lua_error(L);
	}
	lua_insert(L, -arg_n-1);
	lua_call(L, arg_n, 1);
	return 1;
}

static void *
get_ud(lua_State *L, const char *key) {
	if (lua_getfield(L, -1, key) != LUA_TLIGHTUSERDATA) {
		lua_pop(L, 1);
		return NULL;
	}
	void * ud = lua_touserdata(L, -1);
	lua_pop(L, 1);
	return ud;
}

static int
get_function(lua_State *L, const char *key, int index) {
	if (lua_getfield(L, -1, key) != LUA_TFUNCTION) {
		fprintf(stderr, "main.lua need return a %s function", key);
		return 1;
	}
	lua_insert(L, index);
	return 0;
}

static int
init_callback(lua_State *L, struct app_context * ctx) {
	if (!lua_istable(L, -1)) {
		fprintf(stderr, "main.lua need return a table, it's %s\n", lua_typename(L, lua_type(L, -1)));
		return 1;
	}
	ctx->send_log = get_ud(L, "send_log");
	ctx->send_log_ud = get_ud(L, "send_log_ud");
	ctx->mqueue = get_ud(L, "mqueue");
	if (get_function(L, "frame", FRAME_CALLBACK))
		return 1;
	if (get_function(L, "cleanup", CLEANUP_CALLBACK))
		return 1;
	if (get_function(L, "event", EVENT_CALLBACK))
		return 1;
	lua_settop(L, CALLBACK_COUNT);
	return 0;
}

static int
msghandler(lua_State *L) {
	const char *msg = lua_tostring(L, 1);
	luaL_traceback(L, L, msg, 1);
	return 1;
}

static void
get_app_info(lua_State *L) {
	lua_newtable(L);
	const float dpi_scale = sapp_dpi_scale();
	const float safe_scale = dpi_scale > 0.0f ? dpi_scale : 1.0f;
	const int fb_width = sapp_width();
	const int fb_height = sapp_height();
	const int logical_width = (int)((float)fb_width / safe_scale + 0.5f);
	const int logical_height = (int)((float)fb_height / safe_scale + 0.5f);
	lua_pushinteger(L, logical_width);
	lua_setfield(L, -2, "width");
	lua_pushinteger(L, logical_height);
	lua_setfield(L, -2, "height");
}

static int
start_app(lua_State *L) {
	if (L == NULL) {
		fprintf(stderr, "Can't open lua state\n");
		return 1;
	}

	if (lua_islightuserdata(L, 1)) {
		fprintf(stderr, "Init fatal : %s\n", (const char *)lua_touserdata(L, 1));
		return 1;
	}

	if (lua_gettop(L) != 2) {
		fprintf(stderr, "Invalid lua stack (top = %d)\n", lua_gettop(L));
		return 1;
	}
	if (lua_getfield(L, -1, "start") != LUA_TFUNCTION) {
		fprintf(stderr, "No start function\n");
		return 1;
	}
	lua_replace(L, -2);
	get_app_info(L);
	if (lua_pcall(L, 1, 1, 1) != LUA_OK) {
		fprintf(stderr, "Start fatal : %s\n", lua_tostring(L, -1));
		return 1;
	} else {
		return init_callback(L, CTX);
	}
}

static void
app_init() {
#if defined(__APPLE__)
	soluna_macos_install_ime();
#endif
#if defined(__linux__)
	soluna_linux_ensure_im();
#endif
#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__)
	soluna_win32_install_wndproc();
#endif
	
	sg_setup(&(sg_desc) {
        .environment = sglue_environment(),
        .logger.func = log_func,			
	});
		
	lua_State *L = CTX->L;
	if (start_app(L)) {
		if (L) {
			lua_close(L);
		}
		CTX->L = NULL;
		CTX->quitL = NULL;
		sapp_quit();
	}
}

static lua_State *
get_L(struct app_context *ctx) {
	if (ctx == NULL)
		return NULL;
	lua_State *L = ctx->L;
	if (L == NULL) {
		if (ctx->quitL != NULL) {
			sapp_quit();
		}
	}
	return L;
}

static void
invoke_callback(lua_State *L, int index, int nargs) {
	lua_pushvalue(L, index);
	if (nargs > 0) {
		lua_insert(L, -nargs-1);
	}
	if (lua_pcall(L, nargs, 0, 0) != LUA_OK) {
		fprintf(stderr, "Error : %s\n", lua_tostring(L, -1));
		lua_pop(L, 1);
	}
}

static void
app_frame() {
	lua_State *L = get_L(CTX);
	if (L) {
		lua_pushinteger(L, sapp_frame_count());
		invoke_callback(L, FRAME_CALLBACK, 1);
	}
}

static void
app_cleanup() {
	if (CTX == NULL)
		return;
	lua_State *L = CTX->quitL;
	if (L) {
		invoke_callback(L, CLEANUP_CALLBACK, 0);
		lua_close(L);
		CTX->quitL = NULL;
	}
#if defined(__linux__)
	soluna_linux_shutdown_ime();
#endif
#if defined(__EMSCRIPTEN__)
	soluna_wasm_hide();
#endif
	sg_shutdown();
}

static void
app_event(const sapp_event* ev) {
#if defined(__APPLE__)
	if (soluna_macos_is_composition_active() &&
		(ev->type == SAPP_EVENTTYPE_KEY_DOWN ||
		 ev->type == SAPP_EVENTTYPE_KEY_UP)) {
		return;
	}
#endif
#if defined(__EMSCRIPTEN__)
	if (soluna_wasm_should_block_key_event(ev)) {
		return;
	}
#endif
#if defined(__linux__)
	if (soluna_linux_should_skip_event(ev)) {
		return;
	}
#endif
#if defined(__EMSCRIPTEN__)
	if (soluna_wasm_filter_char_event(ev)) {
		return;
	}
#endif
#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__)
	if (ev->type == SAPP_EVENTTYPE_FOCUSED && g_soluna_ime_rect.valid) {
		soluna_win32_apply_ime_rect();
	}
#endif
#if defined(__linux__)
	soluna_linux_handle_event(ev);
#endif
#if defined(__EMSCRIPTEN__)
	soluna_wasm_handle_event(ev);
#endif
	lua_State *L = get_L(CTX);
	if (L) {
		lua_pushlightuserdata(L, (void *)ev);
		invoke_callback(L, EVENT_CALLBACK, 1);
	}
}

static int
init_settings(lua_State *L, sapp_desc *desc) {
	if (lua_gettop(L) != 2) {
		lua_pushlightuserdata(L, (void *)"Invalid lua stack");
		return 1;
	}
	if (lua_getfield(L, -1, "init") != LUA_TFUNCTION) {
		lua_pushlightuserdata(L, (void *)"No start function");
		return 1;
	}
	lua_pushlightuserdata(L, (void *)desc);
	if (lua_pcall(L, 1, 0, 1) != LUA_OK) {
		const char * err = lua_tostring(L, -1);
		lua_pushlightuserdata(L, (void *)err);
		return 1;
	} else {
		return 0;
	}
}

sapp_desc
sokol_main(int argc, char* argv[]) {
	// init sargs
	sargs_desc arg_desc;
	memset(&arg_desc, 0, sizeof(arg_desc));
	arg_desc.argc = argc;
	arg_desc.argv = argv;
	sargs_setup(&arg_desc);
	
	// default sapp_desc
	sapp_desc d;
	memset(&d, 0, sizeof(d));

	d.init_cb = app_init;
	d.frame_cb = app_frame;
	d.cleanup_cb = app_cleanup;
	d.event_cb = app_event;
	d.logger.func = log_func;
	d.win32_console_utf8 = 1;
	d.win32_console_attach = 1;
	d.alpha = 0;
	
	// init L
	static struct app_context app;
	lua_State *L = luaL_newstate();

	if (L) {
		lua_settop(L, 0);
		lua_pushcfunction(L, msghandler);
		lua_pushcfunction(L, pmain);
		lua_pushlightuserdata(L, (void *)argv);
		
		if (lua_pcall(L, 1, 1, 1) != LUA_OK) {
			const char * err = lua_tostring(L, -1);
			lua_pushlightuserdata(L, (void *)err);
			lua_replace(L, 1);
		}
		sargs_shutdown();
		
		if (init_settings(L, &d)) {
			lua_replace(L, 1);
		}
	}

	app.L = L;
	app.quitL = NULL;
	app.send_log = NULL;
	app.send_log_ud = NULL;
	app.mqueue = NULL;
	
	CTX = &app;

	return d;
}
