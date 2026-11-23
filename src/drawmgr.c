#include <lua.h>
#include <lauxlib.h>
#include <stdint.h>
#include <string.h>

#include "sokol/sokol_gfx.h"

#include "batch.h"
#include "spritemgr.h"
#include "render_target.h"
#include "render_batch.h"

#define PASS_TAG (-1)

struct draw_element {
	struct draw_primitive * base;
	int n;	// >=0
	int material;
	int texture;
};

struct draw_pass {
	struct render_target *rt;
	int pass_tag;	// PASS_TAG (-1)
	int pass_n;		// n elements
};

union draw_union {
	struct draw_element element;
	struct draw_pass pass;
};

struct drawmgr {
	struct render_frame frame;
	struct sprite_bank *bank;
	int cap;
	int n;
	int bank_n;
	int last_pass;
	int diff_pass;
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
	d->diff_pass = 0;
	d->frame.frame_count++;
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

static void
append_draw(lua_State *L, struct drawmgr * d, struct draw_primitive *prim, int prim_n) {
	int i;
	struct sprite_rect *rect = d->bank->rect;
	int rect_n = d->bank_n;
	struct draw_primitive *end_ptr = &prim[prim_n];
	for (i=0;i<prim_n;) {
		struct draw_primitive *p = &prim[i];
		int index = p->sprite;
		if (d->n >= d->cap) {
			luaL_error(L, "Too many draw");
		}
		if (index <= 0) {
			if (i == prim_n || index == 0) {
				luaL_error(L, "Invalid batch stream");
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
				luaL_error(L, "Invalid sprite id %d", index);
			int texid = rect[index].texid;
			i += append_default_material(d, p, end_ptr - p, texid);
		}
	}
}

static int
ldrawmgr_append(lua_State *L) {
	struct drawmgr * d = (struct drawmgr *)luaL_checkudata(L, 1, "SOLUNA_DRAWMGR");
	struct render_command_buffer *cb = (struct render_command_buffer *)lua_touserdata(L, 2);
	luaL_checktype(L, 3, LUA_TTABLE);	// pass table
	struct batch *b = &cb->b;
	struct draw_primitive * prim = batch_reserve(b->b, 0);
	int i;
	int n = cb->command_n;
	for (i=0;i<n;i++) {
		if (d->n >= d->cap) {
			return luaL_error(L, "Too many draw");
		}
		struct draw_pass *p = &d->data[d->n].pass;
		int pass_id = cb->command[i].pass_id;
		if (lua_geti(L, 3, pass_id) != LUA_TUSERDATA) {
			return luaL_error(L, "pass %d is missing", pass_id);
		}
		p->rt = lua_touserdata(L, -1);
		lua_pop(L, 1);
		if (d->n > 0 && d->diff_pass == 0) {
		   struct draw_pass *first = &d->data[0].pass;
		   if (first->rt != p->rt) {
				d->diff_pass = d->n;
			}
		}
		p->pass_tag = PASS_TAG;
		p->pass_n = -1;
		++d->n;
		if (d->n > 1) {
			if (d->last_pass >= d->n)
				return luaL_error(L, "Invalid last pass index %d", d->last_pass);
			struct draw_pass *last_p = &d->data[d->last_pass].pass;
			if (last_p->pass_tag != PASS_TAG)
				return luaL_error(L, "Invalid last pass");
			last_p->pass_n = d->n - d->last_pass - 2;
			d->last_pass = d->n - 1;
		}
		int start = cb->command[i].batch_start;
		int endindex = (i+1 >= n) ? b->n : cb->command[i+1].batch_start;
		append_draw(L, d, prim + start, endindex - start);
	}
	return 0;
}

static int
ldrawmgr_nextpass(lua_State *L) {
	struct drawmgr * d = (struct drawmgr *)luaL_checkudata(L, 1, "SOLUNA_DRAWMGR");
	int offset = luaL_optinteger(L, 2, 0);
	if (offset >= d->n) {
		return 0;
	}
	struct draw_pass *p = &d->data[offset].pass;
	if (p->pass_tag != PASS_TAG) {
		return luaL_error(L, "[%d] is not a draw_pass", offset);
	}
	lua_pushlightuserdata(L, p->rt);
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

int lpass_new(lua_State *L);

static int
ldrawmgr_newpass(lua_State *L) {
	struct drawmgr *d = luaL_checkudata(L, 1, "SOLUNA_DRAWMGR");
	lua_remove(L, 1);
	lpass_new(L);
	struct render_target *rt = lua_touserdata(L, -1);
	rt->frame = &d->frame;
	rt->frame_count = d->frame.frame_count -1;
	return 1;
}

static int
find_swapchain(lua_State *L, struct drawmgr * d) {
	struct draw_pass *pass = &d->data[0].pass;
	if (pass->pass_tag != PASS_TAG) {
		luaL_error(L, "Invalid SOLUNA_DRAWMGR");
	}
	if (pass->rt->swapchain) {
		// swapchain first
		return 0;
	} else {
		// the first pass is not swapchain
		int i = d->diff_pass;
		int first_swapchain = -1;
		while (i < d->n) {
			struct draw_pass *pass = &d->data[i].pass;
			if (pass->pass_tag != PASS_TAG) {
				luaL_error(L, "Invalid SOLUNA_DRAWMGR");
			}
			if (pass->rt->swapchain) {
				if (first_swapchain < 0) {
					// first swapchain
					first_swapchain = i;
				}
			} else if (first_swapchain >= 0) {
				// rt ... swapchain ... rt
				return first_swapchain;
			}
			i += pass->pass_n + 1;
		}
		// render target only
	}
	return -1;
}

static int
get_pass_n(struct drawmgr *d, int offset) {
	struct draw_pass *pass = &d->data[offset].pass;
	int n = pass->pass_n;
	if (n < 0) {
		n = d->n - offset - 1;
		pass->pass_n = n;
	}
	return n + 1;
}

static int
ldrawmgr_sortswapchain(lua_State *L) {
	struct drawmgr * d = (struct drawmgr *)luaL_checkudata(L, 1, "SOLUNA_DRAWMGR");
	if (d->diff_pass == 0)
		return 0;
	int i = find_swapchain(L, d);
	if (i < 0)
		return 0;
	// move swapchains to the end of drawmgr
	int end = d->n;
	union draw_union *ptr = d->data+i;
	while (i < d->n) {
		struct draw_pass *pass = &d->data[i].pass;
		int copyn = get_pass_n(d, i);
		if (pass->rt && pass->rt->swapchain) {
			if (end + copyn > d->cap)
				return luaL_error(L, "drawmgr overflow");
			memcpy(d->data + end, d->data + i, copyn * sizeof(*d->data));
			end += copyn;
			i += copyn;
		} else {
			memmove(ptr, d->data + i, copyn * sizeof(*d->data));
			ptr += copyn;
			i += copyn;
		}
	}
	assert(i == d->n);
	while (i < end) {
		int copyn = get_pass_n(d, i);
		memmove(ptr, d->data + i, copyn * sizeof(*d->data));
		ptr += copyn;
		i += copyn;
	}
	return 0;
}

static struct render_target *
get_rt(lua_State *L, int *last) {
	struct drawmgr * d = (struct drawmgr *)luaL_checkudata(L, 1, "SOLUNA_DRAWMGR");
	int pass_index = luaL_checkinteger(L, 2) - 1;
	struct draw_pass *pass = &d->data[pass_index].pass;
	if (pass_index >= d->n || pass->pass_tag != PASS_TAG)
		luaL_error(L, "Invalid pass index %d/%d", pass_index, d->n);
	if (last) {
		if (pass->pass_n < 0 || pass_index + pass->pass_n + 1 >= d->n) {
			*last = 1;
		} else {
			*last = 0;
		}
	}
	return pass->rt;
}

int
ldrawmgr_passbegin(lua_State *L) {
	struct render_target * p = get_rt(L, NULL);
	if (p->frame_count == p->frame->frame_count) {
		// not the first time in current frame
		if (!p->swapchain) {
			int i;
			int load_action[SG_MAX_COLOR_ATTACHMENTS];
			for (i = 0; i < SG_MAX_COLOR_ATTACHMENTS; i++) {
				load_action[i] = p->pass.action.colors[i].load_action;
				p->pass.action.colors[i].load_action = SG_LOADACTION_LOAD;
			}
			sg_begin_pass(&p->pass);
			for (i = 0; i < SG_MAX_COLOR_ATTACHMENTS; i++) {
				p->pass.action.colors[i].load_action = load_action[i];
			}
			if (p->clip_n > 0)
				render_target_scissor(p);
		}
		return 0;
	}
	p->frame_count = p->frame->frame_count;
	p->clip_n = 0;
	if (p->swapchain) {
		render_target_swapchain(p);
	}
	sg_begin_pass(&p->pass);
	return 0;
}

int
ldrawmgr_passend(lua_State *L) {
	int last_pass;
	struct render_target * p = get_rt(L, &last_pass);
	if (p->swapchain && !last_pass) {
		return 0;
	}
	sg_end_pass();
	return 0;
}

static int
ldrawmgr_submit(lua_State *L) {
	sg_commit();
	return 0;
}

static int
ldrawmgr_new(lua_State *L) {
	luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
	void * bank = lua_touserdata(L, 1);
	int cap = luaL_checkinteger(L, 2);
	struct drawmgr * d = (struct drawmgr *)lua_newuserdatauv(L, sizeof(*d) + (cap-1)*sizeof(d->data[0]), 0);
	memset(d, 0, sizeof(*d));
	d->bank = (struct sprite_bank *)bank;
	d->cap = cap;

	if (luaL_newmetatable(L, "SOLUNA_DRAWMGR")) {
		luaL_Reg l[] = {
			{ "__index", NULL },
			{ "__call", ldrawmgr_index },
			{ "reset", ldrawmgr_reset },
			{ "append", ldrawmgr_append },
			{ "nextpass", ldrawmgr_nextpass },
			{ "newpass", ldrawmgr_newpass },
			{ "beginpass", ldrawmgr_passbegin },
			{ "endpass", ldrawmgr_passend },
			{ "sortswapchain", ldrawmgr_sortswapchain },
			{ "submit", ldrawmgr_submit },
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
