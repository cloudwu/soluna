@vs vs
layout(binding=0) uniform vs_params {
	vec2 framesize;
};

struct sr_mat {
	mat2 m;
};

layout(binding=0) readonly buffer sr_lut {
	sr_mat sr[];
};

in vec4 position;
in uint idx;
in vec4 c;

out vec4 color;

void main() {
	ivec2 u2 = ivec2(0 , position.z);
	ivec2 v2 = ivec2(0 , position.w);
	vec2 uv_offset = vec2(u2[gl_VertexIndex & 1] , v2[gl_VertexIndex >> 1]);
	vec2 pos = (uv_offset * sr[idx].m + position.xy) * framesize;
	gl_Position = vec4(pos - sign(framesize), 0, 1);
	color = c;
}

@end

@fs fs

in vec4 color;
out vec4 frag_color;

void main() {
	frag_color = color;
}
@end

@program colorquad vs fs