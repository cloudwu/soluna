#include <lua.h>
#include <lauxlib.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "sokol/sokol_gfx.h"
#include "sdftext.glsl.h"
#include "srbuffer.h"
#include "batch.h"
#include "spritemgr.h"
#include "font_manager.h"
#include "sprite_submit.h"
#include "material_util.h"
#include "render_bindings.h"
#include "tmpbuffer.h"

struct text {
	struct draw_primitive_external header;
	int codepoint;
	uint16_t font;
	uint16_t size;
	uint32_t color;
};

struct inst_object {
	float x, y;
	float sr_index;
    uint32_t offset;
    uint32_t u;
    uint32_t v;
};

struct material_text {
	sg_pipeline pip;
	sg_buffer inst;
	struct soluna_render_bindings *bind;
	vs_params_t *uniform;
	struct sr_buffer *srbuffer;
	struct font_manager *font;
	fs_params_t fs_uniform;
	struct tmp_buffer tmp;
};

static void
submit(lua_State *L, void *m_, struct draw_primitive *prim, int n) {
	struct material_text *m = (struct material_text *)m_;
	struct inst_object *tmp = TMPBUFFER_PTR(struct inst_object, &m->tmp);
	int i;
	int count = 0;
	for (i=0;i<n;i++) {
		struct draw_primitive *p = &prim[i*2];
		assert(p->sprite == -MATERIAL_TEXT_NORMAL);
		
		struct text * t = (struct text *)&prim[i*2+1];
		struct font_glyph g, og;
		const char* err = font_manager_glyph(m->font, t->font, t->codepoint, t->size, &g, &og);
		if (err == NULL) {
			tmp[count].offset = (-og.offset_x + 0x8000) << 16 | (-og.offset_y + 0x8000);
			tmp[count].u = og.u << 16 | FONT_MANAGER_GLYPHSIZE;
			tmp[count].v = og.v << 16 | FONT_MANAGER_GLYPHSIZE;
			
			uint32_t scale_fix = og.w == 0 ? 0 : (g.w << 12) / og.w;
			sprite_apply_scale(p, scale_fix);
			// calc scale/rot index
			int sr_index = srbuffer_add(m->srbuffer, p->sr);
			if (sr_index < 0) {
				// todo: support multiply srbuffer
				luaL_error(L, "sr buffer is full");
			}
			tmp[count].x = (float)p->x / 256.0f;
			tmp[count].y = (float)p->y / 256.0f;
			tmp[count].sr_index = (float)sr_index;
			++count;
		} else {
			t->codepoint = -1;
		}
	}
	sg_append_buffer(m->inst, &(sg_range) { tmp , count * sizeof(tmp[0]) });
}

static int
lmateraial_text_submit(lua_State *L) {
	struct material_text *m = (struct material_text *)luaL_checkudata(L, 1, "SOLUNA_MATERIAL_TEXT");
	int batch_n = TMPBUFFER_SIZE(struct inst_object, &m->tmp);
	util_submit_material(L, batch_n, m, submit);
	return 0;
}

static inline void
draw_text(struct material_text *m, uint32_t color, int count, int ex) {
	m->fs_uniform.color = color;
	sg_apply_uniforms(UB_vs_params, &(sg_range){ m->uniform, sizeof(vs_params_t) });
	sg_apply_uniforms(UB_fs_params, &(sg_range){ &m->fs_uniform, sizeof(fs_params_t) });
	if (ex) {
		sg_apply_bindings(&m->bind->bindings);
		sg_draw_ex(0, 4, count, 0, m->bind->base);
	} else {
		size_t base = m->bind->base * sizeof(struct inst_object);
		m->bind->bindings.vertex_buffer_offsets[0] += base;
		sg_apply_bindings(&m->bind->bindings);
		sg_draw(0, 4, count);
		m->bind->bindings.vertex_buffer_offsets[0] -= base;
	}

	m->bind->base += count;
}

static inline int
lmateraial_text_draw_(lua_State *L, int ex) {
	struct material_text *m = (struct material_text *)luaL_checkudata(L, 1, "SOLUNA_MATERIAL_TEXT");
	struct draw_primitive *prim = lua_touserdata(L, 2);
	int prim_n = luaL_checkinteger(L, 3);
	if (prim_n <= 0)
		return 0;
	
	int i;
	float texsize = m->uniform->texsize;
	m->uniform->texsize = 1.0f / FONT_MANAGER_TEXSIZE;
	sg_apply_pipeline(m->pip);
	
	int count = -1;
	uint32_t color = 0;
	for (i=0;i<prim_n;i++) {
		struct text * t = (struct text *)&prim[i*2+1];
		if (t->codepoint >= 0) {
			if (count < 0) {
				color = t->color;
				count = 1;
			} else if (t->color != color) {
				draw_text(m, color, count, ex);
				color = t->color;
				count = 1;
			} else {
				++count;
			}
		}
	}
	draw_text(m, color, count, ex);

	m->uniform->texsize = texsize;

	return 0;
}

static int
lmateraial_text_draw(lua_State *L) {
	return lmateraial_text_draw_(L, 0);
}

static int
lmateraial_text_draw_ex(lua_State *L) {
	return lmateraial_text_draw_(L, 1);
}

static void
init_pipeline(struct material_text *p) {
	sg_pipeline_desc desc = {
		.layout.attrs = {
			[ATTR_texquad_position].format = SG_VERTEXFORMAT_FLOAT3,
			[ATTR_texquad_offset].format = SG_VERTEXFORMAT_UINT,
			[ATTR_texquad_u].format = SG_VERTEXFORMAT_UINT,
			[ATTR_texquad_v].format = SG_VERTEXFORMAT_UINT,
        },
    };
	p->pip = util_make_pipeline(&desc, texquad_shader_desc, "text-pipeline", 1);
}

static int
lnew_material_text_normal(lua_State *L) {
	luaL_checktype(L, 1, LUA_TTABLE);
	struct material_text *m = (struct material_text *)lua_newuserdatauv(L, sizeof(*m), 5);
	util_ref_object(L, &m->inst, 1, "inst_buffer", "SOKOL_BUFFER", 0);
	util_ref_object(L, &m->bind, 2, "bindings", "SOKOL_BINDINGS", 1);
	util_ref_object(L, &m->uniform, 3, "uniform", "SOKOL_UNIFORM", 1);
	util_ref_object(L, &m->srbuffer, 4, "sr_buffer", "SOLUNA_SRBUFFER", 1);
	tmp_buffer_init(L, &m->tmp, 5, "tmp_buffer");
	init_pipeline(m);

	if (lua_getfield(L, 1, "font_manager") != LUA_TLIGHTUSERDATA) {
		return luaL_error(L, "Missing .font_manager");
	}
	m->font = lua_touserdata(L, -1);
	lua_pop(L, 1);
	
  fs_params_t temp = {
      .edge_mask = font_manager_sdf_mask(m->font),
      .dist_multiplier = 1.0f,
      .color = 0xff000000,
  };
  memcpy(&m->fs_uniform, &temp, sizeof(fs_params_t));

	if (luaL_newmetatable(L, "SOLUNA_MATERIAL_TEXT")) {
		luaL_Reg l[] = {
			{ "__index", NULL },
			{ "submit", lmateraial_text_submit },
			{ "draw", DRAWFUNC(lmateraial_text_draw) },
			{ NULL, NULL },
		};
		luaL_setfuncs(L, l, 0);

		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");
	}
	lua_setmetatable(L, -2);
	return 1;
}

static int
lchar_for_batch(lua_State *L) {
	struct text * t = (struct text *)lua_touserdata(L, lua_upvalueindex(1));
	t->header.sprite = -1;
	t->codepoint = luaL_checkinteger(L, 1);
	t->font = luaL_checkinteger(L, 2);
	t->size = luaL_checkinteger(L, 3);
	t->color = luaL_checkinteger(L, 4);
	if (!(t->color & 0xff000000))
		t->color |= 0xff000000;
	lua_pushvalue(L, lua_upvalueindex(1));
	return 1;
}

struct text_primitive {
	struct draw_primitive pos;
	union {
		struct draw_primitive dummy;
		struct text text;
	} u;
};

/*
** From lua utf8lib :
** Decode one UTF-8 sequence, returning NULL if byte sequence is
** invalid.  The array 'limits' stores the minimum value for each
** sequence length, to check for overlong representations. Its first
** entry forces an error for non-ascii bytes with no continuation
** bytes (count == 0).
*/

#define iscont(c)	(((c) & 0xC0) == 0x80)
#define l_uint32 uint32_t
#define MAXUTF		0x7FFFFFFFu

static const char *utf8_decode (const char *s, l_uint32 *val) {
  static const l_uint32 limits[] =
        {~(l_uint32)0, 0x80, 0x800, 0x10000u, 0x200000u, 0x4000000u};
  unsigned int c = (unsigned char)s[0];
  l_uint32 res = 0;  /* final result */
  if (c < 0x80)  /* ascii? */
    res = c;
  else {
    int count = 0;  /* to count number of continuation bytes */
    for (; c & 0x40; c <<= 1) {  /* while it needs continuation bytes... */
      unsigned int cc = (unsigned char)s[++count];  /* read next byte */
      if (!iscont(cc))  /* not a continuation byte? */
        return NULL;  /* invalid byte sequence */
      res = (res << 6) | (cc & 0x3F);  /* add lower 6 bits from cont. byte */
    }
    res |= ((l_uint32)(c & 0x7F) << (count * 5));  /* add first byte */
    if (count > 5 || res > MAXUTF || res < limits[count])
      return NULL;  /* invalid byte sequence */
    s += count;  /* skip continuation bytes read */
  }
  *val = res;
  return s + 1;  /* +1 to include first byte */
}

static const char *
skip_bracket(const char *str) {
	for (;;) {
		if (*str == ']') {
			return str + 1;
		} else if (*str == '\0') {
			return str;
		}
		++str;
	}
}

static int
count_string(const char *str) {
	uint32_t val = 0;
	int n = 0;
	while ((str = utf8_decode(str, &val))) {
		if (val == 0)
			break;
		if (val > 32) {
			if (val == '[') {
				char c = *str;
				if (c == '[') {
					++str;
					++n;
				} else {
					if (c == 'i') {
						// icons
						++n;
					}
					str = skip_bracket(str);
				}
			} else {
				++n;
			}
		}
	}
	return n;
}

#define MAX_WIDTH 4096
#define MAX_HEIGHT 4096
#define DEFAULT_FONTSIZE 24

static void *
free_primitive(void *ud, void *ptr, size_t osize, size_t nsize) {
	free(ptr);
	return NULL;
}

#define ALIGNMENT_LEFT 0
#define ALIGNMENT_CENTER 1
#define ALIGNMENT_RIGHT 2
#define ALIGNMENT_MASK 3
#define VALIGNMENT_TOP (1<<2)
#define VALIGNMENT_CENTER 0
#define VALIGNMENT_BOTTOM (2<<2)
#define VALIGNMENT_MASK (3<<2)

// todo: support multi font/size
struct block_context {
	int width;
	int height;
	int x;
	int y;
	int ascent;
	int decent;
	int line_prim;
	int line_width;
	int alignment;
	uint32_t default_color;
	uint32_t color;
};

static inline int
advance(struct block_context *ctx, int x) {
	if (x + ctx->x > ctx->width)
		return 1;
	ctx->x += x;
	return 0;
}

struct position {
	// input
	int n;
	// output
	int x;
	int y;
	int w;
	int h;
	int decent;
};

static inline int
newline(struct block_context *ctx, struct text_primitive * prim, int n, struct position *pos) {
	int from = ctx->line_prim;
	ctx->line_prim = n;
	int line_width = ctx->line_width;
	ctx->line_width = 0;
	int offx = 0;
	int align = ctx->alignment & ALIGNMENT_MASK;
	switch (align) {
	case ALIGNMENT_CENTER:
		offx = (ctx->width - line_width) / 2 * 256;
		break;
	case ALIGNMENT_RIGHT:
		offx = (ctx->width - line_width) * 256;
		break;
	}
	if (pos && pos->n <= 0 && pos->y == ctx->y) {
		pos->x += offx / 256;
	}
	
	if (prim != NULL && offx > 0) {
		int i;
		for (i=from;i<n;i++) {
			prim[i].pos.x += offx;
		}
	}

	if (ctx->y + ctx->ascent + ctx->decent > ctx->height) {
		return 1;
	} else {
		ctx->y += ctx->ascent + ctx->decent;
		ctx->x = 0;

		return 0;
	}
}

static inline int
tohex(char c) {
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

static const char *
parse_bracket(struct block_context *ctx, const char *str, int *icon) {
	char c = *str;
	int hex = -1;
	if (c == 'i') {
		++str;
		int num = 0;
		while (*str >= '0' && *str <= '9') {
			num = num * 10 + (*str - '0');
			++str;
		}
		*icon = num + 1;
	} else if ((hex = tohex(c)) >= 0) {
		int color = hex;
		for (;;) {
			++str;
			if ((hex = tohex(*str)) >= 0) {
				color = color * 16 + hex;
			} else {
				break;
			}
		}
		if (!(color & 0xff000000))
			color |= 0xff000000;
		ctx->color = color;
	} else if (c == 'n') {
		ctx->color = ctx->default_color;
	}
	// todo: other command
	return skip_bracket(str);
}

static inline void
advance_position(struct position *pos, struct block_context *ctx) {
	if (pos) {
		if (pos->n == 0) {
			pos->x = ctx->x;
			pos->y = ctx->y;
		}
		--pos->n;
	}
}

// todo: support color
static int
ltext_(lua_State *L, struct position *pos) {
	const char * str = luaL_checkstring(L, 1);
	int count = count_string(str);
	struct block_context ctx;
	ctx.width = luaL_optinteger(L, 2, MAX_WIDTH);
	ctx.height = luaL_optinteger(L, 3, MAX_HEIGHT);
	struct font_manager *mgr = (struct font_manager *)lua_touserdata(L, lua_upvalueindex(1));
	int fontid = lua_tointeger(L, lua_upvalueindex(2));
	int fontsize = lua_tointeger(L, lua_upvalueindex(3));
	ctx.default_color = lua_tointeger(L, lua_upvalueindex(4));
	ctx.color = ctx.default_color;
	ctx.x = 0;
	int decent, gap;
	font_manager_fontheight(mgr, fontid, fontsize, &ctx.ascent, &decent, &gap);
	if (gap == 0)
		gap = 1;
	ctx.decent = -decent + gap;
	ctx.y = ctx.ascent;
	ctx.line_prim = 0;
	ctx.line_width = 0;
	ctx.alignment = lua_tointeger(L, lua_upvalueindex(5));
	int pos_n = 0;
	if (pos) {
		if (pos->n < 0) {
			pos->n = 0;
		}
		pos_n = pos->n;
	}
	
	char * buffer = NULL;
	struct text_primitive * prim = NULL;
	if (pos == NULL) {
		buffer = (char *)malloc(count * sizeof(struct text_primitive)+1);
		prim = (struct text_primitive *)buffer;
	}
	int i;
	int n = 0;
	for (i=0;i<count;) {
		uint32_t val = 0;
		str = utf8_decode(str, &val);
		if (val <= 32) {
			advance_position(pos, &ctx);
			if (val == '\n') {
				if (ctx.x > ctx.line_width)
					ctx.line_width = ctx.x;
				if (newline(&ctx, prim, n, pos))
					break;
			} else {
				struct font_glyph g, og;
				if (font_manager_glyph(mgr, fontid, ' ', fontsize, &g, &og) == NULL) {
					if (ctx.x > ctx.line_width)
						ctx.line_width = ctx.x;
					if (advance(&ctx, g.advance_x)) {
						if (newline(&ctx, prim, n, pos))
							break;
						advance(&ctx, g.advance_x);
					}
				}
			}
		} else {
			int icon = 0;
			if (val == '[') {
				if (*str != '[') {
					str = parse_bracket(&ctx, str, &icon);
					if (!icon) {
						continue;
					}
				} else {
					++str;
				}
			}
			int dy = 0;
			int codepoint = val;
			int font = fontid;
			
			if (icon > 0) {
				codepoint = icon -1;
				font = FONT_ICON;
				
				dy = - ( ctx.ascent ) * 256;
			}
			
			if (prim) {
				prim[n].pos.x = ctx.x * 256;
				prim[n].pos.y = ctx.y * 256 + dy;
				prim[n].pos.sr = 0;
				prim[n].pos.sprite = -MATERIAL_TEXT_NORMAL;
			}
			
			struct font_glyph g, og;
			if (font_manager_glyph(mgr, font, codepoint, fontsize, &g, &og) == NULL) {
				advance_position(pos, &ctx);
				if (ctx.x > ctx.line_width)
					ctx.line_width = ctx.x;
				if (advance(&ctx, g.advance_x)) {
					if (newline(&ctx, prim, n, pos))
						break;
					if (prim) {
						prim[n].pos.x = ctx.x * 256;
						prim[n].pos.y = ctx.y * 256 + dy;
					}
					advance(&ctx, g.advance_x);
				}
				if (prim) {
					prim[n].u.text.header.sprite = -1;
					prim[n].u.text.codepoint = codepoint;
					prim[n].u.text.font = font;
					prim[n].u.text.size = fontsize;
					prim[n].u.text.color = ctx.color;
				}
				++n;
			}
			++i;
		}
	}
	if (ctx.x > ctx.line_width)
		ctx.line_width = ctx.x;
	int height = ctx.y + ctx.decent - gap;
	
	if (pos && pos->n >= 0) {
		pos_n -= pos->n;
		pos->n = 0;
		advance_position(pos, &ctx);
	}
	
	newline(&ctx, prim, n, pos);
	int offy;
	int valign = ctx.alignment & VALIGNMENT_MASK;
	switch (valign) {
	case VALIGNMENT_CENTER:
		offy = (ctx.height - height) / 2 * 256;
		break;
	case VALIGNMENT_BOTTOM:
		offy = (ctx.height - height) * 256;
		break;
	default:
		offy = 0;
		break;
	}
	if (prim != NULL) {
		if (offy != 0) {
			for (i=0;i<n;i++) {
				prim[i].pos.y += offy;
			}
		}
		lua_pushexternalstring(L, buffer, n * sizeof(struct text_primitive), free_primitive, NULL);
		lua_pushinteger(L, height);
		return 2;
	} else {
		pos->y += offy / 256 - ctx.ascent;
		pos->w = 2;
		pos->h = ctx.ascent + ctx.decent - gap;
		pos->n = pos_n;
		pos->decent = ctx.decent;
		return 0;
	}
}

static int
ltext(lua_State *L) {
	return ltext_(L, NULL);
}

static int
ltext_position(lua_State *L) {
	struct position pos;
	pos.n = luaL_checkinteger(L, 2);
	pos.x = 0;
	pos.y = 0;
	pos.w = 0;
	pos.h = 0;
	lua_remove(L, 2);
	ltext_(L, &pos);
	lua_pushinteger(L, pos.x);
	lua_pushinteger(L, pos.y);
	lua_pushinteger(L, pos.w);
	lua_pushinteger(L, pos.h);
	lua_pushinteger(L, pos.n);
	lua_pushinteger(L, pos.decent);
	return 6;
}

static uint32_t
parse_alignment(lua_State *L, int index) {
	const char *alignment_string = lua_tostring(L, index);
	int i;
	char c;
	uint32_t alignment = 0;
	for (i=0;(c = alignment_string[i]);i++) {
		switch(c) {
		case 'l' :
		case 'L' :
			alignment |= ALIGNMENT_LEFT;
			break;
		case 'r' :
		case 'R' :
			alignment |= ALIGNMENT_RIGHT;
			break;
		case 'c' :
		case 'C' :
			alignment |= ALIGNMENT_CENTER;
			break;
		case 't' :
		case 'T' :
			alignment |= VALIGNMENT_TOP;
			break;
		case 'v' :
		case 'V' :
			alignment |= VALIGNMENT_CENTER;
			break;
		case 'b' :
		case 'B' :
			alignment |= VALIGNMENT_BOTTOM;
			break;
		}
	}
	return alignment;
}

static int
ltext_block(lua_State *L) {
	luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
	void * font_mgr = lua_touserdata(L, 1);
	int fontid = luaL_checkinteger(L, 2);
	int fontsize = luaL_optinteger(L, 3, DEFAULT_FONTSIZE);
	uint32_t color = luaL_optinteger(L, 4, 0xff000000);
	uint32_t alignment = 0;
	if (lua_type(L, 5) == LUA_TSTRING) {
		alignment = parse_alignment(L, 5);
	}
	if (!(color & 0xff000000))
		color |= 0xff000000;
	lua_pushlightuserdata(L, font_mgr);	// 1
	lua_pushinteger(L, fontid);	// 2
	lua_pushinteger(L, fontsize);	// 3
	lua_pushinteger(L, color);	// 4
	lua_pushinteger(L, alignment);	// 5
	lua_pushcclosure(L, ltext, 5);
	lua_pushlightuserdata(L, font_mgr);	// 1
	lua_pushinteger(L, fontid);	// 2
	lua_pushinteger(L, fontsize);	// 3
	lua_pushinteger(L, color);	// 4
	lua_pushinteger(L, alignment);	// 5
	lua_pushcclosure(L, ltext_position, 5);
	return 2;
}

int
luaopen_material_text(lua_State *L) {
	luaL_checkversion(L);
	luaL_Reg l[] = {
		{ "char", NULL },
		{ "block", ltext_block },
		{ "normal", lnew_material_text_normal },
		{ "instance_size", NULL },
		{ NULL, NULL },
	};
	luaL_newlib(L, l);
	
	// char()
	struct text * t = lua_newuserdatauv(L, sizeof(*t), 1);
	memset(t, 0, sizeof(*t));
	lua_pushinteger(L, MATERIAL_TEXT_NORMAL);
	lua_setiuservalue(L, -2, 1);
	lua_pushcclosure(L, lchar_for_batch, 1);
	lua_setfield(L, -2, "char");
	
	lua_pushinteger(L, sizeof(struct inst_object));
	lua_setfield(L, -2, "instance_size");
	return 1;
}
