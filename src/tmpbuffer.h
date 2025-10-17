#ifndef soluna_tmp_buffer_h
#define soluna_tmp_buffer_h

#include <lua.h>
#include <lauxlib.h>

struct tmp_buffer {
	void *ptr;
	size_t sz;
};

#define TMPBUFFER_PTR(type, obj) (type *)((obj)->ptr)
#define TMPBUFFER_SIZE(type, obj) ((obj)->sz / sizeof(type))

static inline void
tmp_buffer_init(lua_State *L, struct tmp_buffer *tmp, int uv_index, const char *key) {
	if (lua_getfield(L, 1, key) != LUA_TUSERDATA)
		luaL_error(L, "Invalid key .%s", key);
	if (lua_type(L, -1) != LUA_TUSERDATA || lua_getmetatable(L, -1)) {
		luaL_error(L, "Not an userdata without metatable");
	}
	tmp->ptr = lua_touserdata(L, -1);
	tmp->sz = lua_rawlen(L, -1);
	// ud, object
	lua_setiuservalue(L, -2, uv_index);
}

#endif
