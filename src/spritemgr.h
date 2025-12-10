#ifndef soluna_spritemgr_h
#define soluna_spritemgr_h

#include <stdint.h>
#include <assert.h>

#define INVALID_TEXTUREID 0xffff

struct sprite_rect {
	uint32_t texid;
	uint32_t off;	// (dx + 0x8000) << (dy + 0x8000)
	uint32_t u;		// x << 16 | w
	uint32_t v;		// y << 16 | h
};

struct sprite_bank {
	int n;
	int cap;
	int texture_size;
	int texture_n;
	struct sprite_rect rect[1];
};

#endif
