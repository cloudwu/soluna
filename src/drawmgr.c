#include <lua.h>
#include <lauxlib.h>
#include <stdint.h>

#include "sokol/sokol_gfx.h"
#include "sokol/sokol_glue.h"
#include "sokol/sokol_app.h"

#include "batch.h"
#include "spritemgr.h"
#include "render_pass.h"

#define PASS_TAG (-1)

struct draw_element {
	struct draw_primitive * base;
	int n;	// >=0
	int material;
	int texture;
};

struct draw_pass {
	struct render_pass *pass;
	int pass_tag;	// PASS_TAG (-1)
	int pass_n;		// n elements
};

union draw_union {
	struct draw_element element;
	struct draw_pass pass;
};

struct drawmgr {
	struct sprite_bank *bank;
	int cap;
	int n;
	int bank_n;
	int last_pass;
	union draw_union data[1];
};

static int
ldrawmgr_index(lua_State *L) {
	struct drawmgr * d = lua_touserdata(L, 1);
	int idx = luaL_checkinteger(L, 2);
	if (idx < 1 || idx >= d->n) {
		return 0;
	}
	struct draw_element *e = &(d->data[idx].element);
	lua_pushinteger(L, e->material);
	lua_pushlightuserdata(L, e->base);
	lua_pushinteger(L, e->n);
	if (e->texture >= 0) {
		lua_pushinteger(L, e->texture);
		return 4;
	} else {
		return 3;
	}
}

static int
ldrawmgr_reset(lua_State *L) {
	struct drawmgr * d = (struct drawmgr *)luaL_checkudata(L, 1, "SOLUNA_DRAWMGR");
	d->n = 0;
	d->bank_n = d->bank->n;
	d->last_pass = 0;
	return 0;
}

static int
append_external_material(struct drawmgr * d, struct draw_primitive *base, int n, int matid, int texid) {
	int i;
	struct sprite_rect * rect = d->bank->rect;

	for (i=1;i<n;i++) {
		if (base[i*2].sprite != matid) {
			break;
		}
		struct draw_primitive_external * ext = (struct draw_primitive_external *)&base[i*2+1];
		int sprite = ext->sprite;
		if (sprite >= 0 && rect[sprite].texid != texid) {
			break;
		}
	}
	struct draw_element *e = &d->data[d->n++].element;
	e->base = base;
	e->n = i;
	e->material = -matid;
	e->texture = texid;
	return i;
}

static int
append_default_material(struct drawmgr * d, struct draw_primitive *base, int n, int texid) {
	int i;
	struct sprite_rect * rect = d->bank->rect;
	int rect_n = d->bank_n;

	for (i=1;i<n;i++) {
		int sprite = base[i].sprite;
		if (sprite <= 0 || sprite > rect_n)
			break;
		--sprite;
		if (texid != rect[sprite].texid)
			break;
	}
	struct draw_element *e = &d->data[d->n++].element;
	e->base = base;
	e->n = i;
	e->material = 0;
	e->texture = texid;
	return i;
}

static int
ldrawmgr_append(lua_State *L) {
	struct drawmgr * d = (struct drawmgr *)luaL_checkudata(L, 1, "SOLUNA_DRAWMGR");
	
	struct draw_primitive *prim = (struct draw_primitive *)lua_touserdata(L, 2);
	int prim_n = luaL_checkinteger(L, 3);
	
	struct sprite_rect * rect = d->bank->rect;
	int rect_n = d->bank_n;

	int i;
	struct draw_primitive *end_ptr = &prim[prim_n];
	for (i=0;i<prim_n;) {
		struct draw_primitive *p = &prim[i];
		int index = p->sprite;
		if (d->n >= d->cap) {
			return luaL_error(L, "Too many draw");
		}
		if (index <= 0) {
			if (i == prim_n || index == 0) {
				return luaL_error(L, "Invalid batch stream");
			}
			struct draw_primitive_external * ext = (struct draw_primitive_external *)&prim[i+1];
			int sprite = ext->sprite;
			int texid = -1;
			if (sprite >= 0) {
				texid = rect[sprite].texid;
			}
			i += append_external_material(d, p, (end_ptr - p)/2, index, texid) * 2;
		} else {
			--index;
			if (index >= rect_n)
				return luaL_error(L, "Invalid sprite id %d", index);
			int texid = rect[index].texid;
			i += append_default_material(d, p, end_ptr - p, texid);
		}
	}

	return 0;
}

static int
ldrawmgr_newpass(lua_State *L) {
	struct drawmgr * d = (struct drawmgr *)luaL_checkudata(L, 1, "SOLUNA_DRAWMGR");
	if (d->n >= d->cap) {
		return luaL_error(L, "Too many draw");
	}
	struct draw_pass *p = &d->data[d->n].pass;
	p->pass = lua_touserdata(L, 2);
	p->pass_tag = PASS_TAG;
	p->pass_n = -1;
	++d->n;
	if (d->last_pass == 0 && d->n == 1) {
		// the first pass
		return 0;
	}
	if (d->last_pass >= d->n)
		return luaL_error(L, "Invalid last pass index %d", d->last_pass);
	struct draw_pass *last_p = &d->data[d->last_pass].pass;
	if (last_p->pass_tag != PASS_TAG)
		return luaL_error(L, "Invalid last pass");
	last_p->pass_n = d->n - d->last_pass - 2;
	d->last_pass = d->n - 1;
	return 0;
}

static void
begin_pass(struct render_pass *p) {
	if (p->pass.swapchain.invalid) {
		p->pass.swapchain = sglue_swapchain();
		sg_begin_pass(&p->pass);
		p->pass.swapchain.invalid = true;
	} else {
		sg_begin_pass(&p->pass);
	}
}

static int
ldrawmgr_beginpass(lua_State *L) {
	struct drawmgr * d = (struct drawmgr *)luaL_checkudata(L, 1, "SOLUNA_DRAWMGR");
	int offset = luaL_optinteger(L, 2, 0);
	if (offset >= d->n) {
		return 0;
	}
	struct draw_pass *p = &d->data[offset].pass;
	if (p->pass_tag != PASS_TAG) {
		return luaL_error(L, "[%d] is not a draw_pass", offset);
	}
	begin_pass(p->pass);
	lua_pushlightuserdata(L, p->pass);
	lua_pushinteger(L, offset + 1);
	int to;
	if (p->pass_n < 0) {
		to = d->n - 1;
	} else {
		to = offset + p->pass_n;
	}
	lua_pushinteger(L, to);
	return 3;
}

static int
ldrawmgr_endpass(lua_State *L) {
	sg_end_pass();
	return 0;
}

static int
ldrawmgr_new(lua_State *L) {
	luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
	void * bank = lua_touserdata(L, 1);
	int cap = luaL_checkinteger(L, 2);
	struct drawmgr * d = (struct drawmgr *)lua_newuserdatauv(L, sizeof(*d) + (cap-1)*sizeof(d->data[0]), 0);
	d->bank = (struct sprite_bank *)bank;
	d->cap = cap;
	d->n = 0;

	if (luaL_newmetatable(L, "SOLUNA_DRAWMGR")) {
		luaL_Reg l[] = {
			{ "__index", NULL },
			{ "__call", ldrawmgr_index },
			{ "reset", ldrawmgr_reset },
			{ "append", ldrawmgr_append },
			{ "newpass", ldrawmgr_newpass },
			{ "beginpass", ldrawmgr_beginpass },
			{ "endpass", ldrawmgr_endpass },
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
luaopen_drawmgr(lua_State *L) {
	luaL_checkversion(L);
	luaL_Reg l[] = {
		{ "new", ldrawmgr_new },
		{ NULL, NULL },
	};
	luaL_newlib(L, l);
	return 1;
}
