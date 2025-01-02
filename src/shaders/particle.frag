#version 450
layout(location=0) in vec2 vp;
layout(location=1) in vec4 vc;

layout(location=0) out vec4 oc;

void main() {
	oc = vc;
	oc.a *= smoothstep(0, 0.01, 1-dot(vp, vp));
}
