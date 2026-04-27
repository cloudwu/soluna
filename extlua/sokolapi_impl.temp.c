#include "sokol/sokol_gfx.h"

struct sokol_api {
	int version;

$API_DECL$
};

struct sokol_api *
extlua_sokol_api() {
	static struct sokol_api api = {
		SOKOL_GFX_INCLUDED,

$API_STRUCT$
	};
	return &api;
}
