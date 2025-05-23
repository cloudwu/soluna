#ifndef soluna_batch_h
#define soluna_batch_h

#include <stdint.h>

struct draw_primitive {
	int32_t x;		// sign bit + 23 + 8   fix number
	int32_t y;
	uint32_t sr;	// scale + rot
	int32_t sprite;		// negative : material 
};

struct draw_batch;

struct draw_batch * batch_new(int size);
struct draw_primitive * batch_reserve(struct draw_batch *, int size);
void batch_delete(struct draw_batch *);

#endif
