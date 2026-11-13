#ifndef soluna_material_util_h
#define soluna_material_util_h

#include <lua.h>
#include "sokol/sokol_gfx.h"
#include "batch.h"
#include "transient.h"

void util_ref_object(lua_State *L, void *ptr, int uv_index, const char *key, const char *luatype, int direct);

typedef void (*util_submit_func)(lua_State *L, void *m_, struct draw_primitive *prim, int n);
void util_submit_material(lua_State *L, int batch_n, void *mat, util_submit_func submit);

typedef const sg_shader_desc* (*util_shader_desc_func)(sg_backend backend);
sg_pipeline util_make_pipeline(sg_pipeline_desc *desc, util_shader_desc_func func, const char *what, int blend);
void util_transient_buffer_append(lua_State *L, struct transient_buffer *tb, sg_buffer buffer, const void *ptr, size_t sz, size_t from);

#endif
