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
in vec4 color;
in uint offset;
in uint u;
in uint v;

out vec2 uv;
out vec4 maskcolor;

void main() {
	ivec2 uv_base = ivec2(u >> 16, v >> 16);
	ivec2 u2 = ivec2(0 , u & 0xffff);
	ivec2 v2 = ivec2(0 , v & 0xffff);
	ivec2 off = ivec2(offset >> 16 , offset & 0xffff) - 0x8000;
	vec2 uv_offset = vec2(u2[gl_VertexIndex & 1] , v2[gl_VertexIndex >> 1]);
	vec2 pos = ((uv_offset - off) * sr[int(position.z)].m + position.xy) * framesize;
	gl_Position = vec4(pos - sign(framesize), 0, 1);
	uv = (uv_base + uv_offset) * texsize;
	maskcolor = color;
}

@end

@fs fs
layout(binding=1) uniform texture2D tex;
layout(binding=0) uniform sampler smp;

in vec2 uv;
in vec4 maskcolor;
out vec4 frag_color;

void main() {
	float alpha = texture(sampler2D(tex,smp), uv).a; 
	frag_color = maskcolor;
	frag_color.a = alpha * maskcolor.a;
}
@end

@program maskquad vs fs