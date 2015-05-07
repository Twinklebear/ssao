#version 430 core

#define BLUR_HORIZ 0
#define BLUR_VERT 1
#define RADIUS 4

#include "global.glsl"

// Gaussian filter values from the author's blurring shader
const float gaussian[RADIUS + 1] = float[](0.153170, 0.144893, 0.122649, 0.092902, 0.062970);

uniform sampler2D ao_in;
// True if we're blurring vertically, false if horizontally since we
// do the blurring in two passes, once for horizontal and once for vertical
uniform ivec2 axis;

out vec2 result;

void main(void){
	ivec2 px = ivec2(gl_FragCoord.xy);
	vec3 val = texelFetch(ao_in, px, 0).xyz;
	float z_pos = val.y;

	// Compute weighting for the term at the center of the kernel
	float base = gaussian[0];
	float weight = base;
	float sum = weight * val.x;

	for (int i = -RADIUS; i <= RADIUS; ++i){
		// We handle the center pixel above so skip that case
		if (i != 0){
			// Filter scale effects how many pixels the kernel actually covers
			ivec2 p = px + axis * i * ao_params.filter_scale;
			vec3 val = texelFetch(ao_in, p, 0).xyz;
			float z = val.y;
			float w = 0.3 + gaussian[abs(i)];
			// Decrease weight as depth difference increases. This prevents us from
			// blurring across depth discontinuities
			w *= max(0.f, 1.f - (ao_params.edge_sharpness * 400.f) * abs(z_pos - z));
			sum += val.x * w;
			weight += w;
		}
	}
	result = vec2(sum / (weight + 0.0001), val.y);
}

