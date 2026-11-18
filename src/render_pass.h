#ifndef soluna_render_pass_h
#define soluna_render_pass_h

#include "sokol/sokol_gfx.h"

struct render_pass {
	sg_pass pass;
	int width;
	int height;
	float inv_width;
	float inv_height;
};

#endif