#include <string.h>

#include <lua.h>
#include <lauxlib.h>

#include "batch.h"
#include "extlua/materialapi.h"
#include "sprite_submit.h"

int
material_push(lua_State *L, int material_id, const struct material_push_item *item) {
	if (material_id <= 0) {
		return luaL_error(L, "Invalid material id %d", material_id);
	}
	if (item == NULL) {
		return luaL_error(L, "Invalid material item");
	}
	if (item->sprite < -1) {
		return luaL_error(L, "Invalid material sprite %d", item->sprite);
	}
	struct draw_primitive stream[2];
	memset(stream, 0, sizeof(stream));

	struct draw_primitive_external *ext = (struct draw_primitive_external *)&stream[1];
	stream[0].sprite = -material_id;
	sprite_set_xy(&stream[0], item->x, item->y);
	float scale = item->scale > 0.0f ? item->scale : 1.0f;
	if (scale != 1.0f || item->rotation != 0.0f) {
		sprite_set_sr(&stream[0], scale, item->rotation);
	}
	ext->sprite = item->sprite;
	if (item->data != NULL) {
		memcpy((char *)ext + sizeof(*ext), item->data, MATERIAL_DATA_SIZE);
	}
	lua_pushlstring(L, (const char *)stream, sizeof(stream));
	return 1;
}
