#include "bootstrap.lua.h"
#include "service.lua.h"
#include "log.lua.h"
#include "timer.lua.h"
#include "root.lua.h"
#include "main.lua.h"
#include "start.lua.h"
#include "print_r.lua.h"
#include "loader.lua.h"
#include "spritebundle.lua.h"
#include "render.lua.h"
#include "settingdefault.dl.h"
#include "settings.lua.h"
#include "initsetting.lua.h"
#include "fontmgr.lua.h"
#include "gamepad.lua.h"
#include "soluna.lua.h"
#include "icon.lua.h"
#include "layout.lua.h"
#include "text.lua.h"
#include "util.lua.h"
#include "coroutine.lua.h"
#include "packageloader.lua.h"

#include "lua.h"
#include "lauxlib.h"

#define REG_SOURCE(name) \
	lua_pushlightuserdata(L, (void *)luasrc_##name);	\
	lua_pushinteger(L, sizeof(luasrc_##name) - 1);	\
	lua_pushcclosure(L, get_string, 2);	\
	lua_setfield(L, -2, #name);

#define REG_DATALIST(name) \
	lua_pushlightuserdata(L, (void *)dl_##name);	\
	lua_pushinteger(L, sizeof(dl_##name) - 1);	\
	lua_pushcclosure(L, get_stringloader, 2);	\
	lua_setfield(L, -2, #name);

static int
get_string(lua_State *L) {
	const char * s = (const char *)lua_touserdata(L, lua_upvalueindex(1));
	size_t sz = (size_t)lua_tointeger(L, lua_upvalueindex(2));
	lua_pushexternalstring(L, s, sz, NULL, NULL);
	return 1;
}

static int
get_stringloader(lua_State *L) {
	lua_pushvalue(L, lua_upvalueindex(1));
	lua_pushvalue(L, lua_upvalueindex(2));
	return 2;
}

int
luaopen_embedsource(lua_State *L) {
	lua_newtable(L);
		lua_newtable(L);	// runtime
			REG_SOURCE(bootstrap)
			REG_SOURCE(service)
			REG_SOURCE(main)
			REG_SOURCE(print_r)
			REG_SOURCE(fontmgr)
			REG_SOURCE(packageloader)
		lua_setfield(L, -2, "runtime");

		lua_newtable(L);	// runtime
			REG_SOURCE(spritebundle)
			REG_SOURCE(icon)
			REG_SOURCE(layout)
			REG_SOURCE(text)
			REG_SOURCE(soluna)
			REG_SOURCE(util)
			REG_SOURCE(coroutine)
			REG_SOURCE(initsetting)
		lua_setfield(L, -2, "lib");

		lua_newtable(L);	// service
			REG_SOURCE(log)
			REG_SOURCE(root)
			REG_SOURCE(timer)
			REG_SOURCE(start)
			REG_SOURCE(loader)
			REG_SOURCE(render)
			REG_SOURCE(gamepad)
			REG_SOURCE(settings)
		lua_setfield(L, -2, "service");

		lua_newtable(L);	// data list
			REG_DATALIST(settingdefault)
		lua_setfield(L, -2, "data");
	return 1;
}
