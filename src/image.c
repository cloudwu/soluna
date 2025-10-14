#include <lua.h>
#include <lauxlib.h>

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

static void *
malloc_luastring(size_t sz) {
	unsigned char * buffer = malloc(sz+1);
	if (buffer) {
		buffer[sz] = 0;
		return (void *)buffer;
	} else {
		return NULL;
	}
}

static void *
realloc_luastring(void *ptr, size_t sz) {
	unsigned char * buffer = realloc(ptr, sz+1);
	if (buffer) {
		buffer[sz] = 0;
		return (void *)buffer;
	} else {
		return NULL;
	}
}

#define STBI_MALLOC malloc_luastring
#define STBI_FREE free
#define STBI_REALLOC realloc_luastring

#define STBI_ONLY_PNG
#define STBI_MAX_DIMENSIONS 65536
#define STBI_NO_STDIO
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#define STBIW_WINDOWS_UTF8
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

#include "stb/stb_image_resize2.h"

#include "luabuffer.h"

static void *
free_image(void *ud, void *ptr, size_t osize, size_t nsize) {
	stbi_image_free(ptr);
	return NULL;
}

static void *
free_buffer(void *ud, void *ptr, size_t osize, size_t nsize) {
	free(ptr);
	return NULL;
}

static int
load_image(lua_State *L, int *x, int *y, stbi_uc **output) {
	size_t sz;
	int c;
	const stbi_uc *buffer = luaL_getbuffer(L, &sz);
	stbi_uc * img = stbi_load_from_memory(buffer, sz, x, y, &c, 4);
	if (img == NULL) {
		lua_pushnil(L);
		lua_pushstring(L, stbi_failure_reason());
		return 2;
	}
	*output = img;
	return 0;
}

static int
image_load(lua_State *L) {
	int x, y;
	stbi_uc * img = NULL;
	int r = load_image(L, &x, &y, &img);
	if (r)
		return r;
	lua_pushexternalstring(L, (const char *)img, x * y * 4, free_image, NULL);
	lua_pushinteger(L, x);
	lua_pushinteger(L, y);
	return 3;
}

static int
image_load_alpha(lua_State *L) {
	int x, y;
	stbi_uc * img = NULL;
	int r = load_image(L, &x, &y, &img);
	if (r)
		return r;
	int i, j;
	stbi_uc * ptr = img;
	for (i=0;i<y;i++) {
		for (j=0;j<x;j++) {
			ptr[3] = 255 - ptr[0];
			ptr[0] = 0;
			ptr[1] = 0;
			ptr[2] = 0;
			ptr += 4;
		}
	}
	lua_pushexternalstring(L, (const char *)img, x * y * 4, free_image, NULL);
	lua_pushinteger(L, x);
	lua_pushinteger(L, y);
	return 3;
};

static int
image_info(lua_State *L) {
	size_t sz;
	const stbi_uc* buffer = luaL_getbuffer(L, &sz);
	int x, y, c;
	if (!stbi_info_from_memory(buffer, sz, &x, &y, &c)) {
		lua_pushnil(L);
		lua_pushstring(L, stbi_failure_reason());
		return 2;
	}
	lua_pushinteger(L, x);
	lua_pushinteger(L, y);
	lua_pushinteger(L, c);
	return 3;
}

struct rect {
	const uint8_t * ptr;
	int stride;
	int width;
	int line;
};

static int
rect_init(struct rect *r, const uint8_t *buffer, int x, int y, int dx, int dy, int w, int h) {
	if (dx < 0) {
		w += dx;
		dx = 0;
	} else if (dx > x) {
		return 0;
	}
	if (dy < 0) {
		h += dy;
		dy = 0;
	} else if (dy > y) {
		return 0;
	}
	if (w + dx > x) {
		w = x - dx;
	}
	if (h + dy > y) {
		h = y - dy;
	}
	if (w <=0 || h <= 0)
		return 0;
	
	r->ptr = buffer + x * 4 * dy + 4 * dx;
	r->stride = 4 * x;
	r->width = w;
	r->line = h;

	return 1;
}

static int
remove_top(struct rect *r) {
	int x, y;
	const uint8_t * ptr = r->ptr;
	for (y=0;y<r->line;y++) {
		const uint8_t *cur = ptr;
		for (x=0;x<r->width;x++) {
			if (cur[3]) {
				r->ptr = ptr;
				r->line -= y;
				return y;
			}
			cur += 4;
		}
		ptr += r->stride;
	}
	return r->line;
}

static int
remove_bottom(struct rect *r) {
	int x, y;
	const uint8_t * ptr = r->ptr + (r->line - 1) * r->stride;
	for (y=0;y<r->line-1;y++) {
		const uint8_t *cur = ptr;
		for (x=0;x<r->width;x++) {
			if (cur[3]) {
				r->line -= y;
				return y;
			}
			cur += 4;
		}
		ptr -= r->stride;
	}
	return r->line;
}

static int
remove_left(struct rect *r) {
	int x, y;
	const uint8_t * ptr = r->ptr;
	int min_left = r->width;
	for (y=0;y<r->line;y++) {
		const uint8_t *cur = ptr;
		for (x=0;x<min_left;x++) {
			if (cur[3]) {
				break;
			}
			cur += 4;
		}
		if (x == 0)
			return 0;
		else if (x < min_left) {
			min_left = x;
		}
		ptr += r->stride;
	}
	return min_left;
}

static int
remove_right(struct rect *r) {
	int x, y;
	const uint8_t * ptr = r->ptr + r->width * 4;
	int min_right = r->width;
	for (y=0;y<r->line;y++) {
		const uint8_t *cur = ptr;
		for (x=0;x<min_right;x++) {
			cur -= 4;
			if (cur[3]) {
				break;
			}
		}
		if (x == 0)
			return 0;
		else if (x < min_right) {
			min_right = x;
		}
		ptr += r->stride;
	}
	return min_right;
}

static int
image_crop(lua_State *L) {
	size_t sz;
	const uint8_t * image = luaL_getbuffer(L, &sz);
	int x = luaL_checkinteger(L, 2);
	int y = luaL_checkinteger(L, 3);
	if (x * y * 4 != sz)
		return luaL_error(L, "Invalid image size %d * %d * 4 != %z\n", x, y, sz);
	int dx = luaL_optinteger(L, 4, 0);
	int dy = luaL_optinteger(L, 5, 0);
	int w = luaL_optinteger(L, 6, x - dx);
	int h = luaL_optinteger(L, 7, y - dy);
	
	struct rect r;

	if (!(rect_init(&r, image, x, y, dx, dy, w, h))) {
		return 0;
	}

	int top = remove_top(&r);
	if (top == h)
		return 0;
	remove_bottom(&r);
	int left = remove_left(&r);
	int right = remove_right(&r);
	// reserve border for alpha channel
	lua_pushinteger(L, left);
	lua_pushinteger(L, top);
	lua_pushinteger(L, r.width - (left + right));
	lua_pushinteger(L, r.line);

	return 4;
}

static uint8_t *
get_image_buffer(lua_State *L, int *w, int *h) {
	uint8_t * buffer = lua_touserdata(L, 1);
	if (buffer == NULL || !lua_getmetatable(L, 1))
		luaL_error(L, "Neet image userdata");
	if (lua_getfield(L, -1, "width") != LUA_TNUMBER) {
		luaL_error(L, "No .width");
	}
	int width = lua_tointeger(L, -1);
	lua_pop(L, 1);
	
	if (lua_getfield(L, -1, "height") != LUA_TNUMBER) {
		luaL_error(L, "No .height");
	}
	int height = lua_tointeger(L, -1);
	lua_pop(L, 1);
	
	int size = lua_rawlen(L, 1);
	if (width * height * 4 != size)
		luaL_error(L, "Invalid size %d * %d * 4 != %d", width, height, size);
	*w = width;
	*h = height;
	return buffer;
}

static int
limage_write_png(lua_State *L) {
	int width, height;
	uint8_t * buffer = get_image_buffer(L, &width, &height);
	
	const char * filename = luaL_checkstring(L, 2);
	if (!stbi_write_png(filename, width, height, 4, buffer, width * 4)) {
		return luaL_error(L, "Write %s failed", filename);
	}

	return 0;
}

struct canvas {
	void * buffer;
	int width;
	int height;
	int stride;
};

static int
limage_tocanvas(lua_State *L) {
	int width, height;
	uint8_t * buffer = get_image_buffer(L, &width, &height);
	struct canvas * c = (struct canvas *)lua_newuserdatauv(L, sizeof(*c), 1);
	lua_pushvalue(L, 1);
	lua_setiuservalue(L, -2, 1);
	c->buffer = buffer;
	c->width = width;
	c->height = height;
	c->stride = width * 4;

	return 1;
}

static int
image_new(lua_State *L) {
	int w = luaL_checkinteger(L, 1);
	int h = luaL_checkinteger(L, 2);
	uint8_t * buffer = (uint8_t *)lua_newuserdatauv(L, w * h * 4, 0);
	
	size_t sz = 0;
	const char *data = NULL;
	switch (lua_type(L, 3)) {
	case LUA_TSTRING:
		data = lua_tolstring(L, 3, &sz);
		break;
	case LUA_TLIGHTUSERDATA:
		data = (const char *)lua_touserdata(L, 3);
		sz = luaL_checkinteger(L, 4);
		break;
	case LUA_TUSERDATA:
		data = (const char *)lua_touserdata(L, 3);
		sz = lua_rawlen(L, 3);
		break;
	}
	if (sz == 0) {
		memset(buffer, 0, w * h * 4);
	} else if (sz == w*h*4) {
		memcpy(buffer, data, sz);
	} else if (sz == w*h) {
		int i,j;
		uint8_t *dst = buffer;
		const uint8_t *src = (const uint8_t *)data;
		for (i=0;i<h;i++) {
			for (j=0;j<w;j++) {
				uint8_t alpha = *src;
				dst[0] = dst[1] = dst[2] = dst[3] = alpha;
				++src;
				dst += 4;
			}
		}
	} else {
		return luaL_error(L, "Invalid image data size");
	}

	lua_newtable(L);

	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");

	lua_pushvalue(L, 1);
	lua_setfield(L, -2, "width");

	lua_pushvalue(L, 2);
	lua_setfield(L, -2, "height");

	lua_pushcfunction(L, limage_write_png);
	lua_setfield(L, -2, "write");

	lua_pushcfunction(L, limage_tocanvas);
	lua_setfield(L, -2, "canvas");

	lua_setmetatable(L, -2);
	return 1;
}

static int
image_canvas(lua_State *L) {
	int t = lua_type(L, 1);
	if (t != LUA_TSTRING && t != LUA_TUSERDATA && t != LUA_TLIGHTUSERDATA)
		return luaL_error(L, "Need buffer");
	int width = luaL_checkinteger(L, 2);
	int height = luaL_checkinteger(L, 3);
	int stride;
	int stack_top= lua_gettop(L);
	int x = 0;
	int y = 0;
	if (stack_top <= 4) {
		stride = luaL_optinteger(L, 4, width * 4);
	}  else {
		x = luaL_checkinteger(L, 4);
		y = luaL_checkinteger(L, 5);
		int w = luaL_checkinteger(L, 6);
		int h = luaL_checkinteger(L, 7);
		stride = width * 4;
		if (x < 0 || y < 0 || x+w > width || y+h > height) {
			return luaL_error(L, "Invalid rect (%d %d %d %d) in (%d %d)", x, y, w, h, width, height);
		}
		width = w;
		height = h;
	}
	struct canvas * c = (struct canvas *)lua_newuserdatauv(L, sizeof(*c), 1);
	lua_pushvalue(L, 1);
	lua_setiuservalue(L, -2, 1);
	if (t == LUA_TSTRING) {
		size_t sz;
		c->buffer = (void *)lua_tolstring(L, 1, &sz);
		if (stride * (y + height) > sz)
			return luaL_error(L, "Invalid buffer size %d * %d > %d", stride, (y + height), sz);
	} else {
		c->buffer = lua_touserdata(L, 1);
	}
	c->buffer = (void *)((char *)c->buffer + y * stride + x * 4);
	c->width = width;
	c->height = height;
	c->stride = stride;
	return 1;	
}

static int
check_canvas(lua_State *L, int index) {
	if (lua_type(L, index) != LUA_TUSERDATA)
		return luaL_error(L, "Need canvas");
	
	int t = lua_getiuservalue(L, index, 1);
	if (t != LUA_TSTRING && t != LUA_TUSERDATA && t != LUA_TLIGHTUSERDATA)
		return luaL_error(L, "Invalid canvas at %d", index);
	lua_pop(L, 1);
	return t;
}

static int
image_canvas_size(lua_State *L) {
	check_canvas(L, 1);
	struct canvas * c = (struct canvas *)lua_touserdata(L, 1);
	
	lua_pushinteger(L, c->width);
	lua_pushinteger(L, c->height);
	
	lua_pushlightuserdata(L, c->buffer);
	
	return 3;
}

static int
canvas_blit(lua_State *L) {
	if (check_canvas(L, 1) == LUA_TSTRING)
		return luaL_error(L, "dst canvas is readonly");
	check_canvas(L, 2);
	struct canvas * dst = (struct canvas *)lua_touserdata(L, 1);
	struct canvas * src = (struct canvas *)lua_touserdata(L, 2);
	int x = luaL_optinteger(L, 3, 0);
	int y = luaL_optinteger(L, 4, 0);
	int w = src->width;
	int h = src->height;
	int sx = 0;
	int sy = 0;
	if (x < 0) {
		w += x;
		sx = -x;
		x = 0;
	}
	if (y < 0) {
		h += y;
		sy = -y;
		y = 0;
	}
	if (x + w > dst->width) {
		w = dst->width - x;
	}
	if (y + h > dst->height) {
		h = dst->height - y;
	}
	if (w <=0 || h <= 0)
		return 0;

	int i;
	uint8_t * dst_ptr = (uint8_t *)dst->buffer + y * dst->stride + 4 * x;
	const uint8_t *src_ptr = (const uint8_t *)src->buffer + sy * src->stride + 4 * sx;
	for (i=0;i<h;i++) {
		memcpy(dst_ptr, src_ptr, w * 4);
		src_ptr += src->stride;
		dst_ptr += dst->stride;
	}
	
	return 0;
}

static int
image_makeindex(lua_State *L) {
	if (lua_isnoneornil(L, 1)) {
		lua_pushinteger(L, -1);
		return 1;
	}
	int x = luaL_checkinteger(L, 1);
	int y = luaL_checkinteger(L, 2);
	int w = luaL_checkinteger(L, 3);
	int h = luaL_checkinteger(L, 4);
	union {
		uint64_t index;
		uint16_t v[4];
	} u;
	u.v[0] = (uint16_t)x;
	u.v[1] = (uint16_t)y;
	u.v[2] = (uint16_t)w;
	u.v[3] = (uint16_t)h;
	lua_pushinteger(L, u.index);
	return 1;
}

static int
image_resize(lua_State *L) {
	size_t sz;
	const char *buffer = luaL_checklstring(L, 1, &sz);
	int x = luaL_checkinteger(L, 2);
	int y = luaL_checkinteger(L, 3);
	float scale_x = luaL_checknumber(L, 4);
	float scale_y = luaL_optnumber(L, 5, scale_x);
	int channel;
	size_t output_sz;
	int tx = (int)(x * scale_x + 0.5);
	int ty = (int)(y * scale_y + 0.5);
	int stride_p;
	if (x * y * 4 == sz) {
		channel = STBIR_4CHANNEL;
		output_sz =  tx * ty * 4;
		stride_p = 4;
	} else {
		if (x * y != sz) {
			return luaL_error(L, "Invalid size (%d) != %d * %d", (int)sz, x, y);
		}
		channel = STBIR_1CHANNEL;
		output_sz = tx * ty;
		stride_p = 1;
	}
	unsigned char *output = (unsigned char *)malloc(output_sz +1);
	if (output == NULL)
		return luaL_error(L, "Out of memory");
	output[output_sz] = 0;
	memcpy(output, buffer, output_sz);

	stbir_resize_uint8_linear((const unsigned char *)buffer , x , y, stride_p * x, output, tx, ty, stride_p * tx, channel);
	lua_pushexternalstring(L, (const char *)output, output_sz, free_buffer, NULL);
	lua_pushinteger(L, tx);
	lua_pushinteger(L, ty);
	return 3;
}

int
luaopen_image(lua_State *L) {
	luaL_checkversion(L);
	luaL_Reg l[] = {
		{ "load", image_load },
		{ "load_alpha", image_load_alpha },
		{ "resize", image_resize },
		{ "info", image_info },
		{ "crop", image_crop },
		{ "canvas", image_canvas },
		{ "canvas_size", image_canvas_size },
		{ "new", image_new },
		{ "blit", canvas_blit },
		{ "makeindex", image_makeindex },
		{ NULL, NULL },
	};
	luaL_newlib(L, l);
	return 1;
}
