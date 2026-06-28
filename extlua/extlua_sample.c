#include <lua.h>
#include <lauxlib.h>
#include <stdint.h>
#include <string.h>

#include "fontapi.h"
#include "materialapi.h"
#include "perspective_quad.glsl.h"

LUA_API void luaapi_init(lua_State *L);
void fontapi_init(lua_State *L);
void materialapi_init(lua_State *L);

#if defined(_WIN32)
#define EXTLUA_EXPORT __declspec(dllexport)
#else
#define EXTLUA_EXPORT __attribute__((visibility("default")))
#endif

#define PQUAD_CORNER_N 4
#define PQUAD_EPSILON 0.000001f
#define PQUAD_DEPTH 460.0f

struct color {
	unsigned char channel[4];
};

struct pquad_payload {
	float sin_angle;
	float cos_angle;
	struct color color;
};

typedef char pquad_payload_size_check[sizeof(struct pquad_payload) == MATERIAL_DATA_SIZE ? 1 : -1];

struct pquad_inst {
	float pos_h0[3];
	float pos_h1[3];
	float pos_h2[3];
	float position[3];
	float uv_rect[4];
	float q[4];
	struct color color;
};

static int material_id = 0;

static void
set_position_homography(const float pos[PQUAD_CORNER_N][2], struct pquad_inst *inst) {
	const float x0 = pos[0][0], y0 = pos[0][1];
	const float x1 = pos[1][0], y1 = pos[1][1];
	const float x2 = pos[2][0], y2 = pos[2][1];
	const float x3 = pos[3][0], y3 = pos[3][1];
	const float sx = x0 - x1 + x3 - x2;
	const float sy = y0 - y1 + y3 - y2;
	const float dx1 = x1 - x3;
	const float dx2 = x2 - x3;
	const float dy1 = y1 - y3;
	const float dy2 = y2 - y3;
	const float det = dx1 * dy2 - dx2 * dy1;
	float m31 = 0.0f;
	float m32 = 0.0f;
	float abs_det = det < 0.0f ? -det : det;
	if (abs_det > PQUAD_EPSILON) {
		m31 = (sx * dy2 - sy * dx2) / det;
		m32 = (sy * dx1 - sx * dy1) / det;
	}
	const float m11 = x1 - x0 + m31 * x1;
	const float m12 = x2 - x0 + m32 * x2;
	const float m13 = x0;
	const float m21 = y1 - y0 + m31 * y1;
	const float m22 = y2 - y0 + m32 * y2;
	const float m23 = y0;
	inst->pos_h0[0] = m11;
	inst->pos_h0[1] = m21;
	inst->pos_h0[2] = m31;
	inst->pos_h1[0] = m12;
	inst->pos_h1[1] = m22;
	inst->pos_h1[2] = m32;
	inst->pos_h2[0] = m13;
	inst->pos_h2[1] = m23;
	inst->pos_h2[2] = 1.0f;
}

static struct color
get_color(lua_State *L, int index) {
	uint32_t color;
	struct color c;
	lua_getfield(L, index, "color");
	color = (uint32_t)luaL_optinteger(L, -1, 0xffffffff);
	if (!(color & 0xff000000)) {
		color |= 0xff000000;
	}
	c.channel[0] = (color >> 16) & 0xff;
	c.channel[1] = (color >> 8) & 0xff;
	c.channel[2] = color & 0xff;
	c.channel[3] = (color >> 24) & 0xff;
	lua_pop(L, 1);
	return c;
}

static float
get_number_field(lua_State *L, int index, const char *field, float defv) {
	lua_getfield(L, index, field);
	float v = luaL_optnumber(L, -1, defv);
	lua_pop(L, 1);
	return v;
}

static material_error
submit_perspective_quad(const struct material_item *item, void *out) {
	if (item->sprite < 0) {
		return "Perspective quad needs a sprite";
	}
	const struct pquad_payload *payload = (const struct pquad_payload *)item->data;
	float depth = PQUAD_DEPTH;
	float focal = depth;
	float s = payload->sin_angle;
	float c = payload->cos_angle;
	float pos[PQUAD_CORNER_N][2];
	struct pquad_inst *inst = (struct pquad_inst *)out;
	int corner;
	for (corner = 0; corner < PQUAD_CORNER_N; corner++) {
		float x = (corner & 1) ? (item->rect.w - item->rect.ox) : -item->rect.ox;
		float y = (corner >> 1) ? (item->rect.h - item->rect.oy) : -item->rect.oy;
		float rx = x * c;
		float rz = -x * s;
		float w = depth + rz;
		if (w <= PQUAD_EPSILON) {
			w = PQUAD_EPSILON;
		}
		float scale = focal / w;
		pos[corner][0] = rx * scale;
		pos[corner][1] = y * scale;
		inst->q[corner] = 1.0f / w;
	}
	set_position_homography(pos, inst);
	inst->position[0] = item->x;
	inst->position[1] = item->y;
	inst->position[2] = (float)item->transform_index;
	inst->uv_rect[0] = item->rect.u;
	inst->uv_rect[1] = item->rect.v;
	inst->uv_rect[2] = item->rect.w;
	inst->uv_rect[3] = item->rect.h;
	inst->color = payload->color;
	return NULL;
}

static void
pipeline_perspective_quad(sg_pipeline_desc *desc) {
	desc->layout.attrs[ATTR_perspective_quad_pos_h0].format = SG_VERTEXFORMAT_FLOAT3;
	desc->layout.attrs[ATTR_perspective_quad_pos_h1].format = SG_VERTEXFORMAT_FLOAT3;
	desc->layout.attrs[ATTR_perspective_quad_pos_h2].format = SG_VERTEXFORMAT_FLOAT3;
	desc->layout.attrs[ATTR_perspective_quad_position].format = SG_VERTEXFORMAT_FLOAT3;
	desc->layout.attrs[ATTR_perspective_quad_uv_rect].format = SG_VERTEXFORMAT_FLOAT4;
	desc->layout.attrs[ATTR_perspective_quad_q].format = SG_VERTEXFORMAT_FLOAT4;
	desc->layout.attrs[ATTR_perspective_quad_color].format = SG_VERTEXFORMAT_UBYTE4N;
}

static const struct material_hook perspective_quad_hooks[] = {
	{ "shader", { .shader = perspective_quad_shader_desc } },
	{ "pipeline", { .pipeline = pipeline_perspective_quad } },
	{ "submit", { .submit = submit_perspective_quad } },
	{ NULL, { NULL } },
};

static int
lset_material_id(lua_State *L) {
	int id = luaL_checkinteger(L, 1);
	if (id <= 0) {
		return luaL_error(L, "Invalid perspective quad material id %d", id);
	}
	material_id = id;
	return 0;
}

static int
lperspective_quad_sprite(lua_State *L) {
	if (material_id <= 0) {
		return luaL_error(L, "Perspective quad material is not registered");
	}
	luaL_checktype(L, 2, LUA_TTABLE);
	struct pquad_payload payload;
	memset(&payload, 0, sizeof(payload));
	payload.sin_angle = get_number_field(L, 2, "sin_angle", 0.0f);
	payload.cos_angle = get_number_field(L, 2, "cos_angle", 1.0f);
	payload.color = get_color(L, 2);
	int sprite = luaL_checkinteger(L, 1) - 1;
	struct material_push_item item = {
		.sprite = sprite,
		.data = &payload,
	};
	return material_push(L, material_id, &item);
}

static int
luaopen_ext_material_perspective_quad(lua_State *L) {
	luaL_checkversion(L);
	luaL_Reg l[] = {
		{ "set_material_id", lset_material_id },
		{ "sprite", lperspective_quad_sprite },
		{ "instance_size", NULL },
		{ "hooks", NULL },
		{ NULL, NULL },
	};
	luaL_newlib(L, l);
	lua_pushinteger(L, sizeof(struct pquad_inst));
	lua_setfield(L, -2, "instance_size");
	material_push_hooks(L, perspective_quad_hooks);
	lua_setfield(L, -2, "hooks");
	return 1;
}

static int
lhello(lua_State *L) {
	lua_pushstring(L, "Hello World From Sample");
	return 1;
}

static int
luaopen_foobar(lua_State *L) {
	luaL_Reg l[] = {
		{ "hello", lhello },
		{ NULL, NULL },
	};
	luaL_newlib(L, l);
	return 1;
}

static void
set_number(lua_State *L, const char *key, lua_Number value) {
	lua_pushnumber(L, value);
	lua_setfield(L, -2, key);
}

static int
lfont_info(lua_State *L) {
	fontapi_font *font = (fontapi_font *)lua_touserdata(L, 1);
	int font_id = luaL_checkinteger(L, 2);
	int size = luaL_checkinteger(L, 3);
	int codepoint = luaL_checkinteger(L, 4);

	struct font_inspect inspect = {
		.font_id = font_id,
		.size = size,
		.codepoint = codepoint,
	};
	union font_inspect_result info;
	union font_inspect_result metrics;
	union font_inspect_result glyph;
	font_error err = font_inspect(font, FONT_INSPECT_INFO, &inspect, &info);
	if (err != NULL) {
		return luaL_error(L, "%s", err);
	}
	err = font_inspect(font, FONT_INSPECT_METRICS, &inspect, &metrics);
	if (err != NULL) {
		return luaL_error(L, "%s", err);
	}
	err = font_inspect(font, FONT_INSPECT_GLYPH_METRICS, &inspect, &glyph);
	if (err != NULL) {
		return luaL_error(L, "%s", err);
	}

	lua_createtable(L, 0, 11);
	lua_pushinteger(L, info.info.texture_size);
	lua_setfield(L, -2, "texture_size");
	set_number(L, "edge", info.info.edge);
	set_number(L, "ascent", metrics.metrics.ascent);
	set_number(L, "descent", metrics.metrics.descent);
	set_number(L, "line_gap", metrics.metrics.line_gap);
	set_number(L, "offset_x", glyph.glyph_metrics.offset_x);
	set_number(L, "offset_y", glyph.glyph_metrics.offset_y);
	set_number(L, "advance_x", glyph.glyph_metrics.advance_x);
	set_number(L, "advance_y", glyph.glyph_metrics.advance_y);
	set_number(L, "width", glyph.glyph_metrics.width);
	set_number(L, "height", glyph.glyph_metrics.height);
	return 1;
}

static int
luaopen_font_probe(lua_State *L) {
	luaL_Reg l[] = {
		{ "info", lfont_info },
		{ NULL, NULL },
	};
	luaL_newlib(L, l);
	return 1;
}

EXTLUA_EXPORT int
extlua_init(lua_State *L) {
	luaapi_init(L);
	fontapi_init(L);
	materialapi_init(L);
	luaL_Reg l[] = {
		{ "ext.foobar", luaopen_foobar },
		{ "ext.font_probe", luaopen_font_probe },
		{ "ext.material.perspective_quad", luaopen_ext_material_perspective_quad },
		{ NULL, NULL },
	};
	luaL_newlib(L, l);
	return 1;
}
