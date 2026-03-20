#include <lua.h>
#include <lauxlib.h>
#include "soloud_c.h"

static int
laudio_init(lua_State *L) {
	void * so = Soloud_create();
	
	// todo : Call Soloud_initEx
	int err = Soloud_init(so);

	lua_pushlightuserdata(L, so);
	
	if (err) {
		lua_pushstring(L, Soloud_getErrorString(so, err));
		return 2;
	} else {
		return 1;
	}
}

static int
laudio_deinit(lua_State *L) {
	luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
	void * so = lua_touserdata(L, 1);
	Soloud_deinit(so);
	Soloud_destroy(so);
	return 0;
}

static int
laudio_load(lua_State *L) {
	size_t sz;
	const char * data = luaL_checklstring(L, 1, &sz);
	void * s = Wav_create();
	int err = Wav_loadMemEx(s, (const unsigned char *)data, sz, 0, 0);
	if (err) {
		Wav_destroy(s);
		return 0;
	}
	lua_pushlightuserdata(L, s);
	return 1;
}

static int
laudio_unload(lua_State *L) {
	if (lua_isnoneornil(L, 1)) {
		return 0;
	}
	luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
	void *s = lua_touserdata(L, 1);
	Wav_destroy(s);
	return 0;
}

static int
laudio_play(lua_State *L) {
	if (lua_isnoneornil(L, 2)) {
		return 0;
	}
	luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
	luaL_checktype(L, 2, LUA_TLIGHTUSERDATA);
	void * dev = lua_touserdata(L, 1);
	void * s = lua_touserdata(L, 2);
	unsigned int h = Soloud_play(dev, s);
	lua_pushinteger(L, h);
	return 1;
}

int
luaopen_soluna_audio(lua_State *L) {
	luaL_checkversion(L);
	luaL_Reg l[] = {
		{ "init", laudio_init },
		{ "deinit", laudio_deinit },
		{ "load", laudio_load },
		{ "unload", laudio_unload },
		{ "play", laudio_play },
		{ NULL, NULL },
	};
	luaL_newlib(L, l);
	return 1;
}
