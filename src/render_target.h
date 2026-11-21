#ifndef soluna_render_target_h
#define soluna_render_target_h

#include "sokol/sokol_gfx.h"

struct render_frame {
	unsigned int frame_count;
};

#define RENDER_CLIP_MAX 64

struct render_clip {
	int x;
	int y;
	int w;
	int h;
};

struct render_target {
	sg_pass pass;
	struct render_frame *frame;
	unsigned int frame_count;
	int swapchain;
	int width;
	int height;
	float dpi_scale;
	float inv_width;
	float inv_height;
	float framesize[4];
	int clip_n;
	struct render_clip clip[RENDER_CLIP_MAX];
};

void render_target_size(struct render_target *rt, int width, int height, int flip);
void render_target_swapchain(struct render_target *rt);
void render_target_scissor(struct render_target *rt);

#endif