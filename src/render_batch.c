#include <lua.h>
#include <lauxlib.h>
#include <string.h>

#include "sprite_submit.h"
#include "render_batch.h"

static int
lbatch_reset(lua_State *L) {
	struct render_command_buffer *cb = luaL_checkudata(L, 1,  "SOLUNA_BATCH");
	cb->command_n = 0;
	struct batch *b = &cb->b;
	b->n = 0;
	b->layer = 0;
	sprite_transform_identity(&b->trans);
	return 0;
}

static int
lbatch_release(lua_State *L) {
	struct render_command_buffer *cb = luaL_checkudata(L, 1,  "SOLUNA_BATCH");
	cb->command_n = 0;
	struct batch *b = &cb->b;
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
	struct render_command_buffer *cb = luaL_checkudata(L, 1,  "SOLUNA_BATCH");
	struct batch *b = &cb->b;
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
	struct render_command_buffer *cb = luaL_checkudata(L, 1,  "SOLUNA_BATCH");
	lua_pushlightuserdata(L, cb);
	return 1;
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
	struct render_command_buffer *cb = luaL_checkudata(L, 1,  "SOLUNA_BATCH");
	struct batch *b = &cb->b;
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
	struct render_command_buffer *cb = luaL_checkudata(L, 1,  "SOLUNA_BATCH");
	struct batch *b = &cb->b;
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
lbatch_pass(lua_State *L) {
	struct render_command_buffer *cb = luaL_checkudata(L, 1,  "SOLUNA_BATCH");
	if (cb->command_n >= cb->command_cap)
		return luaL_error(L, "Too many command %d/%d", cb->command_n, cb->command_cap);
	int n = cb->command_n;
	struct render_command *c = &cb->command[n];
	c->pass_id = luaL_checkinteger(L, 2);
	c->batch_start = cb->b.n;
	++cb->command_n;
	return 0;
}

int
lrender_batch(lua_State *L) {
	int size = luaL_checkinteger(L, 1);
	struct render_command_buffer *cb = (struct render_command_buffer *)lua_newuserdatauv(L, sizeof(*cb) + (size-1)*sizeof(cb->command[0]), 1);
	cb->command_n = 0;
	cb->command_cap = size;
	struct batch *b = &cb->b;
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
			{ "pass", lbatch_pass },
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
