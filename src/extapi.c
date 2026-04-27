#include <assert.h>
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
#define STREAM_ERROR_SIZE 128

#if defined(_MSC_VER)
#define SOLUNA_THREAD_LOCAL __declspec(thread)
#elif defined(__GNUC__)
#define SOLUNA_THREAD_LOCAL __thread
#else
#define SOLUNA_THREAD_LOCAL _Thread_local
#endif

typedef void (*material_submit_stride_func)(void *ud, void *ctx, int n);

struct material_stream_context {
	const char *data;
	int n;
	int material_id;
	soluna_material_error error;
	char error_buffer[STREAM_ERROR_SIZE];
};

static SOLUNA_THREAD_LOCAL char submit_error_buffer[STREAM_ERROR_SIZE];

static soluna_material_error
copy_error(char *buffer, size_t size, const char *error) {
	const char *message = error != NULL ? error : "Material stream error";
	size_t len = strlen(message);
	if (len >= size) {
		len = size - 1;
	}
	memcpy(buffer, message, len);
	buffer[len] = '\0';
	return buffer;
}

void
material_stream_error(void *ctx_, const char *error) {
	struct material_stream_context *ctx = (struct material_stream_context *)ctx_;
	if (ctx != NULL && ctx->error == NULL) {
		ctx->error = copy_error(ctx->error_buffer, sizeof(ctx->error_buffer), error);
	}
}

int
material_stream_failed(void *ctx_) {
	struct material_stream_context *ctx = (struct material_stream_context *)ctx_;
	return ctx == NULL || ctx->error != NULL;
}

static soluna_material_error
submit_material_stride(const void *data_, int prim_n, int material_id, int batch_n, void *ud, material_submit_stride_func submit, size_t stride) {
	const char *data = (const char *)data_;
	if (data == NULL) {
		return "Missing material stream";
	}
	if (material_id <= 0) {
		return "Invalid material id";
	}
	if (batch_n <= 0) {
		return "Invalid material submit batch";
	}
	if (prim_n < 0) {
		return "Invalid material primitive count";
	}
	if (submit == NULL) {
		return "Missing material submit function";
	}
	if (stride == 0) {
		return "Invalid material submit stride";
	}
	int i = 0;
	for (;;) {
		int n = prim_n - i;
		struct material_stream_context ctx = {
			.data = data,
			.n = n > batch_n ? batch_n : n,
			.material_id = material_id,
			.error = NULL,
			.error_buffer = { 0 },
		};
		if (n > batch_n) {
			submit(ud, &ctx, batch_n);
			if (ctx.error != NULL) {
				return copy_error(submit_error_buffer, sizeof(submit_error_buffer), ctx.error);
			}
			i += batch_n;
			data += stride * batch_n;
		} else {
			submit(ud, &ctx, n);
			if (ctx.error != NULL) {
				return copy_error(submit_error_buffer, sizeof(submit_error_buffer), ctx.error);
			}
			break;
		}
	}
	return NULL;
}

soluna_material_error
material_submit(const void *stream, int prim_n, int material_id, int batch_n, void *ud, soluna_material_submit_func submit) {
	return submit_material_stride(stream, prim_n, material_id, batch_n, ud, submit, sizeof(struct draw_primitive) * 2);
}

int
material_sprite_rect(void *bank, int sprite, struct soluna_sprite_rect *out) {
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
material_bindings(void *bindings) {
	struct soluna_render_bindings *b = (struct soluna_render_bindings *)bindings;
	assert(b != NULL);
	return b->bindings;
}

static size_t
stream_payload_max(void) {
	return sizeof(struct draw_primitive) - sizeof(struct draw_primitive_external);
}

void
material_stream_free(void *ptr) {
	free(ptr);
}

soluna_material_error
material_push_stream(int material_id, int count, size_t payload_size, soluna_material_stream_write_func write, void *ud, struct soluna_material_stream *out) {
	size_t payload_max = stream_payload_max();
	if (out == NULL) {
		return "Missing material stream output";
	}
	out->data = NULL;
	out->size = 0;
	if (material_id <= 0) {
		return "Invalid material id";
	}
	if (count < 0) {
		return "Invalid material stream count";
	}
	if (payload_size > payload_max) {
		return "Invalid material payload size";
	}
	if (write == NULL) {
		return "Missing material stream writer";
	}
	size_t item_size = sizeof(struct draw_primitive) * 2;
	if ((size_t)count > (~(size_t)0 - 1) / item_size) {
		return "Material stream is too large";
	}
	size_t stream_size = item_size * (size_t)count;
	char *buffer = (char *)malloc(stream_size + 1);
	if (buffer == NULL) {
		return "No memory for material stream";
	}
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
				material_stream_free(buffer);
				return "Missing material stream payload";
			}
			memcpy((char *)ext_prim + sizeof(*ext), item.payload, payload_size);
		}
		pos->x = (int32_t)(item.x * STREAM_FIX_SCALE);
		pos->y = (int32_t)(item.y * STREAM_FIX_SCALE);
		pos->sprite = -material_id;
		ext->sprite = item.sprite;
	}
	buffer[stream_size] = '\0';
	out->data = buffer;
	out->size = stream_size;
	return NULL;
}

static void
clear_stream_read(size_t payload_size, void *payload, struct soluna_material_stream_data *out) {
	if (out != NULL) {
		memset(out, 0, sizeof(*out));
	}
	if (payload != NULL && payload_size > 0 && payload_size <= stream_payload_max()) {
		memset(payload, 0, payload_size);
	}
}

static int
fail_stream_read(void *ctx, const char *error, size_t payload_size, void *payload, struct soluna_material_stream_data *out) {
	material_stream_error(ctx, error);
	clear_stream_read(payload_size, payload, out);
	return 0;
}

int
material_stream_read(void *ctx_, int index, size_t payload_size, void *payload, struct soluna_material_stream_data *out) {
	struct material_stream_context *ctx = (struct material_stream_context *)ctx_;
	size_t payload_max = stream_payload_max();
	if (ctx == NULL) {
		clear_stream_read(payload_size, payload, out);
		return 0;
	}
	if (ctx->error != NULL) {
		clear_stream_read(payload_size, payload, out);
		return 0;
	}
	if (payload_size > payload_max) {
		return fail_stream_read(ctx_, "Invalid material payload size", payload_size, payload, out);
	}
	if (ctx->data == NULL) {
		return fail_stream_read(ctx_, "Missing material stream", payload_size, payload, out);
	}
	if (index < 0) {
		return fail_stream_read(ctx_, "Invalid material stream index", payload_size, payload, out);
	}
	if (index >= ctx->n) {
		return fail_stream_read(ctx_, "Invalid material stream index", payload_size, payload, out);
	}
	if (ctx->material_id <= 0) {
		return fail_stream_read(ctx_, "Invalid material id", payload_size, payload, out);
	}
	if (out == NULL) {
		return fail_stream_read(ctx_, "Missing material stream output", payload_size, payload, out);
	}
	if (payload_size > 0 && payload == NULL) {
		return fail_stream_read(ctx_, "Missing material stream payload output", payload_size, payload, out);
	}
	const struct draw_primitive *prim = (const struct draw_primitive *)ctx->data;
	const struct draw_primitive *pos = &prim[index * 2];
	const struct draw_primitive *ext_prim = pos + 1;
	const struct draw_primitive_external *ext = (const struct draw_primitive_external *)ext_prim;
	if (pos->sprite != -ctx->material_id) {
		return fail_stream_read(ctx_, "Invalid material marker", payload_size, payload, out);
	}
	out->x = (float)pos->x * STREAM_FIX_INV_SCALE;
	out->y = (float)pos->y * STREAM_FIX_INV_SCALE;
	out->sprite = ext->sprite;
	if (payload_size > 0) {
		memcpy(payload, (const char *)ext_prim + sizeof(*ext), payload_size);
	}
	return 1;
}
