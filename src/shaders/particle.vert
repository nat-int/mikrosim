#version 450
layout(location=0) in vec2 vpos;
layout(location=1) in vec2 pos;
layout(location=2) in uint type;
layout(location=3) in float conc;
layout(location=0) out vec2 fvpos;
layout(location=1) out float fvedge;
layout(location=2) out vec4 fvcol;

layout(push_constant) uniform PT {
	mat4 proj;
	float psize;
} p;

/*vec3 hsv_to_rgb(vec3 hsv) {
	vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
	vec3 p = abs(fract(hsv.xxx + K.xyz) * 6.0 - K.www);
	return hsv.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), hsv.y);
}*/

void main() {
	const float scales[] = float[6](1., 1.2, .9, .6, .6, .6);
	vec4 real_pos = vec4(pos + vpos * p.psize * scales[type], 0, 1);
	gl_Position = p.proj * real_pos;
	fvpos = vpos;
	fvcol = vec4(.1, .1, 1., 1.);
	const float edges[] = float[6](-1, 1, .01, .01, .01, .01);
	const vec4 colors[] = vec4[6](
		vec4(0, 0, 0, .5),
		vec4(.1, 0, 1, 1),
		vec4(.5, .1, .8, 1),
		vec4(1., .8, .1, 1),
		vec4(.5, .1, .5, 1),
		vec4(.6, .2, .1, 1)
	);
	const vec4 conc_factor[] = vec4[6](
		vec4(1, 0, 0, 0),
		vec4(0, 1, 0, 0),
		vec4(0, .5, 0, 0),
		vec4(0, 0, 0, 0),
		vec4(0, .5, 0, 0),
		vec4(0, .5, 0, 0)
	);
	fvedge = edges[type];
	fvcol = colors[type] + conc_factor[type] * conc;
}
