@vs vs
layout(binding=0) uniform vs_params {
	vec2 framesize;
	float texsize;
};

in vec3 pos_h0;
in vec3 pos_h1;
in vec3 pos_h2;
in vec4 uv_rect;
in vec4 q;

in vec4 color;

out vec3 uvq;
out vec4 frag_color;
out flat float tex_scale;

void main() {
	vec2 corner = vec2(float(gl_VertexIndex & 1), float(gl_VertexIndex >> 1));
	mat3 pos_h = mat3(pos_h0, pos_h1, pos_h2);
	vec3 pos_hv = pos_h * vec3(corner, 1.0);
	float pos_w = max(pos_hv.z, 1e-6);
	vec2 pos = pos_hv.xy / pos_w;
	vec2 uv = uv_rect.xy + uv_rect.zw * corner;
	float qx0 = mix(q.x, q.y, corner.x);
	float qx1 = mix(q.z, q.w, corner.x);
	float qv = max(mix(qx0, qx1, corner.y), 1e-6);
	uvq = vec3(uv * qv, qv);
	vec2 clip = pos * framesize;
	gl_Position = vec4(clip.x - 1.0, clip.y + 1.0, 0.0, 1.0);
	frag_color = color;
	tex_scale = texsize;
}

@end

@fs fs
layout(binding=1) uniform texture2D tex;
layout(binding=0) uniform sampler smp;

in vec3 uvq;
in vec4 frag_color;
in flat float tex_scale;
out vec4 out_color;

void main() {
	vec3 proj_uv = vec3(uvq.xy * tex_scale, uvq.z);
	out_color = textureProj(sampler2D(tex, smp), proj_uv) * frag_color;
}
@end

@program perspective_quad vs fs
