#include <lua.h>
#include <lauxlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "sokol/sokol_gfx.h"
#include "batch.h"
#include "spritemgr.h"
#include "render_bindings.h"
#include "extapi_types.h"

#define STREAM_FIX_SCALE 256.0f
#define STREAM_FIX_INV_SCALE (1.0f / STREAM_FIX_SCALE)

typedef void (*material_submit_stride_func)(lua_State *L, void *ud, void *stream, int n);

static void
submit_material_stride(lua_State *L, int batch_n, void *ud, material_submit_stride_func submit, size_t stride) {
	char *stream = lua_touserdata(L, 2);
	int prim_n = luaL_checkinteger(L, 3);
	if (stream == NULL) {
		luaL_error(L, "Missing material stream");
	}
	if (batch_n <= 0) {
		luaL_error(L, "Invalid material submit batch %d", batch_n);
	}
	if (prim_n < 0) {
		luaL_error(L, "Invalid material primitive count %d", prim_n);
	}
	if (submit == NULL) {
		luaL_error(L, "Missing material submit function");
	}
	if (stride == 0) {
		luaL_error(L, "Invalid material submit stride");
	}
	int i = 0;
	for (;;) {
		int n = prim_n - i;
		if (n > batch_n) {
			submit(L, ud, stream, batch_n);
			i += batch_n;
			stream += stride * batch_n;
		} else {
			submit(L, ud, stream, n);
			break;
		}
	}
}

struct material_submit_context {
	void *ud;
	soluna_material_submit_func submit;
};

static void
submit_external_material(lua_State *L, void *ctx_, void *stream, int n) {
	struct material_submit_context *ctx = (struct material_submit_context *)ctx_;
	ctx->submit(L, ctx->ud, stream, n);
}

void
material_submit(lua_State *L, int batch_n, void *ud, soluna_material_submit_func submit) {
	struct material_submit_context ctx = {
		.ud = ud,
		.submit = submit,
	};
	submit_material_stride(L, batch_n, &ctx, submit_external_material, sizeof(struct draw_primitive) * 2);
}

int
material_sprite_rect(lua_State *L, void *bank, int sprite, struct soluna_sprite_rect *out) {
	(void)L;
	struct sprite_bank *b = (struct sprite_bank *)bank;
	if (b == NULL || out == NULL || sprite < 0 || sprite >= b->n) {
		return 0;
	}
	struct sprite_rect *r = &b->rect[sprite];
	out->texture = r->texid;
	out->u = (float)(r->u >> 16);
	out->v = (float)(r->v >> 16);
	out->w = (float)(r->u & 0xffffu);
	out->h = (float)(r->v & 0xffffu);
	out->ox = (float)((r->off >> 16) & 0xffffu) - 0x8000;
	out->oy = (float)(r->off & 0xffffu) - 0x8000;
	return 1;
}

sg_bindings
material_bindings(lua_State *L, void *bindings) {
	struct soluna_render_bindings *b = (struct soluna_render_bindings *)bindings;
	if (b == NULL) {
		luaL_error(L, "Missing material bindings");
	}
	return b->bindings;
}

static size_t
stream_payload_max(void) {
	return sizeof(struct draw_primitive) - sizeof(struct draw_primitive_external);
}

struct stream_guard {
	void *ptr;
};

static int
stream_guard_gc(lua_State *L) {
	struct stream_guard *guard = (struct stream_guard *)lua_touserdata(L, 1);
	free(guard->ptr);
	guard->ptr = NULL;
	return 0;
}

static struct stream_guard *
push_stream_guard(lua_State *L) {
	struct stream_guard *guard = (struct stream_guard *)lua_newuserdatauv(L, sizeof(*guard), 0);
	guard->ptr = NULL;
	if (luaL_newmetatable(L, "SOLUNA_MATERIAL_STREAM_GUARD")) {
		luaL_Reg l[] = {
			{ "__gc", stream_guard_gc },
			{ NULL, NULL },
		};
		luaL_setfuncs(L, l, 0);
	}
	lua_setmetatable(L, -2);
	return guard;
}

static void *
free_stream(void *ud, void *ptr, size_t osize, size_t nsize) {
	(void)ud;
	(void)osize;
	if (nsize == 0) {
		free(ptr);
	}
	return NULL;
}

void
material_push_stream(lua_State *L, int material_id, int count, size_t payload_size, soluna_material_stream_write_func write, void *ud) {
	size_t payload_max = stream_payload_max();
	if (material_id <= 0) {
		luaL_error(L, "Invalid material id %d", material_id);
	}
	if (count < 0) {
		luaL_error(L, "Invalid material stream count %d", count);
	}
	if (payload_size > payload_max) {
		luaL_error(L, "Invalid material payload size %d > %d", (int)payload_size, (int)payload_max);
	}
	if (write == NULL) {
		luaL_error(L, "Missing material stream writer");
	}
	size_t item_size = sizeof(struct draw_primitive) * 2;
	if ((size_t)count > (~(size_t)0 - 1) / item_size) {
		luaL_error(L, "Material stream is too large");
	}
	size_t stream_size = item_size * (size_t)count;
	struct stream_guard *guard = push_stream_guard(L);
	int guard_index = lua_gettop(L);
	char *buffer = (char *)malloc(stream_size + 1);
	if (buffer == NULL) {
		luaL_error(L, "No memory for material stream");
	}
	guard->ptr = buffer;
	struct draw_primitive *stream = (struct draw_primitive *)buffer;
	int i;
	for (i=0; i<count; i++) {
		struct draw_primitive *pos = &stream[i * 2];
		struct draw_primitive *ext_prim = pos + 1;
		struct draw_primitive_external *ext = (struct draw_primitive_external *)ext_prim;
		struct soluna_material_stream_item item = {
			.x = 0.0f,
			.y = 0.0f,
			.sprite = -1,
			.payload = NULL,
		};
		memset(pos, 0, sizeof(*pos));
		memset(ext_prim, 0, sizeof(*ext_prim));
		write(ud, i, &item);
		if (payload_size > 0) {
			if (item.payload == NULL) {
				luaL_error(L, "Missing material stream payload");
			}
			memcpy((char *)ext_prim + sizeof(*ext), item.payload, payload_size);
		}
		pos->x = (int32_t)(item.x * STREAM_FIX_SCALE);
		pos->y = (int32_t)(item.y * STREAM_FIX_SCALE);
		pos->sprite = -material_id;
		ext->sprite = item.sprite;
	}
	buffer[stream_size] = '\0';
	guard->ptr = NULL;
	lua_pushexternalstring(L, buffer, stream_size, free_stream, NULL);
	lua_remove(L, guard_index);
}

void
material_stream_read(lua_State *L, const void *stream, int index, int material_id, size_t payload_size, void *payload, struct soluna_material_stream_data *out) {
	size_t payload_max = stream_payload_max();
	if (payload_size > payload_max) {
		luaL_error(L, "Invalid material payload size %d > %d", (int)payload_size, (int)payload_max);
	}
	if (stream == NULL) {
		luaL_error(L, "Missing material stream");
	}
	if (index < 0) {
		luaL_error(L, "Invalid material stream index %d", index);
	}
	if (out == NULL) {
		luaL_error(L, "Missing material stream output");
	}
	if (payload_size > 0 && payload == NULL) {
		luaL_error(L, "Missing material stream payload output");
	}
	const struct draw_primitive *prim = (const struct draw_primitive *)stream;
	const struct draw_primitive *pos = &prim[index * 2];
	const struct draw_primitive *ext_prim = pos + 1;
	const struct draw_primitive_external *ext = (const struct draw_primitive_external *)ext_prim;
	if (pos->sprite != -material_id) {
		luaL_error(L, "Invalid material marker %d", pos->sprite);
	}
	out->x = (float)pos->x * STREAM_FIX_INV_SCALE;
	out->y = (float)pos->y * STREAM_FIX_INV_SCALE;
	out->sprite = ext->sprite;
	if (payload_size > 0) {
		memcpy(payload, (const char *)ext_prim + sizeof(*ext), payload_size);
	}
}
