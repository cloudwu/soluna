#ifndef soluna_render_target_h
#define soluna_render_target_h

#include "sokol/sokol_gfx.h"

struct render_frame {
	unsigned int frame_count;
	sg_swapchain swapchain;
};

struct render_target {
	sg_pass pass;
	struct render_frame *frame;
	unsigned int frame_count;
	int swapchain;
	int width;
	int height;
	float inv_width;
	float inv_height;
};

#endif