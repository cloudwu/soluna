#include "spritemgr.h"
#include "transform.h"

#include <stdint.h>
#include <string.h>
#include <lua.h>
#include <lauxlib.h>

#define DEFAULT_TEXTURE_SIZE 4096
#define INVALID_TEXTUREID 0xffff

#define MAX_NODE 8192

#define STB_RECT_PACK_IMPLEMENTATION
#include "stb/stb_rect_pack.h"

static int
lbank_add(lua_State *L) {
	struct sprite_bank *b = (struct sprite_bank *)luaL_checkudata(L, 1, "SOLUNA_SPRITEBANK");
	if (b->n >= b->cap) {
		return luaL_error(L, "Too many sprite (%d)", b->n);
	}
	struct sprite_rect *r = &b->rect[b->n++];
	int w = luaL_checkinteger(L, 2);
	int h = luaL_checkinteger(L, 3);
	int dx = luaL_optinteger(L, 4, 0);
	int dy = luaL_optinteger(L, 5, 0);
	if (w <= 0 || w > 0xffff || h <=0 || h >= 0xffff)
		return luaL_error(L, "Invalid sprite size (%d * %d)", w, h);
	if (dx < -0x8000 || dx > 0x7fff || dy < -0x8000 || dy > 0x7ffff)
		return luaL_error(L, "Invalid sprite offset (%d * %d)", dx, dy);
	r->u = w;
	r->v = h;
	r->off = (dx + 0x8000) << 16 | (dy + 0x8000);
	r->texid = luaL_optinteger(L, 6, INVALID_TEXTUREID);
	lua_pushinteger(L, b->n);
	return 1;
}

static int
pack_sprite(struct sprite_bank *b, stbrp_context *ctx, stbrp_node *tmp, stbrp_rect *srect, int from, int reserved, int *reserved_n) {
	int last_texid = b->texture_n;
	
	stbrp_init_target(ctx, b->texture_size, b->texture_size, tmp, MAX_NODE);
	int i;
	int rect_i = reserved;
	for (i=from;i<b->n;i++) {
		struct sprite_rect *rect = &b->rect[i];
		if (rect->texid == INVALID_TEXTUREID || rect->texid == last_texid) {
			rect->texid = last_texid;
			stbrp_rect * sr = &srect[rect_i++];
			sr->id = i;
			// reserve 1 pixel border
			sr->w = (rect->u + 1) & 0xffff;
			sr->h = (rect->v + 1) & 0xffff;
			if (sr->w > b->texture_size || sr->h > b->texture_size) {
				return -1;
			}
		}
	}
	if (stbrp_pack_rects(ctx, srect, rect_i)) {
		// succ
		int j;
		for (j=0;j<rect_i;j++) {
			stbrp_rect * sr = &srect[j];
			struct sprite_rect *rect = &b->rect[sr->id];
			rect->u = sr->x << 16 | (sr->w - 1);
			rect->v = sr->y << 16 | (sr->h - 1);
			rect->texid = last_texid;
		}
		*reserved_n = 0;
	} else {
		// pack a part
		int j;
		int n = 0;
		for (j=0;j<rect_i;j++) {
			stbrp_rect * sr = &srect[j];
			struct sprite_rect *rect = &b->rect[sr->id];
			if (sr->was_packed) {
				rect->u = sr->x << 16 | (sr->w - 1);
				rect->v = sr->y << 16 | (sr->h - 1);
				rect->texid = last_texid;
			} else {
				stbrp_rect * tmp = &srect[n];
				tmp->w = sr->w;
				tmp->h = sr->h;
				tmp->id = sr->id;
				++n;
			}
		}
		*reserved_n = n;
	}
	return i;
}

struct tmp_context {
	stbrp_context ctx;
	stbrp_node tmp[MAX_NODE];
	stbrp_rect rect[1];// = malloc(sizeof(*rect) * b->n);
};

static inline struct tmp_context *
alloc_context(int n) {
	struct tmp_context * r = (struct tmp_context *)malloc(sizeof(*r) + (n - 1) * sizeof(r->rect));
	return r;
}

static inline void
free_context(struct tmp_context *p) {
	free(p);
}

static int
lbank_pack(lua_State *L) {
	struct sprite_bank *b = (struct sprite_bank *)luaL_checkudata(L, 1, "SOLUNA_SPRITEBANK");
	struct tmp_context *ctx = alloc_context(b->n);
	int limit = luaL_optinteger(L, 2, INVALID_TEXTUREID);

	int texture = b->texture_n;
	int from = 0;
	int reserved = 0;
	for (;;) {
		if (b->texture_n >= limit) {
			return luaL_error(L, "Too many textures %d", texture);
		}
		from = pack_sprite(b, &ctx->ctx, ctx->tmp, ctx->rect, from, reserved, &reserved);
		if (from < 0) {
			return luaL_error(L, "sprite image is larger than texture");
		}
		if (reserved == 0 && from >= b->n) {
			break;
		}
		++b->texture_n;
	}
	free_context(ctx);

	lua_pushinteger(L, texture);
	lua_pushinteger(L, b->texture_n - texture + 1);

	return 2;
}

static int
lbank_altas(lua_State *L) {
	struct sprite_bank *b = (struct sprite_bank *)luaL_checkudata(L, 1, "SOLUNA_SPRITEBANK");
	int tid = luaL_checkinteger(L, 2);
	int i;
	lua_newtable(L);
	for (i=0;i<b->n;i++) {
		struct sprite_rect *rect = &b->rect[i];
		if (rect->texid == tid) {
			uint64_t x = rect->u >> 16;
			uint64_t y = rect->v >> 16;
			uint64_t v = x << 32 | y;
			lua_pushinteger(L, v);
			lua_rawseti(L, -2, i + 1);
		}
	}
	return 1;
}

static int
lbank_ptr(lua_State *L) {
	struct sprite_bank *b = (struct sprite_bank *)luaL_checkudata(L, 1, "SOLUNA_SPRITEBANK");
	lua_pushlightuserdata(L, b);
	return 1;
}

static int
lsprite_newbank(lua_State *L) {
	int cap = luaL_checkinteger(L, 1);
	int texture_size = luaL_optinteger(L, 2, DEFAULT_TEXTURE_SIZE);
	struct sprite_bank *b = (struct sprite_bank *)lua_newuserdatauv(L, sizeof(*b) + (cap-1) * sizeof(b->rect[0]), 0);
	b->n = 0;
	b->cap = cap;
	b->texture_size = texture_size;
	b->texture_n = 0;
	
	if (luaL_newmetatable(L, "SOLUNA_SPRITEBANK")) {
		luaL_Reg l[] = {
			{ "__index", NULL },
			{ "add", lbank_add },
			{ "pack", lbank_pack },
			{ "altas", lbank_altas },
			{ "ptr", lbank_ptr },
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
luaopen_spritemgr(lua_State *L) {
	luaL_checkversion(L);
	luaL_Reg l[] = {
		{ "newbank", lsprite_newbank },
		{ NULL, NULL },
	};
	luaL_newlib(L, l);
	sprite_transform_init();
	return 1;
}
