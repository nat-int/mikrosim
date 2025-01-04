#version 450
layout(location=0) in vec2 vp;
layout(location=1) in float ve;
layout(location=2) in vec4 vc;

layout(location=0) out vec4 oc;

void main() {
	if (ve < 0)
		discard;
	oc = vc;
	oc.a *= smoothstep(0, ve, 1-length(vp));
}
