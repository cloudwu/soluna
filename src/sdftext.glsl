@vs vs
layout(binding=0) uniform vs_params {
	vec2 framesize;
	vec2 texsize;
};

struct sr_mat {
	mat2 m;
};

layout(binding=0) readonly buffer sr_lut {
	sr_mat sr[];
};

in vec3 position;
in uint offset;
in uint u;
in uint v;

out vec2 uv;

void main() {
	ivec2 uv_base = ivec2(u >> 16, v >> 16);
	ivec2 u2 = ivec2(0 , u & 0xffff);
	ivec2 v2 = ivec2(0 , v & 0xffff);
	ivec2 off = ivec2(offset >> 16 , offset & 0xffff) - 0x8000;
	vec2 uv_offset = vec2(u2[gl_VertexIndex & 1] , v2[gl_VertexIndex >> 1]);
	vec2 pos = ((uv_offset - off) * sr[int(position.z)].m + position.xy) * framesize;
	gl_Position = vec4(pos - sign(framesize), 0, 1);
	uv = (uv_base + uv_offset) * texsize;
}

@end

@fs fs
layout(binding=1) uniform texture2D tex;
layout(binding=0) uniform sampler smp;

layout(binding=1) uniform fs_params {
	float edge_mask;
	float dist_multiplier;
	int color;
	float unused;
};

in vec2 uv;
out vec4 frag_color;

void main() {
	float dis = texture(sampler2D(tex,smp), uv).r;
	float smoothing = length(fwidth(uv)) * 128.0 * dist_multiplier;
	float alpha = smoothstep(edge_mask - smoothing, edge_mask + smoothing, dis);
	float color_alpha = ((color >> 24) & 0xff) / 255.0f;
	vec4 c = vec4(
		((color >> 16) & 0xff) / 255.0f,
		((color >> 8) & 0xff) / 255.0f,
		((color) & 0xff) / 255.0f,
		color_alpha * alpha);
	frag_color = c;
}
@end

@program texquad vs fs