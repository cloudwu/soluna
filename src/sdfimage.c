#include <lua.h>
#include <lauxlib.h>
#include <math.h>
#include <string.h>

#include "font_define.h"

// implement from image.c
#include "stb/stb_image.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb/stb_image_resize2.h"

#include "stb/stb_image_write.h"

#include "luabuffer.h"

#define MAX_SIZE 4096
#define INF 1e20
#define SDF_RADIUS 8
#define SDF_CUTOFF 0.25
// the same with font glyph size
#define IMAGE_SIZE 64

// 1D squared distance transform
static void
edt1d(const double *f, double *d, int *v, double *z, int n) {
	int q,k;
	
	v[0] = 0;
	z[0] = -INF;
	z[1] = +INF;

	for (q = 1, k = 0; q < n; q++) {
		double s = ((f[q] + q * q) - (f[v[k]] + v[k] * v[k])) / (2 * q - 2 * v[k]);
		while (s <= z[k]) {
			k--;
			s = ((f[q] + q * q) - (f[v[k]] + v[k] * v[k])) / (2 * q - 2 * v[k]);
		}
		k++;
		v[k] = q;
		z[k] = s;
		z[k + 1] = +INF;
	}

	for (q = 0, k = 0; q < n; q++) {
        while (z[k + 1] < q)
			k++;
		d[q] = (q - v[k]) * (q - v[k]) + f[v[k]];
    }
}

// 2D Euclidean distance transform by Felzenszwalb & Huttenlocher https://cs.brown.edu/~pff/dt/
static void
edt(double *data, int width, int height, double *f, double *d, int *v, double *z) {
	int x,y;
	for (x = 0; x < width; x++) {
		for (y = 0; y < height; y++) {
			f[y] = data[y * width + x];
		}
		edt1d(f, d, v, z, height);
		for (y = 0; y < height; y++) {
			data[y * width + x] = d[y];
		}
    }
	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			f[x] = data[y * width + x];
		}
		edt1d(f, d, v, z, width);
		for (x = 0; x < width; x++) {
			data[y * width + x] = sqrt(d[x]);
		}
	}
}

struct sdf_context {
	double f[MAX_SIZE];
	double d[MAX_SIZE];
	double z[MAX_SIZE+1];
	int v[MAX_SIZE];
};

static void
sdf_convert(unsigned char *bytes, int x, int y, double radius, double cutoff) {
	int i;
	assert(x <= MAX_SIZE && y <= MAX_SIZE);
	int length = x * y;
	double *data = (double *)malloc(length * sizeof(double) * 3 + sizeof(struct sdf_context));
	for (i = 0; i < length; i++) {
		// For white background, negative image
		data[i] = (255 - bytes[i]) / 255.0;
	}
	double *gridOuter = data + length;
	double *gridInner = gridOuter + length;
	struct sdf_context * ctx = (struct sdf_context *)(gridInner + length);

	for (i = 0; i < length; i++) {
		double a = data[i];
		if (a >= 0.5) {
			gridOuter[i] = 0;
			if (a >= 1) {
				gridInner[i] = INF;
			} else {
				a -= 0.5;
				gridInner[i] = a * a;
			}
		} else if (a <= 0.5) {
			gridInner[i] = 0;
			if (a <= 0) {
				gridOuter[i] = INF;
			} else {
				a = 0.5 - a;
				gridOuter[i] = a * a;
			}
		}
	}

    edt(gridOuter, x, y, ctx->f, ctx->d, ctx->v, ctx->z);
    edt(gridInner, x, y, ctx->f, ctx->d, ctx->v, ctx->z);

	for (i = 0; i < length; i++) {
		double v = (gridOuter[i] - gridInner[i]) / radius + cutoff;
		if (v <= 0) {
			bytes[i] = 255;
		} else if (v >= 1) {
			bytes[i] = 0;
		} else {
			unsigned char byte = (unsigned char)(v * 255.0 + 0.5);
			bytes[i] = 255 - byte;
		}
    }

	free(data);
}

static void *
free_image(void *ud, void *ptr, size_t osize, size_t nsize) {
	stbi_image_free(ptr);
	return NULL;
}

static void *
free_resizeimage(void *ud, void *ptr, size_t osize, size_t nsize) {
	free(ptr);
	return NULL;
}

static int
image_loadsdf(lua_State *L) {
	size_t sz;
	const stbi_uc *buffer = luaL_getbuffer(L, &sz);
	int x,y;
	stbi_uc * img = stbi_load_from_memory(buffer, sz, &x, &y, NULL, 1);
	if (img == NULL) {
		lua_pushnil(L);
		lua_pushstring(L, stbi_failure_reason());
		return 2;
	}
	
	int target_w = luaL_optinteger(L, 2, IMAGE_SIZE);
	int target_h = luaL_optinteger(L, 3, target_w);
	if (target_w <= 0 || target_w > MAX_SIZE || target_h <= 0 || target_w > MAX_SIZE) {
		return luaL_error(L, "Invalid target size %d * %d", target_w, target_h);
	}

	double radius = (double)x * SDF_RADIUS / target_w;
	sdf_convert(img, x, y, radius, SDF_CUTOFF);
	
	if (x == target_w && y == target_h) {
		lua_pushexternalstring(L, (const char *)img, x * y, free_image, NULL);
	} else {
		size_t size = target_w * target_h;
		unsigned char * target = (unsigned char *)malloc(size + 1);
		if (target == NULL) {
			stbi_image_free(img);
			return luaL_error(L, "Out of memory for image");
		}
		target[size] = 0;
		stbir_resize_uint8_linear(img , x , y, x,
			target, target_w, target_h, target_w, STBIR_1CHANNEL);
		stbi_image_free(img);
		lua_pushexternalstring(L, (const char *)target, size, free_resizeimage, NULL);
	}

	return 1;
}

static int
image_savesdf(lua_State *L) {
	const char * filename = luaL_checkstring(L, 1);
	size_t sz;
	const char * buffer = luaL_checklstring(L, 2, &sz);
	int x = luaL_optinteger(L, 3, IMAGE_SIZE);
	int y = luaL_optinteger(L, 4, x);
	if (x * y != sz) {
		return luaL_error(L, "Invalid image size %d * %d", x, y);
	}
	if (!stbi_write_png(filename, x, y, 1, buffer, x)) {
		return luaL_error(L, "Write %s failed", filename);
	}
	return 1;
}

static int
icon_bundle(lua_State *L) {
	luaL_checktype(L, 1, LUA_TTABLE);
	int n = lua_rawlen(L, 1);
	size_t sz = n * FONT_MANAGER_GLYPHSIZE * FONT_MANAGER_GLYPHSIZE;
	void * buffer = lua_newuserdatauv(L, sz, 0);
	int i;
	for (i=0; i<n; i++) {
		// import icon i
		if (lua_geti(L, 1, i+1) != LUA_TSTRING) {
			return luaL_error(L, "Invalid icon %d (%s)", i+1, lua_typename(L, lua_type(L, -1)));
		}
		size_t icon_size;
		const char *icon = lua_tolstring(L, -1, &icon_size);
		if (icon_size != FONT_MANAGER_GLYPHSIZE * FONT_MANAGER_GLYPHSIZE) {
			return luaL_error(L, "Invalid icon %d size, need %d * %d bytes", i+1, FONT_MANAGER_GLYPHSIZE, FONT_MANAGER_GLYPHSIZE);
		}
		unsigned char *ptr = (unsigned char *)buffer + i * FONT_MANAGER_GLYPHSIZE * FONT_MANAGER_GLYPHSIZE;
		memcpy(ptr, icon, FONT_MANAGER_GLYPHSIZE * FONT_MANAGER_GLYPHSIZE);
		lua_pop(L, 1);
	}
	return 1;
}

int
luaopen_image_sdf(lua_State *L) {
	luaL_checkversion(L);
	luaL_Reg l[] = {
		{ "bundle", icon_bundle },
		{ "load", image_loadsdf },
		{ "save", image_savesdf },	// for debug
		{ NULL, NULL },
	};
	luaL_newlib(L, l);
	return 1;
}

