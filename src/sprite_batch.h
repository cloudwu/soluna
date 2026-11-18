#ifndef soluna_sprite_batch_h
#define soluna_sprite_batch_h

#include <lua.h>
#include "transform.h"
#include "batch.h"

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

int lsprite_newbatch(lua_State *L);

#endif
