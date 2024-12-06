#version 450

layout(location=0) in vec2 vp;
layout(location=1) in vec4 col;

layout(location=0) out vec4 oc;

layout(set=0, binding=0) uniform sampler2D tex;

layout(push_constant) uniform PT {
	layout(offset=64) float spxr;
} push;

float median(float r, float g, float b) {
	return max(min(r, g), min(max(r, g), b));
}
void main() {
	vec4 msd = texture(tex, vp); // alpha is true dist (for glow/shadow)
	float psd = median(msd.r, msd.g, msd.b);
	float opc = clamp(0.5 + push.spxr * (psd - 0.5), 0, 1); // spxr is precomputed for 2d
	oc = col * vec4(1, 1, 1, opc);
}
