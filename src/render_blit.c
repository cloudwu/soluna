#include <stdbool.h>
#include "render_blit.h"
#include "blit.glsl.h"

struct soluna_blit_state {
	bool initialized;
	sg_pipeline pip;
	sg_buffer vbuf;
};

static struct soluna_blit_state BLIT_STATE;

static void
ensure_blit_state(void) {
	if (BLIT_STATE.initialized)
		return;
	static const float quad[] = {
		0.0f, 0.0f,
		1.0f, 0.0f,
		0.0f, 1.0f,
		1.0f, 1.0f,
	};
	BLIT_STATE.vbuf = sg_make_buffer(&(sg_buffer_desc){
		.data = SG_RANGE(quad),
		.label = "soluna-blit-vbuf",
	});
	sg_shader shd = sg_make_shader(blit_shader_desc(sg_query_backend()));
	BLIT_STATE.pip = sg_make_pipeline(&(sg_pipeline_desc){
		.layout = {
			.attrs[0].format = SG_VERTEXFORMAT_FLOAT2,
		},
		.shader = shd,
		.primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
		.label = "soluna-blit-pipeline",
	});
	BLIT_STATE.initialized = true;
}

void
soluna_render_blit(sg_view texture_view, sg_sampler sampler) {
	ensure_blit_state();
	sg_bindings bind = {
		.vertex_buffers[0] = BLIT_STATE.vbuf,
		.views[0] = texture_view,
		.samplers[0] = sampler,
	};
	sg_apply_pipeline(BLIT_STATE.pip);
	sg_apply_bindings(&bind);
	sg_draw(0, 4, 1);
}
