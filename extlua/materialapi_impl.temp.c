#include "materialapi.h"

$API_EXTERN$

struct material_api {
	int version;

$API_DECL$
};

struct material_api *
extlua_material_api() {
	static struct material_api api = {
		MATERIAL_API_VERSION,

$API_STRUCT$
	};
	return &api;
}
