#include "sokol/sokol_gfx.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "transient.h"

#define MAX_TRANSIENT_QUEUE 32
#define BUFFER_QUEUE 4096
#define IMAGE_QUEUE 128

struct command_queue {
	int cap;
	int n;
	int size;
	int seg_n;
	void *buffer;
};

static void
command_queue_init(struct command_queue *q, int seg_n, int sz) {
	q->cap = seg_n;
	q->n = 0;
	q->size = sz;
	q->seg_n = seg_n;
	q->buffer = malloc(sz * seg_n);
}

static void
command_queue_deinit(struct command_queue *q) {
	free(q->buffer);
	memset(q, 0, sizeof(*q));
}

static void
command_queue_reset(struct command_queue *q) {
	q->n = 0;
}

static void *
command_queue_alloc(struct command_queue *q) {
	if (q->n >= q->cap) {
		q->cap += q->seg_n;
		if (q->cap == 0)
			return NULL;
		void *buffer = malloc(q->cap * q->size);
		if (buffer == NULL) {
			command_queue_deinit(q);
			return NULL;
		}
		memcpy(buffer, q->buffer, q->n * q->size);
		free(q->buffer);
		q->buffer = buffer;
	}
	char * ret = (char *)q->buffer + q->n * q->size;
	memset(ret, 0, q->size);
	++q->n;
	return ret;
}

struct transient_buffer {
	sg_buffer set[MAX_TRANSIENT_QUEUE];
	int valid_n;
	int stage_index;
	int min_cap;
	size_t offset;
	size_t cap;
	size_t total;
	int tmp_size;
	void *tmp_buffer;
	struct command_queue buffer[2];
};

static sg_buffer
new_buffer(size_t sz, int stage_index) {
	int staging_buffer = (stage_index == 0);
	int staging_index_buffer = (stage_index != 0);
	return sg_make_buffer(&(sg_buffer_desc){
        .usage = {
            .staging_buffer = staging_buffer,
            .staging_index_buffer = staging_index_buffer,
            .write_transient = true,
            .copy_src = true,
        },
        .size = sz,
        .label = "staging-buffer",
    });
}

struct transient_buffer *
transient_buffer_new(int init_size, int tmp_size, int stage_index) {
	struct transient_buffer * tb = malloc(sizeof(*tb));
	init_size = (init_size + 3) & ~3;	// alignment to 4
	tb->valid_n = 1;
	tb->offset = 0;
	tb->total = 0;
	tb->cap = init_size;
	tb->stage_index = stage_index;
	tb->min_cap = init_size;
	tb->set[0] = new_buffer(init_size, stage_index);
	tb->tmp_buffer = malloc(tmp_size);
	tb->tmp_size = tmp_size;
	command_queue_init(&tb->buffer[TRANSIENT_BUFFER], BUFFER_QUEUE, sizeof(sg_copy_buffer_to_buffer_desc));
	command_queue_init(&tb->buffer[TRANSIENT_IMAGE], IMAGE_QUEUE, sizeof(sg_copy_buffer_to_image_desc));
	return tb;
}

void
transient_buffer_delete(struct transient_buffer *tb) {
	int i;
	for (i=0;i<tb->valid_n;i++) {
		sg_destroy_buffer(tb->set[i]);
	}
	tb->valid_n = 0;
	command_queue_deinit(&tb->buffer[TRANSIENT_BUFFER]);
	command_queue_deinit(&tb->buffer[TRANSIENT_IMAGE]);
	free(tb->tmp_buffer);
	free(tb);
}

void
transient_buffer_reset(struct transient_buffer *tb) {
	int i;
	for (i=1;i<tb->valid_n;i++) {
		sg_destroy_buffer(tb->set[i]);
	}
	tb->valid_n = 1;
	if (tb->total * 2 < tb->cap && tb->total > tb->min_cap) {
		// shrink buffer
		tb->cap /= 2;
		sg_destroy_buffer(tb->set[0]);
		tb->set[0] = new_buffer(tb->cap, tb->stage_index);
	}
	tb->offset = 0;
	tb->total = 0;
	command_queue_reset(&tb->buffer[TRANSIENT_BUFFER]);
	command_queue_reset(&tb->buffer[TRANSIENT_IMAGE]);
}

void *
transient_buffer_append(struct transient_buffer *tb, const void *p, size_t sz, size_t *offset) {
	if (sz + tb->offset > tb->cap) {
		if (tb->valid_n >= MAX_TRANSIENT_QUEUE)
			return NULL;
		int newcap = tb->cap * 2;
		while (newcap < sz) {
			newcap *= 2;
		}
		tb->cap = newcap;
		tb->set[tb->valid_n] = tb->set[0];
		tb->set[0] = new_buffer(tb->cap, tb->stage_index);
		tb->offset = 0;
		tb->valid_n++;
	}
	*offset = tb->offset;
    sg_write_buffer_transient(&(sg_write_buffer_desc){
        .src.data = (sg_range){ p, sz },
        .dst.buffer = tb->set[0],
		.dst.offset = tb->offset,
    });
	sz = (sz + 3) & ~3;
	tb->total += sz;
	tb->offset += sz;
	return &tb->set[0];
}

void *
transient_buffer_alloc_command(struct transient_buffer *tb, int what) {
	return command_queue_alloc(&tb->buffer[what]);
}

void *
transient_buffer_get_command(struct transient_buffer *tb, int what, int *n) {
	*n = tb->buffer[what].n;
	return tb->buffer[what].buffer;
}

void *
transient_buffer_tmp(struct transient_buffer *tb, size_t *sz) {
	*sz = tb->tmp_size;
	return tb->tmp_buffer;
}
