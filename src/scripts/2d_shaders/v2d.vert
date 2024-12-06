#version 450

layout(location=0) in vec2 pos;
layout(location=1) in vec2 tp;
layout(location=2) in vec4 col;
layout(location=0) out vec2 ftp;
layout(location=1) out vec4 fcol;

layout(push_constant) uniform PT {
	mat4 proj;
} p;

void main() {
	gl_Position = p.proj * vec4(pos, 0, 1);
	ftp = tp;
	fcol = col;
}
