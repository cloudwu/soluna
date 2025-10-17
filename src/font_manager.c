#include "font_manager.h"
#include "mutex.h"
#include "truetype.h"

#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb/stb_truetype.h>

#define FONT_MANAGER_SLOTLINE (FONT_MANAGER_TEXSIZE/FONT_MANAGER_GLYPHSIZE)
#define FONT_MANAGER_SLOTS (FONT_MANAGER_SLOTLINE*FONT_MANAGER_SLOTLINE)
#define FONT_MANAGER_HASHSLOTS (FONT_MANAGER_SLOTS * 2)


// --------------
//
//                       xmin                     xmax
//                        |                         |
//                        |<-------- width -------->|
//                        |                         |
//              |         +-------------------------+----------------- ymax
//              |         |    ggggggggg   ggggg    |     ^        ^
//              |         |   g:::::::::ggg::::g    |     |        |
//              |         |  g:::::::::::::::::g    |     |        |
//              |         | g::::::ggggg::::::gg    |     |        |
//              |         | g:::::g     g:::::g     |     |        |
//    offset_x -|-------->| g:::::g     g:::::g     |  offset_y    |
//              |         | g:::::g     g:::::g     |     |        |
//              |         | g::::::g    g:::::g     |     |        |
//              |         | g:::::::ggggg:::::g     |     |        |
//              |         |  g::::::::::::::::g     |     |      height
//              |         |   gg::::::::::::::g     |     |        |
//  baseline ---*---------|---- gggggggg::::::g-----*--------      |
//            / |         |             g:::::g     |              |
//     origin   |         | g:::::gg   gg:::::g     |              |
//              |         | g:::::gg   gg:::::g     |              |
//              |         |  g::::::ggg:::::::g     |              |
//              |         |   gg:::::::::::::g      |              |
//              |         |     ggg::::::ggg        |              |
//              |         |         gggggg          |              v
//              |         +-------------------------+----------------- ymin
//              |                                   |
//              |------------- advance_x ---------->|

struct font_slot {
	uint32_t codepoint_key;	// high 8 bits (ttf index)
	int16_t offset_x;
	int16_t offset_y;
	int16_t advance_x;
	int16_t advance_y;
	uint16_t w;
	uint16_t h;
};

struct priority_list {
	int version;
	int16_t prev;
	int16_t next;
};

struct truetype_font;

struct font_manager {
	int version;
	int count;
	int16_t list_head;
	struct font_slot slots[FONT_MANAGER_SLOTS];
	struct priority_list priority[FONT_MANAGER_SLOTS];
	int16_t hash[FONT_MANAGER_HASHSLOTS];
	struct truetype_font* ttf;
	void *L;
	int dpi_perinch;
	int dirty;
	int icon_n;
	unsigned char *icon_data;
	mutex_t mutex;
	uint8_t texture_buffer[FONT_MANAGER_TEXSIZE*FONT_MANAGER_TEXSIZE];
};

const void *
font_manager_texture(struct font_manager *F, int *sz) {
	*sz = FONT_MANAGER_TEXSIZE;
	return F->texture_buffer;
}

/*
	F->priority is a circular linked list for the LRU cache.
	F->hash is for lookup with [font, codepoint].
*/

#define COLLISION_STEP 7
#define DISTANCE_OFFSET 8
#define ORIGINAL_SIZE (FONT_MANAGER_GLYPHSIZE - DISTANCE_OFFSET * 2)
#define ONEDGE_VALUE	180
#define PIXEL_DIST_SCALE (ONEDGE_VALUE/(float)(DISTANCE_OFFSET))

static const int SAPCE_CODEPOINT[] = {
    ' ', '\t', '\n', '\r',
};

static inline int
is_space_codepoint(int codepoint){
    for (int ii=0; ii < sizeof(SAPCE_CODEPOINT)/sizeof(SAPCE_CODEPOINT[0]); ++ii){
        if (codepoint == SAPCE_CODEPOINT[ii]){
            return 1;
        }
    }
    return 0;
}

static inline void
lock(struct font_manager *F) {
	mutex_acquire(F->mutex);
}

static inline void
unlock(struct font_manager *F) {
	mutex_release(F->mutex);
}

static inline const stbtt_fontinfo*
get_ttf_unsafe(struct font_manager *F, int fontid){
	return truetype_font(F->ttf, fontid, F->L);
}

static inline const stbtt_fontinfo *
get_ttf(struct font_manager *F, int fontid) {
	lock(F);
	const stbtt_fontinfo * r = get_ttf_unsafe(F, fontid);
	unlock(F);
	return r;
}

static inline int
ttf_with_family(struct font_manager *F, const char* family){
	return truetype_name(F->L, family);
}

static inline int
hash(int value) {
	return (value * 0xdeece66d + 0xb) % FONT_MANAGER_HASHSLOTS;
}

static int
hash_lookup(struct font_manager *F, int cp) {
	int slot;
	int position = hash(cp);
	while ((slot = F->hash[position]) >= 0) {
		struct font_slot * s = &F->slots[slot];
		if (s->codepoint_key == cp)
			return slot;
		position = (position + COLLISION_STEP) % FONT_MANAGER_HASHSLOTS;
	}
	return -1;
}

static void rehash(struct font_manager *F);

static void
hash_insert(struct font_manager *F, int cp, int slotid) {
	++F->count;
	if (F->count > FONT_MANAGER_SLOTS + FONT_MANAGER_SLOTS/2) {
		rehash(F);
	}
	int position = hash(cp);
	int slot;
	while ((slot = F->hash[position]) >= 0) {
		struct font_slot * s = &F->slots[slot];
		if (s->codepoint_key < 0)
			break;
		assert(s->codepoint_key != cp);

		position = (position + COLLISION_STEP) % FONT_MANAGER_HASHSLOTS;
	}
	F->hash[position] = slotid;
	F->slots[slotid].codepoint_key = cp;
}

static void
rehash(struct font_manager *F) {
	int i;
	for (i=0;i<FONT_MANAGER_HASHSLOTS;i++) {
		F->hash[i] = -1;	// reset slots
	}
	F->count = 0;
	int count = 0;
	(void)count;
	for (i=0;i<FONT_MANAGER_SLOTS;i++) {
		int cp = F->slots[i].codepoint_key;
		if (cp >= 0) {
			assert(++count <= FONT_MANAGER_SLOTS);
			hash_insert(F, cp, i);
		}
	}
}

static void
remove_node(struct font_manager *F, struct priority_list *node) {
	struct priority_list *prev_node = &F->priority[node->prev];
	struct priority_list *next_node = &F->priority[node->next];
	prev_node->next = node->next;
	next_node->prev = node->prev;
}

static void
touch_slot(struct font_manager *F, int slotid) {
	struct priority_list *node = &F->priority[slotid];
	node->version = F->version;
	if (slotid == F->list_head)
		return;
	remove_node(F, node);
	// insert before head
	int head = F->list_head;
	int tail = F->priority[head].prev;
	node->prev = tail;
	node->next = head;
	struct priority_list *head_node = &F->priority[head];
	struct priority_list *tail_node = &F->priority[tail];
	head_node->prev = slotid;
	tail_node->next = slotid;
	F->list_head = slotid;
}

static int
get_icon(struct font_manager *F, int cp, struct font_glyph *glyph) {
	if (cp < 0 || cp >= F->icon_n) {
		memset(glyph, 0, sizeof(*glyph));
		return -1;
	}
	glyph->offset_x = 0;
	glyph->offset_y = -DISTANCE_OFFSET/2;
	glyph->advance_x = FONT_MANAGER_GLYPHSIZE;
	glyph->advance_y = FONT_MANAGER_GLYPHSIZE;
	glyph->w = FONT_MANAGER_GLYPHSIZE;
	glyph->h = FONT_MANAGER_GLYPHSIZE;
	glyph->u = 0;
	glyph->v = 0;
	return 0;
}

// 1 exist in cache. 0 not exist in cache , call font_manager_update. -1 failed.
static int
font_manager_touch_unsafe(struct font_manager *F, int font, int codepoint, struct font_glyph *glyph) {
	int cp = codepoint_key(font, codepoint);
	int slot = hash_lookup(F, cp);
	if (slot >= 0) {
		touch_slot(F, slot);
		struct font_slot *s = &F->slots[slot];
		glyph->offset_x = s->offset_x;
		glyph->offset_y = s->offset_y;
		glyph->advance_x = s->advance_x;
		glyph->advance_y = s->advance_y;
		glyph->w = s->w;
		glyph->h = s->h;
		glyph->u = (slot % FONT_MANAGER_SLOTLINE) * FONT_MANAGER_GLYPHSIZE;
		glyph->v = (slot / FONT_MANAGER_SLOTLINE) * FONT_MANAGER_GLYPHSIZE;

		return 1;
	}
	int last_slot = F->priority[F->list_head].prev;
	struct priority_list *last_node = &F->priority[last_slot];
	
	if (font == FONT_ICON) {
		if (last_node->version != F->version) {
			F->dirty = 1;
		}
		return get_icon(F, codepoint, glyph);
	}
	
	if (font_index(font) <= 0) {
		// invalid font
		memset(glyph, 0, sizeof(*glyph));
		return -1;
	}

	const struct stbtt_fontinfo *fi = get_ttf_unsafe(F, font);

	float scale = stbtt_ScaleForMappingEmToPixels(fi, ORIGINAL_SIZE);
	int ascent, descent, lineGap;
	int advance, lsb;
	int ix0, iy0, ix1, iy1;

	if (!stbtt_GetFontVMetricsOS2(fi, &ascent, &descent, &lineGap)) {
		stbtt_GetFontVMetrics(fi, &ascent, &descent, &lineGap);
	}
	stbtt_GetCodepointHMetrics(fi, codepoint, &advance, &lsb);
	stbtt_GetCodepointBitmapBox(fi, codepoint, scale, scale, &ix0, &iy0, &ix1, &iy1);

	glyph->w = ix1-ix0 + DISTANCE_OFFSET * 2;
	glyph->h = iy1-iy0 + DISTANCE_OFFSET * 2;
	glyph->offset_x = (short)(lsb * scale) - DISTANCE_OFFSET;
	glyph->offset_y = iy0 - DISTANCE_OFFSET;
	glyph->advance_x = (short)(((float)advance) * scale + 0.5f);
	glyph->advance_y = (short)((ascent - descent) * scale + 0.5f);
	glyph->u = 0;
	glyph->v = 0;

	if (last_node->version == F->version)	// full ?
		return -1;
		
	F->dirty = 1;

	return 0;
}

static int
font_manager_touch(struct font_manager *F, int font, int codepoint, struct font_glyph *glyph) {
	lock(F);
	int r = font_manager_touch_unsafe(F, font, codepoint, glyph);
	unlock(F);
	return r;
}

static inline int
scale_font(int v, float scale, int size) {
	return ((int)(v * scale * size) + ORIGINAL_SIZE/2) / ORIGINAL_SIZE;
}

static inline float
fscale_font(float v, float scale, int size){
	return (v * scale * size) / (float)ORIGINAL_SIZE;
}

void
font_manager_fontheight(struct font_manager *F, int fontid, int size, int *ascent, int *descent, int *lineGap) {
	if (fontid <= 0) {
		*ascent = 0;
		*descent = 0;
		*lineGap = 0;
	}

	const struct stbtt_fontinfo *fi = get_ttf(F, fontid);
	float scale = stbtt_ScaleForMappingEmToPixels(fi, ORIGINAL_SIZE);
	if (!stbtt_GetFontVMetricsOS2(fi, ascent, descent, lineGap)) {
		stbtt_GetFontVMetrics(fi, ascent, descent, lineGap);
	}
	*ascent = scale_font(*ascent, scale, size);
	*descent = scale_font(*descent, scale, size);
	*lineGap = scale_font(*lineGap, scale, size);
}

int 
font_manager_underline(struct font_manager *F, int fontid, int size, float *position, float *thickness){
	const struct stbtt_fontinfo *fi = get_ttf(F, fontid);
	float scale = stbtt_ScaleForMappingEmToPixels(fi, ORIGINAL_SIZE);
	stbtt_uint32 post = stbtt__find_table(fi->data, fi->fontstart, "post");
	if (!post) {
		return -1;
	}
	int16_t underline_position = ttSHORT(fi->data + post + 8);
	int16_t underline_thickness = ttSHORT(fi->data + post + 10);
	*position = fscale_font(underline_position, scale, size);
	*thickness = fscale_font(underline_thickness, scale, size);
	return 0;
}

// F->dpi_perinch is a constant, so do not need to lock
int
font_manager_pixelsize(struct font_manager *F, int fontid, int pointsize) {
	//TODO: need set dpi when init font_manager
	const int defaultdpi = 96;
	const int dpi = F->dpi_perinch == 0 ? defaultdpi : F->dpi_perinch;
	return (int)((pointsize / 72.f) * dpi + 0.5f);
}

static inline void
scale(short *v, int size) {
	*v = (*v * size + ORIGINAL_SIZE/2) / ORIGINAL_SIZE;
}

static inline void
uscale(uint16_t *v, int size) {
	*v = (*v * size + ORIGINAL_SIZE/2) / ORIGINAL_SIZE;
}

void
font_manager_scale(struct font_manager *F, struct font_glyph *glyph, int size) {
	(void)F;
	scale(&glyph->offset_x, size);
	scale(&glyph->offset_y, size);
	scale(&glyph->advance_x, size);
	scale(&glyph->advance_y, size);
	uscale(&glyph->w, size);
	uscale(&glyph->h, size);
}

static void
icon_scale(struct font_glyph *glyph, int size) {
	glyph->offset_x = glyph->offset_x * size / FONT_MANAGER_GLYPHSIZE;
	glyph->offset_y = glyph->offset_y * size / FONT_MANAGER_GLYPHSIZE;
	glyph->advance_x = glyph->advance_x * size / FONT_MANAGER_GLYPHSIZE;
	glyph->advance_y = glyph->advance_y * size / FONT_MANAGER_GLYPHSIZE;
	glyph->w = glyph->w * size / FONT_MANAGER_GLYPHSIZE;
	glyph->h = glyph->h * size / FONT_MANAGER_GLYPHSIZE;
}

static const char *
font_manager_update(struct font_manager *F, int fontid, int codepoint, struct font_glyph *glyph, uint8_t *buffer, int stride) {
	if (fontid <= 0)
		return "Invalid font";

	lock(F);
	
	int cp = codepoint_key(fontid, codepoint);
	int slot = hash_lookup(F, cp);
	if (slot < 0) {
		// move last node to head
		slot = F->priority[F->list_head].prev;
		struct priority_list *last_node = &F->priority[slot];
		if (last_node->version == F->version) {	// full ?
			unlock(F);
			return "Too many glyph";
		}
		last_node->version = F->version;
		F->list_head = slot;
		F->slots[slot].codepoint_key = -1;
		hash_insert(F, cp, slot);
	}

	glyph->u = (slot % FONT_MANAGER_SLOTLINE) * FONT_MANAGER_GLYPHSIZE;
	glyph->v = (slot / FONT_MANAGER_SLOTLINE) * FONT_MANAGER_GLYPHSIZE;

	struct font_slot *s = &F->slots[slot];
	s->codepoint_key = cp;
	s->offset_x = glyph->offset_x;
	s->offset_y = glyph->offset_y;
	s->advance_x = glyph->advance_x;
	s->advance_y = glyph->advance_y;
	s->w = glyph->w;
	s->h = glyph->h;
	
	if (fontid == FONT_ICON) {
		if (codepoint < 0 || codepoint >= F->icon_n) {
			unlock(F);
			return "Invalid icon";
		}
		unsigned char * icon_data = F->icon_data;
		unlock(F);
		
		icon_data += codepoint * FONT_MANAGER_GLYPHSIZE * FONT_MANAGER_GLYPHSIZE;
		buffer += stride * glyph->v + glyph->u;
		
		int i;
		for (i=0;i<FONT_MANAGER_GLYPHSIZE;i++) {
			memcpy(buffer, icon_data, FONT_MANAGER_GLYPHSIZE);
			buffer += stride;
			icon_data += FONT_MANAGER_GLYPHSIZE;
		}
		
		return NULL;
	}

	const struct stbtt_fontinfo *fi = get_ttf_unsafe(F, fontid);
	float scale = stbtt_ScaleForMappingEmToPixels(fi, ORIGINAL_SIZE);

	unlock(F);
	
	int width, height, xoff, yoff;

	unsigned char *tmp = stbtt_GetCodepointSDF(fi, scale, codepoint, DISTANCE_OFFSET, ONEDGE_VALUE, PIXEL_DIST_SCALE, &width, &height, &xoff, &yoff);
	if (tmp == NULL) {
		return NULL;
	}
	
	const uint8_t *src = (const uint8_t *)tmp;
	buffer += stride * glyph->v + glyph->u;

	int src_stride = width;
	if (width > FONT_MANAGER_GLYPHSIZE)
		width = FONT_MANAGER_GLYPHSIZE;
	if (height > FONT_MANAGER_GLYPHSIZE)
		height = FONT_MANAGER_GLYPHSIZE;
	if (width > glyph->w)
		width = glyph->w;
	if (height > glyph->h)
		height = glyph->h;

	int i;
	for (i=0;i<height;i++) {
		memcpy(buffer, src, width);
		memset(buffer + width, 0, FONT_MANAGER_GLYPHSIZE - width);
		src += src_stride;
		buffer += stride;
	}
	for (;i<FONT_MANAGER_GLYPHSIZE;i++) {
		memset(buffer, 0, FONT_MANAGER_GLYPHSIZE);
		buffer += stride;
	}

	stbtt_FreeSDF(tmp, fi->userdata);
	
	return NULL;
}

const char *
font_manager_glyph(struct font_manager *F, int fontid, int codepoint, int size, struct font_glyph *g, struct font_glyph *og) {
	int updated = font_manager_touch(F, fontid, codepoint, g);
	*og = *g;
	if (fontid != FONT_ICON && is_space_codepoint(codepoint)){
		updated = 1;	// not need update
		og->w = og->h = 0;
	}
	if (fontid == FONT_ICON) {
		icon_scale(g, size);
	} else {
		font_manager_scale(F, g, size);
	}
	if (updated == 0) {
		const char * err = font_manager_update(F, fontid, codepoint, og, F->texture_buffer, FONT_MANAGER_TEXSIZE);
		if (err) {
			return err;
		}
	}
	return NULL;
}

int
font_manager_flush(struct font_manager *F) {
	// todo : atomic inc
	lock(F);
	int dirty = F->dirty;
	++F->version;
	F->dirty = 0;
	unlock(F);
	return dirty;
}

static void
font_manager_import_unsafe(struct font_manager *F, void* fontdata, size_t sz) {
	truetype_import(F->L, fontdata, sz);
}

void
font_manager_import(struct font_manager *F, void* fontdata, size_t sz) {
	lock(F);
	font_manager_import_unsafe(F, fontdata, sz);
	unlock(F);
}

static int
font_manager_addfont_with_family_unsafe(struct font_manager *F, const char* family) {
	return ttf_with_family(F, family);
}

int
font_manager_addfont_with_family(struct font_manager *F, const char* family) {
	lock(F);
	int r = font_manager_addfont_with_family_unsafe(F, family);
	unlock(F);
	return r;
}

int
font_manager_enum_fontname(struct font_manager *F, int idx, char buffer[], int buf_sz) {
	lock(F);
	int r = truetype_enum(F->L, idx, buffer, buf_sz);
	unlock(F);
	return r;
}

float
font_manager_sdf_mask(struct font_manager *F){
	return (ONEDGE_VALUE) / 255.f;
}

float
font_manager_sdf_distance(struct font_manager *F, uint8_t numpixel){
	return (numpixel * PIXEL_DIST_SCALE) / 255.f;
}

size_t
font_manager_sizeof() {
	return sizeof(struct font_manager);
}

void
font_manager_icon_init(struct font_manager *F, int n, void *data) {
	lock(F);
	F->icon_n = n;
	F->icon_data = (unsigned char *)data;
	unlock(F);
}

void
font_manager_init(struct font_manager *F, void *L) {
	mutex_init(F->mutex);
	F->version = 1;
	F->count = 0;
	F->ttf = NULL;
	F->L = NULL;
	F->dpi_perinch = 0;
	F->dirty = 0;
	F->icon_n = 0;
	F->icon_data = NULL;
// init priority list
	int i;
	for (i=0;i<FONT_MANAGER_SLOTS;i++) {
		F->priority[i].prev = i+1;
		F->priority[i].next = i-1;
	}
	int lastslot = FONT_MANAGER_SLOTS-1;
	F->priority[0].next = lastslot;
	F->priority[lastslot].prev = 0;
	F->list_head = lastslot;
// init hash
	for (i=0;i<FONT_MANAGER_SLOTS;i++) {
		F->slots[i].codepoint_key = -1;
	}
	for (i=0;i<FONT_MANAGER_HASHSLOTS;i++) {
		F->hash[i] = -1;	// empty slot
	}
	memset(F->texture_buffer, 0, sizeof(F->texture_buffer));
	F->ttf = truetype_cstruct(L);
	F->L = L;
}

void*
font_manager_shutdown(struct font_manager *F) {
	lock(F);
	void *L = F->L;
	F->ttf = NULL;
	F->L = NULL;
	unlock(F);
	return L;
}
