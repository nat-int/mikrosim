#version 450

layout(location=0) in vec2 vp;
layout(location=1) in vec4 col;

layout(location=0) out vec4 oc;

layout(set=0, binding=0) uniform sampler2D tex;

void main() {
	oc = col * texture(tex, vp);
}
