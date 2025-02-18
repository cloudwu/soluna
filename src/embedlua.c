#include "ltasklua_bootstrap.h"
#include "ltasklua_service.h"
#include "ltasklua_log.h"
#include "ltasklua_timer.h"
#include "ltasklua_root.h"
#include "ltasklua_main.h"
#include "ltasklua_external.h"
#include "ltasklua_start.h"

#include "lua.h"
#include "lauxlib.h"

#define REG_SOURCE(name) \
	lua_pushlightuserdata(L, (void *)luasrc_##name);	\
	lua_pushinteger(L, sizeof(luasrc_##name));	\
	lua_pushcclosure(L, get_string, 2);	\
	lua_setfield(L, -2, #name);

static int
get_string(lua_State *L) {
	const char * s = (const char *)lua_touserdata(L, lua_upvalueindex(1));
	size_t sz = (size_t)lua_tointeger(L, lua_upvalueindex(2));
	lua_pushlstring(L, s, sz);
	return 1;
}

int
luaopen_embedsource(lua_State *L) {
	lua_newtable(L);
		lua_newtable(L);	// runtime
			REG_SOURCE(bootstrap)
			REG_SOURCE(service)
			REG_SOURCE(main)
		lua_setfield(L, -2, "runtime");
			
		lua_newtable(L);	// service
			REG_SOURCE(log)
			REG_SOURCE(root)
			REG_SOURCE(timer)
			REG_SOURCE(external)
			REG_SOURCE(start)
		lua_setfield(L, -2, "service");
	return 1;
}
