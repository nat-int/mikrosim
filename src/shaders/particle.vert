#version 450
layout(location=0) in vec2 vpos;
layout(location=1) in vec2 pos;
layout(location=2) in uint debug_tags;
layout(location=0) out vec2 fvpos;
layout(location=1) out vec4 fvcol;

layout(push_constant) uniform PT {
	mat4 proj;
	float psize;
} p;

vec3 hsv_to_rgb(vec3 hsv) {
	vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
	vec3 p = abs(fract(hsv.xxx + K.xyz) * 6.0 - K.www);
	return hsv.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), hsv.y);
}

void main() {
	vec4 real_pos = vec4(pos + vpos * p.psize, 0, 1);
	gl_Position = p.proj * real_pos;
	fvpos = vpos;
	fvcol = vec4(.1, .1, 1., 1.);
	/*if (debug_tags < 14) {
		fvcol = vec4(hsv_to_rgb(vec3(debug_tags * 0.07, 1.0, 1.0)), 1.0);
	} else {
		fvcol = vec4(hsv_to_rgb(vec3(debug_tags * 0.07, 1.0, 0.2)), 1.0);
	}*/
}
