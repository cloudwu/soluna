#ifndef soluna_tmp_buffer_h
#define soluna_tmp_buffer_h

#include "transient.h"

struct tmp_buffer {
	void *ptr;
	size_t sz;
};

#define TMPBUFFER_PTR(type, obj) (type *)((obj)->ptr)
#define TMPBUFFER_SIZE(type, obj) ((obj)->sz / sizeof(type))

static inline void
tmp_buffer_init(struct tmp_buffer *tmp, struct transient_buffer *tb) {
	tmp->ptr = transient_buffer_tmp(tb, &tmp->sz);
}

#endif
