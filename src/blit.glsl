@vs vs
layout(location=0) in vec2 position;

out vec2 uv;

void main() {
	gl_Position = vec4(position * 2.0 - 1.0, 0.0, 1.0);
	uv = position;
}
@end

@fs fs
layout(binding=0) uniform texture2D tex;
layout(binding=0) uniform sampler smp;

in vec2 uv;
out vec4 frag_color;

void main() {
	frag_color = texture(sampler2D(tex, smp), uv);
}
@end

@program blit vs fs
