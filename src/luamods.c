#include <lua.h>
#include <lauxlib.h>

int luaopen_ltask(lua_State *L);
int luaopen_ltask_root(lua_State *L);
int luaopen_ltask_bootstrap(lua_State *L);
int luaopen_ltask_mqueue(lua_State *L);
int luaopen_embedsource(lua_State *L);
int luaopen_appmessage(lua_State *L);
int luaopen_applog(lua_State *L);
int luaopen_image(lua_State *L);
int luaopen_render(lua_State *L);
int luaopen_spritemgr(lua_State *L);
int luaopen_datalist(lua_State *L);
int luaopen_soluna_file(lua_State *L);
int luaopen_font_truetype(lua_State *L);
int luaopen_font_manager(lua_State *L);
int luaopen_font(lua_State *L);
int luaopen_drawmgr(lua_State *L);
int luaopen_material_default(lua_State *L);
int luaopen_material_text(lua_State *L);
int luaopen_material_quad(lua_State *L);
int luaopen_material_mask(lua_State *L);
int luaopen_material_perspective_quad(lua_State *L);
int luaopen_material_blit(lua_State *L);
int luaopen_soluna_app(lua_State *L);
int luaopen_font_system(lua_State *L);
int luaopen_gamepad_device(lua_State *L);
int luaopen_gamepad(lua_State *L);
int luaopen_localfs(lua_State *L);
int luaopen_image_sdf(lua_State *L);
int luaopen_layout_yoga(lua_State *L);
int luaopen_url(lua_State *L);
int luaopen_skynet_crypt(lua_State *L);
int luaopen_zip(lua_State *L);

void soluna_embed(lua_State* L) {
    static const luaL_Reg modules[] = {
		{ "ltask", luaopen_ltask},
		{ "ltask.root", luaopen_ltask_root},
		{ "ltask.bootstrap", luaopen_ltask_bootstrap},
		{ "ltask.mqueue", luaopen_ltask_mqueue},
		{ "soluna.app", luaopen_soluna_app },
		{ "soluna.embedsource", luaopen_embedsource},
		{ "soluna.log", luaopen_applog },
		{ "soluna.image", luaopen_image },
		{ "soluna.render", luaopen_render },
		{ "soluna.spritemgr", luaopen_spritemgr },
		{ "soluna.drawmgr", luaopen_drawmgr },
		{ "soluna.material.default", luaopen_material_default },
		{ "soluna.material.text", luaopen_material_text },
		{ "soluna.material.quad", luaopen_material_quad },
		{ "soluna.material.mask", luaopen_material_mask },
		{ "soluna.material.perspective_quad", luaopen_material_perspective_quad },
		{ "soluna.material.blit", luaopen_material_blit },
		{ "soluna.datalist", luaopen_datalist },
		{ "soluna.file", luaopen_soluna_file },
		{ "soluna.font", luaopen_font },
		{ "soluna.font.truetype", luaopen_font_truetype },
		{ "soluna.font.manager", luaopen_font_manager },
		{ "soluna.font.system", luaopen_font_system },
		{ "soluna.gamepad", luaopen_gamepad },
		{ "soluna.gamepad.device", luaopen_gamepad_device },
		{ "soluna.lfs", luaopen_localfs },
		{ "soluna.image.sdf", luaopen_image_sdf },
		{ "soluna.layout.yoga", luaopen_layout_yoga },
		{ "soluna.url", luaopen_url },
		{ "soluna.crypt", luaopen_skynet_crypt },
		{ "soluna.zip", luaopen_zip },
//		{ "luaforward", luaopen_luaforward },
		{ NULL, NULL },
    };

    const luaL_Reg *lib;
    luaL_getsubtable(L, LUA_REGISTRYINDEX, LUA_PRELOAD_TABLE);
    for (lib = modules; lib->func; lib++) {
        lua_pushcfunction(L, lib->func);
        lua_setfield(L, -2, lib->name);
    }
    lua_pop(L, 1);
}
