#version 430 core

#include "global.glsl"

uniform sampler2D ao_texture;
uniform bool ao_only;

layout(location = 0) out vec4 color;
layout(location = 1) out vec3 cam_normal;

in VertexData {
	vec3 world_pos;
	vec3 cam_space_pos;
	vec3 normal;
	vec2 texcoord;
	vec3 tangent;
	vec3 bitangent;
	flat uint mat_id;
} frag_data;

// Transform a vector from shading space to object space
vec3 from_shading(vec3 v){
	return vec3(frag_data.bitangent.x * v.x + frag_data.tangent.x * v.y + frag_data.normal.x * v.z,
			frag_data.bitangent.y * v.x + frag_data.tangent.y * v.y + frag_data.normal.y * v.z,
			frag_data.bitangent.z * v.x + frag_data.tangent.z * v.y + frag_data.normal.z * v.z);
}

void main(void){
	if (ao_only && !depth_pass){
		color = vec4(texture(ao_texture, gl_FragCoord.xy / viewport_dim).rrr, 1);
		return;
	}
	vec3 view_dir = normalize(cam_pos - frag_data.world_pos);
	vec3 normal = frag_data.normal;
	vec4 ka = mats[frag_data.mat_id].kd;
	vec4 kd = mats[frag_data.mat_id].kd;
	vec4 ks = vec4(0);
	if (mats[frag_data.mat_id].map_mask.xy != ivec2(-1, -1)){
		ka.a = texture(model_textures[mats[frag_data.mat_id].map_mask.x],
				vec3(frag_data.texcoord, mats[frag_data.mat_id].map_mask.y)).r;
		if (ka.a == 0){
			discard;
		}
	}
	if (mats[frag_data.mat_id].map_ks_n.zw != ivec2(-1, -1)){
		normal = texture(model_textures[mats[frag_data.mat_id].map_ks_n.z],
				vec3(frag_data.texcoord, mats[frag_data.mat_id].map_ks_n.w)).xyz;
		normal = 2 * normal - vec3(1);
		normal = normalize(from_shading(normal));
	}

	if (depth_pass){
		cam_normal = (inv_trans_view * vec4(normal, 0)).xyz;
		color = vec4(frag_data.cam_space_pos, 1);
		return;
	}

	if (mats[frag_data.mat_id].map_ka_kd.xy != ivec2(-1, -1)){
		ka = texture(model_textures[mats[frag_data.mat_id].map_ka_kd.x],
				vec3(frag_data.texcoord, mats[frag_data.mat_id].map_ka_kd.y));
	}
	if (mats[frag_data.mat_id].map_ka_kd.zw != ivec2(-1, -1)){
		kd = texture(model_textures[mats[frag_data.mat_id].map_ka_kd.z],
				vec3(frag_data.texcoord, mats[frag_data.mat_id].map_ka_kd.w));
	}
	if (mats[frag_data.mat_id].map_ks_n.xy != ivec2(-1, -1)){
		ks.a = texture(model_textures[mats[frag_data.mat_id].map_ks_n.x],
				vec3(frag_data.texcoord, mats[frag_data.mat_id].map_ks_n.y)).r;
	}

	float ao = texture(ao_texture, gl_FragCoord.xy / viewport_dim).r;
	vec3 linear_col = ambient_light * ka.rgb * ao;
	vec3 half_vec = normalize(light_dir.xyz + view_dir);
	float light_power = light_dir.a;
	if (dot(light_dir.xyz, normal) > 0){
		linear_col += light_power * kd.rgb * dot(light_dir.xyz, normal);
		// If the color has a specular exponent add a white specular component
		if (ks.a > 0){
			linear_col += light_power * ks.rgb * pow(dot(half_vec, normal), ks.a);
		}
	}
	color = vec4(pow(linear_col, vec3(1.0 / 2.2)), ka.a);
}

