// Just having some hardcoded light properties for now
const vec3 ambient_light = vec3(0.2);
// If we're doing a depth only pass
uniform bool depth_pass;

layout(std140, binding = 0) uniform Viewing {
	mat4 proj, view, inv_trans_view;
	vec3 cam_pos;
	// Note: the alpha channel of the light is used as the light power
	vec4 light_dir;
	vec2 viewport_dim;
};

layout(std140, binding = 2) uniform ShadowView {
	mat4 cube_view[6];
	mat4 cube_proj;
};

#define NUM_MODEL_TEXTURES 16
uniform sampler2DArray model_textures[NUM_MODEL_TEXTURES];

// Material information about an object in the Sponza model
struct Material {
	vec4 ka;
	vec4 kd;
	vec4 ks;
	ivec4 map_ka_kd;
	ivec4 map_ks_n;
	ivec4 map_mask;
};

layout(std140, binding = 4) readonly buffer Materials {
	Material mats[];
};

layout(std140, binding = 5) uniform AOTweakParams {
	// Parameters for the AO pass
	int use_rendered_normals;
	int n_samples;
	int turns;
	float ball_radius;
	float sigma;
	float kappa;
	float beta;
	// Parameters for the blurring pass
	int filter_scale;
	float edge_sharpness;
} ao_params;

