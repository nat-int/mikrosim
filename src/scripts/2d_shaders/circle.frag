#version 450

layout(location=0) in vec2 vp;
layout(location=1) in vec4 col;

layout(location=0) out vec4 oc;

void main() {
	oc = col;
	vec2 p = vp * vec2(2) - vec2(1);
	oc.a *= smoothstep(0, 0.01, 1-dot(p, p));
}
