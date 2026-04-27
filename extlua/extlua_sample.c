#include <lua.h>
#include <lauxlib.h>
#include <stdint.h>
#include <stddef.h>

#include "sokol/sokol_gfx.h"
#include "perspective_quad.glsl.h"
#include "solunaapi.h"

LUA_API void luaapi_init(lua_State *L);
void sokolapi_init(lua_State *L);

#if defined(_WIN32)
#define EXTLUA_EXPORT __declspec(dllexport)
#else
#define EXTLUA_EXPORT __attribute__((visibility("default")))
#endif

#define PQUAD_CORNER_N 4
#define PQUAD_INFO_CORNER_MASK 0x3u
#define PQUAD_INFO_USE_SPRITE_RECT 0x4u
#define PQUAD_EPSILON 0.000001f

struct color {
	unsigned char channel[4];
};

struct pquad_payload {
	uint32_t info;
	float q;
	struct color color;
};

struct pquad_inst {
	float pos_h0[3];
	float pos_h1[3];
	float pos_h2[3];
	float uv_rect[4];
	float q[4];
	struct color color;
};

struct material_perspective_quad {
	sg_pipeline pip;
	sg_buffer inst;
	void *bind;
	int base;
	vs_params_t *uniform;
	void *bank;
	void *tmp_ptr;
	size_t tmp_size;
};

struct sprite_rect_basis {
	float scale_x;
	float scale_y;
	float shear_x;
	float shear_y;
	float tx;
	float ty;
};

static int material_id = 0;

static sg_pipeline
make_pipeline(sg_pipeline_desc *desc) {
	sg_shader shd = sg_make_shader(perspective_quad_shader_desc(sg_query_backend()));
	desc->shader = shd;
	desc->primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP;
	desc->label = "extlua-perspective-quad-pipeline";
	desc->layout.buffers[0].step_func = SG_VERTEXSTEP_PER_INSTANCE;
	desc->colors[0].blend = (sg_blend_state) {
		.enabled = true,
		.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
		.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
		.src_factor_alpha = SG_BLENDFACTOR_ONE,
		.dst_factor_alpha = SG_BLENDFACTOR_ZERO,
	};
	return sg_make_pipeline(desc);
}

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

static inline int
perspective_quad_count(lua_State *L, int prim_n) {
	if (prim_n % PQUAD_CORNER_N != 0) {
		luaL_error(L, "Invalid perspective quad primitive count %d", prim_n);
		return 0;
	}
	return prim_n / PQUAD_CORNER_N;
}

static inline void
decode_sprite_rect_basis(struct sprite_rect_basis *basis, uint32_t corner, const struct soluna_material_stream_data *item) {
	float x = item->x;
	float y = item->y;
	switch (corner) {
	case 0:
		basis->scale_x = x;
		basis->scale_y = y;
		break;
	case 1:
		basis->shear_x = x;
		basis->shear_y = y;
		break;
	case 2:
		basis->tx = x;
		basis->ty = y;
		break;
	}
}

static inline void
build_quad_from_rect(const struct sprite_rect_basis *basis, const struct soluna_sprite_rect *rect, float pos[PQUAD_CORNER_N][2]) {
	float scale_x = basis->scale_x - basis->tx;
	float scale_y = basis->scale_y - basis->ty;
	float shear_x = basis->shear_x - basis->tx;
	float shear_y = basis->shear_y - basis->ty;
	int corner;
	for (corner=0; corner<PQUAD_CORNER_N; corner++) {
		float x = (corner & 1) ? (rect->w - rect->ox) : -rect->ox;
		float y = (corner >> 1) ? (rect->h - rect->oy) : -rect->oy;
		pos[corner][0] = x * scale_x + y * shear_x + basis->tx;
		pos[corner][1] = x * shear_y + y * scale_y + basis->ty;
	}
}

static void
submit(lua_State *L, void *m_, const void *stream, int n) {
	struct material_perspective_quad *m = (struct material_perspective_quad *)m_;
	struct pquad_inst *tmp = (struct pquad_inst *)m->tmp_ptr;
	int out_n = perspective_quad_count(L, n);
	int i;
	for (i=0; i<out_n; i++) {
		struct pquad_inst *inst = &tmp[i];
		int base = i * PQUAD_CORNER_N;
		int j;
		int sprite = -1;
		uint32_t stream_flags = 0;
		struct sprite_rect_basis sprite_rect_basis = { 0 };
		float pos[PQUAD_CORNER_N][2];
		for (j=0; j<PQUAD_CORNER_N; j++) {
			struct soluna_material_stream_data item;
			struct pquad_payload payload;
			soluna_material_stream_read(L, stream, base + j, material_id, sizeof(payload), &payload, &item);
			uint32_t flags = payload.info & PQUAD_INFO_USE_SPRITE_RECT;
			if (j == 0) {
				sprite = item.sprite;
				stream_flags = flags;
				inst->color = payload.color;
			} else if (item.sprite != sprite || flags != stream_flags) {
				luaL_error(L, "Invalid perspective quad stream");
			}
			uint32_t corner = payload.info & PQUAD_INFO_CORNER_MASK;
			if (corner >= PQUAD_CORNER_N) {
				luaL_error(L, "Invalid perspective quad corner %u", corner);
			}
			if (stream_flags & PQUAD_INFO_USE_SPRITE_RECT) {
				decode_sprite_rect_basis(&sprite_rect_basis, corner, &item);
			} else {
				pos[corner][0] = item.x;
				pos[corner][1] = item.y;
			}
			float q = payload.q;
			if (q <= PQUAD_EPSILON) {
				q = PQUAD_EPSILON;
			}
			inst->q[corner] = q;
		}
		struct soluna_sprite_rect sprite_rect;
		if (!soluna_material_sprite_rect(L, m->bank, sprite, &sprite_rect)) {
			luaL_error(L, "Invalid perspective quad sprite %d", sprite);
		}
		if (stream_flags & PQUAD_INFO_USE_SPRITE_RECT) {
			build_quad_from_rect(&sprite_rect_basis, &sprite_rect, pos);
		}
		set_position_homography(pos, inst);
		inst->uv_rect[0] = sprite_rect.u;
		inst->uv_rect[1] = sprite_rect.v;
		inst->uv_rect[2] = sprite_rect.w;
		inst->uv_rect[3] = sprite_rect.h;
	}
	sg_append_buffer(m->inst, &(sg_range) { tmp, out_n * sizeof(tmp[0]) });
}

static int
lmaterial_perspective_quad_submit(lua_State *L) {
	struct material_perspective_quad *m = (struct material_perspective_quad *)luaL_checkudata(L, 1, "EXTLUA_MATERIAL_PERSPECTIVE_QUAD");
	int inst_batch_n = (int)(m->tmp_size / sizeof(struct pquad_inst));
	if (inst_batch_n < 1) {
		return luaL_error(L, "Perspective quad tmp buffer is too small");
	}
	soluna_material_submit(L, inst_batch_n * PQUAD_CORNER_N, m, submit);
	return 0;
}

static int
lmaterial_perspective_quad_draw(lua_State *L) {
	struct material_perspective_quad *m = (struct material_perspective_quad *)luaL_checkudata(L, 1, "EXTLUA_MATERIAL_PERSPECTIVE_QUAD");
	int prim_n = luaL_checkinteger(L, 3);
	if (prim_n <= 0) {
		return 0;
	}
	int quad_n = perspective_quad_count(L, prim_n);
	sg_apply_pipeline(m->pip);
	sg_apply_uniforms(UB_vs_params, &(sg_range) { m->uniform, sizeof(vs_params_t) });
	sg_bindings bindings = soluna_material_bindings(L, m->bind);
	bindings.vertex_buffer_offsets[0] += (size_t)m->base * sizeof(struct pquad_inst);
	sg_apply_bindings(&bindings);
	sg_draw(0, 4, quad_n);
	m->base += quad_n;
	return 0;
}

static int
lmaterial_perspective_quad_reset(lua_State *L) {
	struct material_perspective_quad *m = (struct material_perspective_quad *)luaL_checkudata(L, 1, "EXTLUA_MATERIAL_PERSPECTIVE_QUAD");
	m->base = 0;
	return 0;
}

static int
lset_material_id(lua_State *L) {
	int id = luaL_checkinteger(L, 1);
	if (id <= 0) {
		return luaL_error(L, "Invalid perspective quad material id %d", id);
	}
	material_id = id;
	return 0;
}

static void
init_pipeline(struct material_perspective_quad *p) {
	sg_pipeline_desc desc = {
		.layout.attrs = {
			[ATTR_perspective_quad_pos_h0].format = SG_VERTEXFORMAT_FLOAT3,
			[ATTR_perspective_quad_pos_h1].format = SG_VERTEXFORMAT_FLOAT3,
			[ATTR_perspective_quad_pos_h2].format = SG_VERTEXFORMAT_FLOAT3,
			[ATTR_perspective_quad_uv_rect].format = SG_VERTEXFORMAT_FLOAT4,
			[ATTR_perspective_quad_q].format = SG_VERTEXFORMAT_FLOAT4,
			[ATTR_perspective_quad_color].format = SG_VERTEXFORMAT_UBYTE4N,
		},
	};
	p->pip = make_pipeline(&desc);
}

static int
lnew_material_perspective_quad(lua_State *L) {
	luaL_checktype(L, 1, LUA_TTABLE);
	struct material_perspective_quad *m = (struct material_perspective_quad *)lua_newuserdatauv(L, sizeof(*m), 4);
	int material_index = lua_gettop(L);
	init_pipeline(m);
	m->base = 0;

	if (lua_getfield(L, 1, "inst_buffer") != LUA_TUSERDATA) {
		return luaL_error(L, "Invalid key .inst_buffer");
	}
	luaL_checkudata(L, -1, "SOKOL_BUFFER");
	lua_pushvalue(L, -1);
	lua_setiuservalue(L, material_index, 1);
	lua_pushlightuserdata(L, &m->inst);
	lua_call(L, 1, 0);

	if (lua_getfield(L, 1, "bindings") != LUA_TUSERDATA) {
		return luaL_error(L, "Invalid key .bindings");
	}
	m->bind = luaL_checkudata(L, -1, "SOKOL_BINDINGS");
	lua_pushvalue(L, -1);
	lua_setiuservalue(L, material_index, 2);
	lua_pop(L, 1);

	if (lua_getfield(L, 1, "uniform") != LUA_TUSERDATA) {
		return luaL_error(L, "Invalid key .uniform");
	}
	m->uniform = (vs_params_t *)luaL_checkudata(L, -1, "SOKOL_UNIFORM");
	lua_pushvalue(L, -1);
	lua_setiuservalue(L, material_index, 3);
	lua_pop(L, 1);

	if (lua_getfield(L, 1, "sprite_bank") != LUA_TLIGHTUSERDATA) {
		return luaL_error(L, "Invalid key .sprite_bank");
	}
	m->bank = lua_touserdata(L, -1);
	lua_pop(L, 1);

	if (lua_getfield(L, 1, "tmp_buffer") != LUA_TUSERDATA) {
		return luaL_error(L, "Invalid key .tmp_buffer");
	}
	if (lua_getmetatable(L, -1)) {
		return luaL_error(L, "Not an userdata without metatable");
	}
	m->tmp_ptr = lua_touserdata(L, -1);
	m->tmp_size = lua_rawlen(L, -1);
	lua_setiuservalue(L, material_index, 4);

	if (luaL_newmetatable(L, "EXTLUA_MATERIAL_PERSPECTIVE_QUAD")) {
		luaL_Reg l[] = {
			{ "__index", NULL },
			{ "reset", lmaterial_perspective_quad_reset },
			{ "submit", lmaterial_perspective_quad_submit },
			{ "draw", lmaterial_perspective_quad_draw },
			{ NULL, NULL },
		};
		luaL_setfuncs(L, l, 0);
		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");
	}
	lua_setmetatable(L, -2);
	return 1;
}

static inline float
get_number_field(lua_State *L, int index, const char *field, float defv) {
	float v;
	lua_getfield(L, index, field);
	v = luaL_optnumber(L, -1, defv);
	lua_pop(L, 1);
	return v;
}

static int
get_quad(lua_State *L, int index, float quad[8]) {
	if (lua_getfield(L, index, "quad") == LUA_TTABLE) {
		int i;
		for (i=0; i<8; i++) {
			lua_geti(L, -1, i + 1);
			quad[i] = luaL_checknumber(L, -1);
			lua_pop(L, 1);
		}
		lua_pop(L, 1);
		return 0;
	}
	lua_pop(L, 1);
	return 1;
}

static void
get_q(lua_State *L, int index, float q[4]) {
	int i;
	if (lua_getfield(L, index, "q") != LUA_TTABLE) {
		for (i=0; i<4; i++) {
			q[i] = 1.0f;
		}
		lua_pop(L, 1);
		return;
	}
	for (i=0; i<4; i++) {
		lua_geti(L, -1, i + 1);
		q[i] = luaL_optnumber(L, -1, 1.0f);
		lua_pop(L, 1);
	}
	lua_pop(L, 1);
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

struct pquad_stream_context {
	int sprite;
	int use_sprite_rect;
	float scale_x;
	float scale_y;
	float shear_x;
	float shear_y;
	float quad[8];
	float q[4];
	struct color color;
	struct pquad_payload payload[PQUAD_CORNER_N];
};

static void
write_perspective_quad_stream(void *ud, int index, struct soluna_material_stream_item *item) {
	struct pquad_stream_context *ctx = (struct pquad_stream_context *)ud;
	item->sprite = ctx->sprite;
	if (ctx->use_sprite_rect) {
		switch (index) {
		case 0:
			item->x = ctx->scale_x;
			item->y = ctx->scale_y;
			break;
		case 1:
			item->x = ctx->shear_x;
			item->y = ctx->shear_y;
			break;
		default:
			item->x = 0.0f;
			item->y = 0.0f;
			break;
		}
	} else {
		float x = ctx->quad[index * 2];
		float y = ctx->quad[index * 2 + 1];
		item->x = x * ctx->scale_x + y * ctx->shear_x;
		item->y = x * ctx->shear_y + y * ctx->scale_y;
	}
	struct pquad_payload *payload = &ctx->payload[index];
	uint32_t info = (uint32_t)index;
	if (ctx->use_sprite_rect) {
		info |= PQUAD_INFO_USE_SPRITE_RECT;
	}
	payload->info = info;
	payload->q = ctx->q[index];
	payload->color = ctx->color;
	item->payload = payload;
}

static int
lperspective_quad_sprite(lua_State *L) {
	if (material_id <= 0) {
		return luaL_error(L, "Perspective quad material is not registered");
	}
	luaL_checktype(L, 2, LUA_TTABLE);
	struct pquad_stream_context ctx;
	ctx.sprite = luaL_checkinteger(L, 1) - 1;
	ctx.use_sprite_rect = get_quad(L, 2, ctx.quad);
	ctx.scale_x = get_number_field(L, 2, "scale_x", 1.0f);
	ctx.scale_y = get_number_field(L, 2, "scale_y", 1.0f);
	ctx.shear_x = get_number_field(L, 2, "shear_x", 0.0f);
	ctx.shear_y = get_number_field(L, 2, "shear_y", 0.0f);
	get_q(L, 2, ctx.q);
	ctx.color = get_color(L, 2);
	soluna_material_push_stream(L, material_id, PQUAD_CORNER_N, sizeof(struct pquad_payload), write_perspective_quad_stream, &ctx);
	return 1;
}

static int
luaopen_ext_material_perspective_quad(lua_State *L) {
	luaL_checkversion(L);
	luaL_Reg l[] = {
		{ "set_material_id", lset_material_id },
		{ "new", lnew_material_perspective_quad },
		{ "sprite", lperspective_quad_sprite },
		{ "instance_size", NULL },
		{ NULL, NULL },
	};
	luaL_newlib(L, l);
	lua_pushinteger(L, sizeof(struct pquad_inst));
	lua_setfield(L, -2, "instance_size");
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

EXTLUA_EXPORT int
extlua_init(lua_State *L) {
	luaapi_init(L);
	sokolapi_init(L);
	solunaapi_init(L);
	luaL_Reg l[] = {
		{ "ext.foobar", luaopen_foobar },
		{ "ext.material.perspective_quad", luaopen_ext_material_perspective_quad },
		{ NULL, NULL },
	};
	luaL_newlib(L, l);
	return 1;
}
