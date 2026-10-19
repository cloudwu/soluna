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

#define PIXEL_SCALE 256

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
	struct render_bindings *bind;
	vs_params_t *uniform;
	struct sr_buffer *srbuffer;
	struct font_manager *font;
	fs_params_t fs_uniform;
	struct tmp_buffer tmp;
};

static int material_id = 0;

static void
submit(lua_State *L, void *m_, struct draw_primitive *prim, int n) {
	struct material_text *m = (struct material_text *)m_;
	struct inst_object *tmp = TMPBUFFER_PTR(struct inst_object, &m->tmp);
	int i;
	int count = 0;
	for (i=0;i<n;i++) {
		struct draw_primitive *p = &prim[i*2];
		assert(p->sprite == -material_id);
		
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
			tmp[count].x = (float)p->x / PIXEL_SCALE;
			tmp[count].y = (float)p->y / PIXEL_SCALE;
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
	if (material_id <= 0) {
		return luaL_error(L, "Text material is not registered");
	}
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

static int
lset_material_id(lua_State *L) {
	int id = luaL_checkinteger(L, 1);
	if (id <= 0) {
		return luaL_error(L, "Invalid text material id %d", id);
	}
	material_id = id;
	lua_pushvalue(L, lua_upvalueindex(1));
	lua_pushinteger(L, id);
	lua_setiuservalue(L, -2, 1);
	lua_pop(L, 1);
	return 0;
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
		if (val <= 32) {
			if (val != '\n')
				++n;
		} else {
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

struct font_style {
	uint32_t color;
	int fontid;
	int size;
	// read from font
	int ascent;
	int decent;
	int gap;
};

struct styles {
	struct font_manager *font;
	int n;
	struct font_style s[1];
};

static int
get_field(lua_State *L, const char *key, int def) {
	int t = lua_getfield(L, -1, key);
	if (t == LUA_TNIL) {
		lua_pop(L, 1);
		return def;
	} else if (t != LUA_TNUMBER) {
		return luaL_error(L, ".%s should be integer", key);
	}
	int isnum;
	int v = lua_tointegerx(L, -1, &isnum);
	if (!isnum)
		return luaL_error(L, ".%s should be integer", key);
	lua_pop(L, 1);
	return v;
}

static void
style_getinfo(struct font_manager *mgr, struct font_style *fs, int line_height) {
	int ascent, decent, gap;
	font_manager_fontheight(mgr, fs->fontid, fs->size, &ascent, &decent, &gap);
	if (gap == 0)
		gap = 1;
	int natural_decent = -decent;
	int natural_height = ascent + natural_decent;
	if (line_height > 0) {
		if (line_height < natural_height) {
			line_height = natural_height;
		}
		int leading = line_height - natural_height;
		int leading_top = leading / 2;
		fs->ascent = ascent + leading_top;
		fs->decent = natural_decent + leading - leading_top;
		fs->gap = 0;
	} else {
		fs->ascent = ascent;
		fs->decent = natural_decent + gap;
		fs->gap = gap;
	}
}

static void
read_style(lua_State *L, int index, struct styles *s, int i) {
	struct font_style *fs = &s->s[i];
	int line_height;
	if (lua_geti(L, index, i+1) != LUA_TTABLE) {
		luaL_error(L, "Invalid style at %d, need a table", i+1);
	}
	fs->fontid = get_field(L, "font", -1);
	if (fs->fontid < 0)
		luaL_error(L, "Invalid .font");
	fs->color = get_field(L, "color", 0xff000000);
	if (!(fs->color & 0xff000000))
		fs->color |= 0xff000000;
	fs->size = get_field(L, "size", DEFAULT_FONTSIZE);
	line_height = get_field(L, "line_height", 0);
	style_getinfo(s->font, fs, line_height);
	lua_pop(L, 1);
}

static int
ltext_styles(lua_State *L) {
	luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
	struct font_manager *mgr = (struct font_manager *)lua_touserdata(L, 1);
	luaL_checktype(L, 2, LUA_TTABLE);
	lua_len(L, 2);
	int n = lua_tointeger(L, -1);
	lua_pop(L, 1);
	if (n <= 0) {
		return luaL_error(L, "At least one style");
	}
	struct styles * s = (struct styles *)lua_newuserdatauv(L, sizeof(struct styles) + (n-1) * sizeof(struct font_style), 0);
	s->font = mgr;
	s->n = n;
	int i;
	for (i=0;i<n;i++) {
		read_style(L, 2, s, i);
	}
	if (luaL_newmetatable(L, "SOLUNA_TEXT_STYLES")) {
		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");
	}
	lua_setmetatable(L, -2);
	return 1;
}

struct block_context {
	struct styles *s;
	struct font_style *fs;
	int width;
	int height;
	int x;
	int y;
	uint32_t color;
	int line_prim;
	int line_width;
	int line_ascent;
	int line_decent;
	int line_height;
	int alignment;
};

struct line_info {
	int y;
	int h;
};

struct position {
	int x;
	int w;
	int style;
	int line;
};

struct layout {
	int n;
	int cap;
	int width;
	int height;
	int top;
	int text_height;
	int line_count;
	struct font_style *styles;
	struct font_style style;
};

static inline int
newline(struct block_context *ctx, struct text_primitive * prim, int n, struct layout *pos);

static inline struct position *
layout_positions(struct layout *pos) {
	return (struct position *)(pos + 1);
}

static inline struct line_info *
layout_lines(struct layout *pos) {
	return (struct line_info *)(layout_positions(pos) + pos->cap);
}

static inline size_t
layout_size(int cap) {
	return sizeof(struct layout)
		+ (size_t)cap * sizeof(struct position)
		+ (size_t)cap * sizeof(struct line_info);
}

static inline int
layout_line_bottom(struct layout *pos, struct line_info *lines, int line) {
	if (line + 1 < pos->line_count) {
		return lines[line + 1].y;
	}
	return pos->top + pos->text_height;
}

static void
set_layout_styles(lua_State *L, struct layout *pos, struct styles *styles, int external) {
	if (external) {
		pos->styles = styles->s;
		lua_pushvalue(L, lua_upvalueindex(1));
		lua_setiuservalue(L, -2, 1);
	} else {
		pos->style = styles->s[0];
		pos->styles = &pos->style;
	}
}

static inline void
set_position(struct layout *pos, int n, struct block_context *ctx) {
	struct position *p = &layout_positions(pos)[n];
	p->x = ctx->x;
	p->w = 0;
	p->style = (int)(ctx->fs - ctx->s->s);
	p->line = 0;
	pos->n = n + 1;
}

static inline int
advance(struct layout *pos, struct block_context *ctx, int x) {
	if (pos && pos->n > 0) {
		layout_positions(pos)[pos->n-1].w = x;
	}
	if (x + ctx->x > ctx->width)
		return 1;
	int ascent = ctx->fs->ascent;
	if (ascent > ctx->line_ascent)
		ctx->line_ascent = ascent;
	int decent = ctx->fs->decent;
	if (decent > ctx->line_decent)
		ctx->line_decent = decent;
	int height = ctx->fs->ascent + ctx->fs->decent - ctx->fs->gap;
	if (height > ctx->line_height)
		ctx->line_height = height;
	ctx->x += x;
	return 0;
}

static inline int
advance_space(struct block_context *ctx, struct text_primitive *prim, struct layout *pos, int *n, int advance_x) {
	if (pos) {
		set_position(pos, *n, ctx);
	}
	if (advance(pos, ctx, advance_x)) {
		if (newline(ctx, prim, *n, pos))
			return 1;
		if (pos) {
			set_position(pos, *n, ctx);
		}
		advance(pos, ctx, advance_x);
	}
	if (pos) {
		++*n;
	}
	return 0;
}

static inline int
newline(struct block_context *ctx, struct text_primitive * prim, int n, struct layout *pos) {
	int from = ctx->line_prim;
	ctx->line_prim = n;
	int line_width = ctx->line_width;
	int line_ascent = ctx->line_ascent;
	int line_decent = ctx->line_decent;
	int line_height = ctx->line_height;
	ctx->line_width = 0;
	ctx->line_height = 0;
	ctx->line_ascent = 0;
	ctx->line_decent = 0;
	int offx = 0;
	int align = ctx->alignment & ALIGNMENT_MASK;
	switch (align) {
	case ALIGNMENT_CENTER:
		offx = (ctx->width - line_width) / 2;
		break;
	case ALIGNMENT_RIGHT:
		offx = (ctx->width - line_width);
		break;
	}

	int i;
	if (prim != NULL) {
		int dy = line_ascent * PIXEL_SCALE;
		if (offx > 0) {
			int dx = offx * PIXEL_SCALE;
			for (i=from;i<n;i++) {
				prim[i].pos.x += dx;
				prim[i].pos.y += dy;
			}
		} else {
			for (i=from;i<n;i++) {
				prim[i].pos.y += dy;
			}
		}
	}
	if (pos != NULL) {
		if (n > from) {
			struct position *positions = layout_positions(pos);
			struct line_info *lines = layout_lines(pos);
			int line = pos->line_count++;
			lines[line].y = ctx->y;
			lines[line].h = line_height;
			for (i=from;i<n;i++) {
				positions[i].line = line;
			}
		}
		if (offx > 0) {
			struct position *positions = layout_positions(pos);
			for (i=from;i<n;i++) {
				positions[i].x += offx;
			}
		}
	}

	if (ctx->y + line_ascent + line_decent > ctx->height) {
		return 1;
	} else {
		ctx->y += line_ascent + line_decent;
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
parse_bracket(struct block_context *ctx, const char *str, int *icon, int *group) {
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
	} else if (c == 's') {
		++str;
		int style = 0;
		while (*str >= '0' && *str <= '9') {
			style = style * 10 + (*str - '0');
			++str;
		}
		int n = ctx->s->n;
		if (style < 0 || style >=n) {
			style = 0;
		}
		ctx->fs = &ctx->s->s[style];
		ctx->color = ctx->fs->color;
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
		ctx->fs = &ctx->s->s[0];
		ctx->color = ctx->fs->color;
	} else if (c == 'w') {
		++str;
		int word_group = 0;
		while (*str >= '0' && *str <= '9') {
			word_group = word_group * 10 + (*str - '0');
			++str;
		}
		*group = word_group;
	}
	// todo: other command
	return skip_bracket(str);
}

enum text_token_type {
	TEXT_TOKEN_END,
	TEXT_TOKEN_GROUP,
	TEXT_TOKEN_NEWLINE,
	TEXT_TOKEN_SPACE,
	TEXT_TOKEN_GLYPH,
};

struct text_token {
	enum text_token_type type;
	int advance_x;
	int dy;
	int codepoint;
	int font;
};

static const char *
next_text_token(const char *str, struct block_context *ctx, int *group,
	struct text_token *token) {
	struct font_manager *mgr = ctx->s->font;
	for (;;) {
		uint32_t val = 0;
		str = utf8_decode(str, &val);
		if (str == NULL || val == 0) {
			token->type = TEXT_TOKEN_END;
			return str;
		}
		if (val <= 32) {
			if (val == '\n') {
				token->type = TEXT_TOKEN_NEWLINE;
				return str;
			}
			struct font_glyph g, og;
			if (font_manager_glyph(mgr, ctx->fs->fontid, ' ', ctx->fs->size, &g, &og) == NULL) {
				token->type = TEXT_TOKEN_SPACE;
				token->advance_x = g.advance_x;
				return str;
			}
			continue;
		}

		int icon = 0;
		if (val == '[') {
			if (*str != '[') {
				int previous_group = *group;
				str = parse_bracket(ctx, str, &icon, group);
				if (*group != previous_group) {
					token->type = TEXT_TOKEN_GROUP;
					return str;
				}
				if (!icon)
					continue;
			} else {
				++str;
			}
		}

		int codepoint = val;
		int font = ctx->fs->fontid;
		int dy = 0;
		if (icon > 0) {
			codepoint = icon - 1;
			font = FONT_ICON;
			dy = -ctx->fs->ascent;
		}
		struct font_glyph g, og;
		if (font_manager_glyph(mgr, font, codepoint, ctx->fs->size, &g, &og) == NULL) {
			token->type = TEXT_TOKEN_GLYPH;
			token->advance_x = g.advance_x;
			token->dy = dy;
			token->codepoint = codepoint;
			token->font = font;
			return str;
		}
	}
}

static int
measure_word_group(const char *str, struct block_context *ctx, int group) {
	struct block_context probe = *ctx;
	int current_group = group;
	int width = 0;
	for (;;) {
		struct text_token token;
		str = next_text_token(str, &probe, &current_group, &token);
		if (token.type == TEXT_TOKEN_END || token.type == TEXT_TOKEN_GROUP ||
			token.type == TEXT_TOKEN_NEWLINE)
			break;
		if (token.type == TEXT_TOKEN_SPACE || token.type == TEXT_TOKEN_GLYPH) {
			width += token.advance_x;
			if (width > probe.width)
				break;
		}
	}
	return width;
}

static int
ltext_(lua_State *L, struct styles *s, int gen_layout, int external) {
	const char * str = luaL_checkstring(L, 1);
	int count = count_string(str);
	struct block_context ctx;
	ctx.s = s;
	ctx.fs = &s->s[0];
	ctx.width = luaL_optinteger(L, 2, MAX_WIDTH);
	ctx.height = luaL_optinteger(L, 3, MAX_HEIGHT);

	ctx.x = 0;
	ctx.color = s->s[0].color;
	ctx.y = 0;
	ctx.line_prim = 0;
	ctx.line_width = 0;
	ctx.line_ascent = 0;
	ctx.line_decent = 0;
	ctx.line_height = 0;
	ctx.alignment = lua_tointeger(L, lua_upvalueindex(5));

	struct text_primitive * prim = NULL;
	struct layout * pos = NULL;
	if (gen_layout) {
		int cap = count + 1;
		pos = (struct layout *)lua_newuserdatauv(L, layout_size(cap), external ? 1 : 0);
		pos->n = 0;
		pos->cap = cap;
		pos->width = ctx.width;
		pos->height = ctx.height;
		pos->text_height = 0;
		pos->top = 0;
		pos->line_count = 0;
		set_layout_styles(L, pos, s, external);
	} else {
		prim = (struct text_primitive *)malloc((size_t)count * sizeof(*prim) + 1);
		if (prim == NULL)
			return luaL_error(L, "Out of memory");
	}
	int n = 0;
	int group = 0;
	for (;;) {
		struct text_token token;
		str = next_text_token(str, &ctx, &group, &token);
		if (token.type == TEXT_TOKEN_END)
			break;
		if (token.type == TEXT_TOKEN_GROUP) {
			if (group > 0 && ctx.x > 0) {
				int group_width = measure_word_group(str, &ctx, group);
				if (group_width <= ctx.width &&
					ctx.x + group_width > ctx.width) {
					if (ctx.x > ctx.line_width)
						ctx.line_width = ctx.x;
					if (newline(&ctx, prim, n, pos))
						break;
				}
			}
			continue;
		}
		if (token.type == TEXT_TOKEN_NEWLINE) {
			if (ctx.x > ctx.line_width)
				ctx.line_width = ctx.x;
			if (newline(&ctx, prim, n, pos))
				break;
			continue;
		}
		if (token.type == TEXT_TOKEN_SPACE) {
			if (ctx.x > ctx.line_width)
				ctx.line_width = ctx.x;
			if (advance_space(&ctx, prim, pos, &n, token.advance_x))
				break;
			continue;
		}

		if (prim) {
			prim[n].pos.x = ctx.x * PIXEL_SCALE;
			prim[n].pos.y = (ctx.y + token.dy) * PIXEL_SCALE;
			prim[n].pos.sr = 0;
			prim[n].pos.sprite = -material_id;
		} else {
			// assert(pos != NULL)
			set_position(pos, n, &ctx);
		}

		if (ctx.x > ctx.line_width)
			ctx.line_width = ctx.x;
		if (advance(pos, &ctx, token.advance_x)) {
			if (newline(&ctx, prim, n, pos))
				break;
			if (prim) {
				prim[n].pos.x = ctx.x * PIXEL_SCALE;
				prim[n].pos.y = (ctx.y + token.dy) * PIXEL_SCALE;
			} else {
				set_position(pos, n, &ctx);
			}
			advance(pos, &ctx, token.advance_x);
		}
		if (prim) {
			prim[n].u.text.header.sprite = -1;
			prim[n].u.text.codepoint = token.codepoint;
			prim[n].u.text.font = token.font;
			prim[n].u.text.size = ctx.fs->size;
			prim[n].u.text.color = ctx.color;
		}
		++n;
	}
	if (ctx.x > ctx.line_width)
		ctx.line_width = ctx.x;
	int height = ctx.y + ctx.line_height;
	if (n == 0 && pos) {
		// no text, set one
		if (pos) {
			set_position(pos, 0, &ctx);
			ctx.line_ascent = ctx.fs->ascent;
			ctx.line_decent = ctx.fs->decent;
			ctx.line_height = ctx.fs->ascent + ctx.fs->decent - ctx.fs->gap;
			height = ctx.y + ctx.line_height;
		}
		newline(&ctx, prim, 1, pos);
	} else {
		newline(&ctx, prim, n, pos);
	}
	if (pos && n>0) {
		struct position *positions = layout_positions(pos);
		positions[n] = positions[n-1];
		positions[n].x = positions[n-1].x + positions[n-1].w;
		positions[n].w = 0;
	}
	int offy;
	int valign = ctx.alignment & VALIGNMENT_MASK;
	int i;
	switch (valign) {
	case VALIGNMENT_CENTER:
		offy = (ctx.height - height) / 2 * PIXEL_SCALE;
		break;
	case VALIGNMENT_BOTTOM:
		offy = (ctx.height - height) * PIXEL_SCALE;
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
		lua_pushexternalstring(L, (char *)prim,
			n * sizeof(struct text_primitive), free_primitive, NULL);
		lua_pushinteger(L, height);
		return 2;
	} else {
		pos->text_height = height;
		pos->n = n;
		if (offy != 0) {
			int offset = offy / PIXEL_SCALE;
			struct line_info *lines = layout_lines(pos);
			for (i=0;i<pos->line_count;i++) {
				lines[i].y += offset;
			}
			pos->top = offset;
		}
		return 1;
	}
}

static struct styles *
get_styles(lua_State *L, struct styles *tmp) {
	void * ptr = lua_touserdata(L, lua_upvalueindex(1));
	int fontid = lua_tointeger(L, lua_upvalueindex(2));
	if (fontid == 0) {
		return (struct styles *)ptr;
	}
	tmp->font = (struct font_manager *)ptr;
	tmp->n = 1;
	struct font_style *fs = &tmp->s[0];
	fs->fontid = fontid;
	fs->size = lua_tointeger(L, lua_upvalueindex(3));
	fs->color = lua_tointeger(L, lua_upvalueindex(4));

	style_getinfo(tmp->font, fs, 0);
	return tmp;
}

static int
ltext(lua_State *L) {
	struct styles tmp;
	return ltext_(L, get_styles(L, &tmp), 0, 0);
}

static int
ltext_layout(lua_State *L) {
	struct styles tmp;
	int external = luaL_testudata(L, lua_upvalueindex(1), "SOLUNA_TEXT_STYLES") != NULL;
	ltext_(L, get_styles(L, &tmp), 1, external);
	lua_pushvalue(L, lua_upvalueindex(6));
	lua_setmetatable(L, -2);
	return 1;
}

static int
ltext_cursor(lua_State *L) {
	struct layout * pos = (struct layout *)lua_touserdata(L, 1);
	int n = luaL_checkinteger(L, 2);
	if (n < 0) {
		n = 0;
	} else if (n > pos->n ) {
		n = pos->n;
	}
	struct position *p = &layout_positions(pos)[n];
	struct font_style *style = &pos->styles[p->style];
	struct line_info *line = &layout_lines(pos)[p->line];
	int height = style->ascent + style->decent - style->gap;
	lua_pushinteger(L, p->x);
	lua_pushinteger(L, line->y + line->h - height);
	lua_pushinteger(L, 2);
	lua_pushinteger(L, height);
	lua_pushinteger(L, n);
	lua_pushinteger(L, style->decent);
	return 6;
}

static int
ltext_height(lua_State *L) {
	struct layout * pos = (struct layout *)lua_touserdata(L, 1);
	lua_pushinteger(L, pos->text_height);
	return 1;
}

static int
ltext_line_height(lua_State *L) {
	struct layout * pos = (struct layout *)lua_touserdata(L, 1);
	int n = pos->n;
	if (n <= 0) {
		lua_pushinteger(L, pos->text_height);
		return 1;
	}
	int height = 0;
	int i;
	struct line_info *lines = layout_lines(pos);
	for (i=0;i<pos->line_count;i++) {
		struct line_info *line = &lines[i];
		int line_height = layout_line_bottom(pos, lines, i) - line->y;
		if (line_height > height) {
			height = line_height;
		}
	}
	lua_pushinteger(L, height);
	return 1;
}

static int
ltext_line_count(lua_State *L) {
	struct layout * pos = (struct layout *)lua_touserdata(L, 1);
	int n = pos->n;
	if (n <= 0) {
		lua_pushinteger(L, pos->text_height > 0 ? 1 : 0);
		return 1;
	}
	lua_pushinteger(L, pos->line_count);
	return 1;
}

static int
ltext_cursor_count(lua_State *L) {
	struct layout * pos = (struct layout *)lua_touserdata(L, 1);
	lua_pushinteger(L, pos->n + 1);
	return 1;
}

static void
set_integer_field(lua_State *L, const char *key, int value) {
	lua_pushinteger(L, value);
	lua_setfield(L, -2, key);
}

static void
push_line(lua_State *L, struct layout *pos, int start, int finish) {
	struct position *positions = layout_positions(pos);
	struct line_info *lines = layout_lines(pos);
	int x0 = positions[start].x;
	int x1 = x0;
	int y = lines[positions[start].line].y;
	int y1 = y;
	int text_bottom = pos->top + pos->text_height;
	int i;
	for (i=start;i<finish;i++) {
		struct position *p = &positions[i];
		if (p->x < x0) {
			x0 = p->x;
		}
		int right = p->x + p->w;
		if (right > x1) {
			x1 = right;
		}
		int bottom = layout_line_bottom(pos, lines, p->line);
		if (bottom > y1) {
			y1 = bottom;
		}
	}
	if (y1 > text_bottom) {
		y1 = text_bottom;
	}
	lua_createtable(L, 0, 6);
	set_integer_field(L, "start", start);
	set_integer_field(L, "finish", finish);
	set_integer_field(L, "x", x0);
	set_integer_field(L, "y", y);
	set_integer_field(L, "width", x1 - x0);
	set_integer_field(L, "height", y1 > y ? y1 - y : 0);
}

static int
ltext_line(lua_State *L) {
	struct layout * pos = (struct layout *)lua_touserdata(L, 1);
	int line = luaL_checkinteger(L, 2);
	if (line <= 0) {
		lua_pushnil(L);
		return 1;
	}
	int n = pos->n;
	if (n <= 0) {
		if (line != 1) {
			lua_pushnil(L);
			return 1;
		}
		lua_createtable(L, 0, 6);
		set_integer_field(L, "start", 0);
		set_integer_field(L, "finish", 0);
		set_integer_field(L, "x", 0);
		set_integer_field(L, "y", pos->top);
		set_integer_field(L, "width", 0);
		set_integer_field(L, "height", pos->text_height);
		return 1;
	}
	int target = line - 1;
	if (target >= pos->line_count) {
		lua_pushnil(L);
		return 1;
	}
	int start = -1;
	int finish = -1;
	struct position *positions = layout_positions(pos);
	int i;
	for (i=0;i<n;i++) {
		if (positions[i].line == target) {
			if (start < 0) {
				start = i;
			}
			finish = i + 1;
		} else if (start >= 0) {
			break;
		}
	}
	if (start >= 0) {
		push_line(L, pos, start, finish);
		return 1;
	}
	lua_pushnil(L);
	return 1;
}

static int
ltext_style(lua_State *L) {
	struct layout * pos = (struct layout *)lua_touserdata(L, 1);
	int n = luaL_checkinteger(L, 2);
	if (n < 0 || n >= pos->n) {
		lua_pushnil(L);
		return 1;
	}
	lua_pushinteger(L, layout_positions(pos)[n].style);
	return 1;
}

static int
get_pos(lua_State *L, int index) {
	int isnum;
	int r = lua_tointegerx (L, index, &isnum);
	if (isnum) {
		return r;
	}
	return (int)(luaL_checknumber(L, index) + 0.5);
}

static int
bsearch_pos(struct layout * pos, int x, int y) {
	int from = 0;
	int to = pos->n;
	struct position *positions = layout_positions(pos);
	struct line_info *lines = layout_lines(pos);
	while (from < to) {
		int mid = from + (to - from) / 2;
		struct position *p = &positions[mid];
		struct line_info *line = &lines[p->line];
		if (y < line->y) {
			to = mid;
		} else if (y >= layout_line_bottom(pos, lines, p->line)) {
			from = mid + 1;
		} else if (x < p->x) {
			to = mid;
		} else if (x >= p->x + p->w) {
			from = mid + 1;
		} else {
			return mid;
		}
	}
	return from;
}

static int
ltext_hit_test(lua_State *L) {
	struct layout * pos = (struct layout *)lua_touserdata(L, 1);
	int x = get_pos(L, 2);
	int y = get_pos(L, 3);
	int top = pos->top;
	if (y < top) {
		lua_pushinteger(L, 0);
		lua_pushboolean(L, y < 0);	// out of box
		return 2;
	}
	if (y - top >= pos->text_height) {
		lua_pushinteger(L, pos->n);
		lua_pushboolean(L, y >= pos->height);	// out of box
		return 2;
	}
	int index = bsearch_pos(pos, x, y);
	lua_pushinteger(L, index);
	lua_pushboolean(L, (x < 0 || x >= pos->width));
	return 2;
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
	if (material_id <= 0) {
		return luaL_error(L, "Text material is not registered");
	}
	int fontid = 0;
	int fontsize = 0;
	uint32_t color = 0;
	uint32_t alignment = 0;

	if (luaL_testudata(L, 1, "SOLUNA_TEXT_STYLES")) {
		if (lua_type(L, 2) == LUA_TSTRING) {
			alignment = parse_alignment(L, 2);
		} else {
			luaL_opt(L, luaL_checkstring, 2, NULL);
		}
	} else {
		luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
		fontid = luaL_checkinteger(L, 2);
		fontsize = luaL_optinteger(L, 3, DEFAULT_FONTSIZE);
		color = luaL_optinteger(L, 4, 0xff000000);
		
		if (lua_type(L, 5) == LUA_TSTRING) {
			alignment = parse_alignment(L, 5);
		}
		if (!(color & 0xff000000))
			color |= 0xff000000;
	}

	lua_pushvalue(L, 1);
	lua_pushinteger(L, fontid);	// 2
	lua_pushinteger(L, fontsize);	// 3
	lua_pushinteger(L, color);	// 4
	lua_pushinteger(L, alignment);	// 5
	lua_pushcclosure(L, ltext, 5);
	lua_pushvalue(L, 1);
	lua_pushinteger(L, fontid);	// 2
	lua_pushinteger(L, fontsize);	// 3
	lua_pushinteger(L, color);	// 4
	lua_pushinteger(L, alignment);	// 5
	lua_pushvalue(L, lua_upvalueindex(1));	// metatable
	lua_pushcclosure(L, ltext_layout, 6);
	return 2;
}

int
luaopen_material_text(lua_State *L) {
	luaL_checkversion(L);
	luaL_Reg l[] = {
		{ "char", NULL },
		{ "set_material_id", NULL },
		{ "block", ltext_block },
		{ "normal", lnew_material_text_normal },
		{ "instance_size", NULL },
		{ "styles", ltext_styles },
		{ NULL, NULL },
	};
	luaL_newlib(L, l);
	
	if (luaL_newmetatable(L, "SOLUNA_TEXT_LAYOUT")) {
		luaL_Reg meta[] = {
			{ "height", ltext_height },
			{ "line_height", ltext_line_height },
			{ "line_count", ltext_line_count },
			{ "cursor_count", ltext_cursor_count },
			{ "line", ltext_line },
			{ "cursor", ltext_cursor },
			{ "hit_test", ltext_hit_test },
			{ "style", ltext_style },
			{ NULL, NULL },
		};
		luaL_setfuncs(L, meta, 0);
		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");
	}
	lua_pushcclosure(L, ltext_block, 1);
	lua_setfield(L, -2, "block");

	// char()
	struct text * t = lua_newuserdatauv(L, sizeof(*t), 1);
	memset(t, 0, sizeof(*t));
	lua_pushvalue(L, -1);
	lua_pushcclosure(L, lset_material_id, 1);
	lua_setfield(L, -3, "set_material_id");
	lua_pushcclosure(L, lchar_for_batch, 1);
	lua_setfield(L, -2, "char");
	
	lua_pushinteger(L, sizeof(struct inst_object));
	lua_setfield(L, -2, "instance_size");
	return 1;
}
