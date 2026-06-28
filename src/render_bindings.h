#ifndef soluna_render_bindings_h
#define soluna_render_bindings_h

#include "sokol/sokol_gfx.h"

struct render_bindings {
	int base;
	sg_bindings bindings;
};

struct view {
	sg_view view;
	int type;
};

#define DRAWFUNC(name) (sg_query_features().draw_base_instance ? name##_ex : name)

#endif
