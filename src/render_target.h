#ifndef soluna_render_target_h
#define soluna_render_target_h

#include "sokol/sokol_gfx.h"

struct render_target {
	sg_pass pass;
	int width;
	int height;
	float inv_width;
	float inv_height;
};

#endif