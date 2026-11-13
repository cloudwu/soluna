#ifndef soluna_transient_h
#define soluna_transient_h

#include <stdlib.h>

#define TRANSIENT_BUFFER 0
#define TRANSIENT_IMAGE 1

struct transient_buffer * transient_buffer_new(int init_size,int tmp_size, int stage_index);
void transient_buffer_delete(struct transient_buffer *);
void transient_buffer_reset(struct transient_buffer *);
void * transient_buffer_append(struct transient_buffer *, const void *p, size_t sz, size_t *offset);
void * transient_buffer_alloc_command(struct transient_buffer *, int what);
void * transient_buffer_get_command(struct transient_buffer *, int what, int *n);
void * transient_buffer_tmp(struct transient_buffer *, size_t *sz);

#endif
