#include "spritemgr.h"
#include "sprite_submit.h"
#include "batch.h"
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
	r->texid = INVALID_TEXTUREID;
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

	int texture = b->texture_n;
	int from = 0;
	int reserved = 0;
	for (;;) {
		from = pack_sprite(b, &ctx->ctx, ctx->tmp, ctx->rect, from, reserved, &reserved);
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

struct layer {
	float s;
	float r;
	float x;
	float y;
};

struct batch {
	int n;
	int layer;
	int layer_cap;
	struct transform trans;
	struct draw_batch *b;
	struct layer *stack;
};

static int
lbatch_reset(lua_State *L) {
	struct batch *b = (struct batch *)luaL_checkudata(L, 1, "SOLUNA_BATCH");
	b->n = 0;
	b->layer = 0;
	sprite_transform_identity(&b->trans);
	return 0;
}

static int
lbatch_release(lua_State *L) {
	struct batch *b = (struct batch *)luaL_checkudata(L, 1, "SOLUNA_BATCH");
	b->n = 0;
	b->n = 0;
	batch_delete(b->b);
	b->b = NULL;
	return 0;
}

static struct draw_primitive *
batch_add_sprite(lua_State *L, struct batch *b) {
	int n = b->n;
	struct draw_primitive * p = batch_reserve(b->b, n + 1);
	if (p == NULL)
		luaL_error(L, "batch_add_sprite : Out of memory, n = %d", n);
	
	p += n;
	
	int id = luaL_checkinteger(L, 2);
	if (id <= 0)
		luaL_error(L, "Invalid sprite id %d", id);

	p->x = 0;
	p->y = 0;
	p->sr = 0;
	p->sprite = id;
	b->n = n + 1;

	return p;
}

static struct draw_primitive *
batch_add_material(lua_State *L, struct batch *b) {
	int n = b->n;
	struct draw_primitive * p = batch_reserve(b->b, n + 2);
	if (p == NULL)
		luaL_error(L, "batch_add_material : Out of memory, n = %d", n);

	p += n;
	
	if (lua_getiuservalue(L, 2, 1) != LUA_TNUMBER)
		luaL_error(L, "Invalid material object");
	int matid = lua_tointeger(L, -1);
	if (matid <= 0)
		luaL_error(L, "Invalid material id %d", matid);
	lua_pop(L, 1);

	p->x = 0;
	p->y = 0;
	p->sr = 0;
	p->sprite = -matid;

	int sz = lua_rawlen(L, 2);
	if (sz > sizeof(struct draw_primitive))
		luaL_error(L, "Invalid material object size (%d > %d)", sz, sizeof(struct draw_primitive));
	
	memcpy(p+1, lua_touserdata(L, 2), sz);

	b->n = n + 2;

	return p;
}

static struct draw_primitive *
batch_add_stream(lua_State *L, struct batch *b, int *count) {
	int n = b->n;
	size_t sz = 0;
	const char * data = luaL_checklstring(L, 2, &sz);
	if (sz == 0)
		return NULL;
	*count = sz / 2 / sizeof(struct draw_primitive);
	if (*count * 2 * sizeof(struct draw_primitive) != sz)
		luaL_error(L, "Invalid stream size (%d)", sz);
	struct draw_primitive * p = batch_reserve(b->b, n + 2 * *count);
	if (p == NULL) {
		luaL_error(L, "batch_add_stream : Out of memory n = %d count = %d", n, *count);
	}
	p += n;
	b->n = n + *count * 2;
	memcpy(p, data, *count * 2 * sizeof(struct draw_primitive));
	return p;
}

static int
lbatch_add(lua_State *L) {
	struct batch *b = (struct batch *)luaL_checkudata(L, 1, "SOLUNA_BATCH");
	struct draw_primitive *p;
	int n = 1;
	switch (lua_type(L, 2)) {
	case LUA_TNUMBER:
		p = batch_add_sprite(L, b);
		break;
	case LUA_TUSERDATA:
		p = batch_add_material(L, b);
		break;
	case LUA_TSTRING:
		p = batch_add_stream(L, b, &n);
		if (p == NULL)
			return 0;
		break;
	default:
		return luaL_error(L, "Invalid type %s", lua_typename(L, lua_type(L, 2)));
	}
	float x = luaL_optnumber(L, 3, 0);
	float y = luaL_optnumber(L, 4, 0);
	
	int i;
	
	for (i=0;i<n;i++) {
		sprite_apply_xy(p, x, y);
		sprite_transform_apply(p, &b->trans);
		p+=2;
	}
	return 0;
}

static int
lbatch_ptr(lua_State *L) {
	struct batch *b = (struct batch *)luaL_checkudata(L, 1, "SOLUNA_BATCH");
	int offset = luaL_optinteger(L, 2, 0);
	struct draw_primitive * p = batch_reserve(b->b, 0);
	if (offset >= b->n)
		return 0;
	p += offset;
	int n = b->n - offset;
	lua_pushlightuserdata(L, p);
	lua_pushinteger(L, n);
	return 2;
}

static void
layer_close(lua_State *L, struct batch *b) {
	if (b->layer <= 0)
		luaL_error(L, "none layer to close");
	if (b->layer == 1) {
		b->layer = 0;
		sprite_transform_identity(&b->trans);
	} else {
		--b->layer;
		struct layer *current = &b->stack[b->layer-1];
		sprite_transform_set(&b->trans, current->s, current->r, current->x, current->y);
	}
}

static struct layer *
layer_new(lua_State *L, struct batch *b) {
	if (b->layer >= b->layer_cap) {
		int cap = (b->layer + 1) * 3 / 2;
		struct layer * tmp = (struct layer *)lua_newuserdatauv(L, cap * sizeof(struct layer), 0);
		memcpy(tmp, b->stack, b->layer_cap * sizeof(struct layer));
		b->stack = tmp;
		b->layer_cap = cap;
		lua_setiuservalue(L, 1, 1);
	}
	struct layer * ret = &b->stack[b->layer];
	++b->layer;
	return ret;
}

static void 
layer_merge(struct layer *current, struct layer *last) {
	float x, y;
	if (last->r != 0) {
		float sinv = sinf(last->r);
		float cosv = cosf(last->r);
		y = current->y * cosv + current->x * sinv;
		x = current->x * cosv - current->y * sinv;
		current->r += last->r;
	} else {
		x = current->x;
		y = current->y;
	}
	if (last->s != 1) {
		x *= last->s;
		y *= last->s;
		current->s *= last->s;
	}
	current->x = x + last->x;
	current->y = y + last->y;
}

static int
lbatch_layer(lua_State *L) {
	struct batch *b = (struct batch *)luaL_checkudata(L, 1, "SOLUNA_BATCH");
	struct layer *new_layer = layer_new(L, b);
	switch (lua_gettop(L)) {
	case 1:
		// close layer
		--b->layer;
		layer_close(L, b);
		return 0;
	case 2:
		// rot only
		new_layer->s = 1;
		new_layer->r = luaL_checknumber(L, 2);
		new_layer->x = 0;
		new_layer->y = 0;
		break;
	case 3:
		// trans only
		new_layer->s = 1;
		new_layer->r = 0;
		new_layer->x = luaL_checknumber(L, 2);
		new_layer->y = luaL_checknumber(L, 3);
		break;
	case 4:
		// st, no rot
		new_layer->s = luaL_checknumber(L, 2);
		new_layer->r = 0;
		new_layer->x = luaL_checknumber(L, 3);
		new_layer->y = luaL_checknumber(L, 4);
		break;
	case 5:
		// srt
		new_layer->s = luaL_checknumber(L, 2);
		new_layer->r = luaL_checknumber(L, 3);
		new_layer->x = luaL_checknumber(L, 4);
		new_layer->y = luaL_checknumber(L, 5);
		break;
	default:
		luaL_error(L, "Too many arguments");
	}
	if (new_layer->s == 0)
		luaL_error(L, "Scale can't be 0");
	if (b->layer > 1) {
		layer_merge(new_layer, new_layer-1);
	}
	sprite_transform_set(&b->trans, new_layer->s, new_layer->r, new_layer->x, new_layer->y);
	return 0;
}

static int
lbatch_point(lua_State *L) {
	struct batch *b = (struct batch *)luaL_checkudata(L, 1, "SOLUNA_BATCH");
	if (b->layer == 0) {
		lua_settop(L, 3);
		return 2;
	}
	float x = luaL_checknumber(L, 2);
	float y = luaL_checknumber(L, 3);
	
	// to .8 fix number
	int fx = (int)(x * 256);
	int fy = (int)(y * 256);
	
	sprite_transform_point(&b->trans, &fx, &fy);
	
	lua_pushnumber(L, (float)fx / 256.0f);
	lua_pushnumber(L, (float)fy / 256.0f);
	return 2;
}

static int
lsprite_newbatch(lua_State *L) {
	struct batch *b = (struct batch *)lua_newuserdatauv(L, sizeof(*b), 1);
	b->n = 0;
	b->b = batch_new(0);
	if (b->b == NULL)
		return luaL_error(L, "sprite_newbatch : Out of memory");
	b->layer = 0;
	b->layer_cap = 0;
	b->stack = NULL;
	sprite_transform_identity(&b->trans);
		
	if (luaL_newmetatable(L, "SOLUNA_BATCH")) {
		luaL_Reg l[] = {
			{ "__index", NULL },
			{ "reset", lbatch_reset },
			{ "add", lbatch_add },
			{ "ptr", lbatch_ptr },
			{ "release", lbatch_release },
			{ "layer", lbatch_layer },
			{ "point", lbatch_point },
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
		{ "newbatch", lsprite_newbatch },
		{ NULL, NULL },
	};
	luaL_newlib(L, l);
	sprite_transform_init();
	return 1;
}
