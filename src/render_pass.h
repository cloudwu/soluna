#ifndef soluna_render_pass_h
#define soluna_render_pass_h

#include "sokol/sokol_gfx.h"

struct render_pass {
	sg_pass pass;
	int frame_width;
	int frame_hegith;
};

#endif