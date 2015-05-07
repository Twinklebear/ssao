#version 430 core

#include "global.glsl"

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texcoord;
layout(location = 3) in uint mat_id;

out VertexData {
	vec3 obj_pos;
	vec3 world_pos;
	vec3 cam_space_pos;
	vec3 normal;
	vec2 texcoord;
	flat uint mat_id;
} vert_data;

void main(void){
	vert_data.obj_pos = pos;
	vert_data.world_pos = pos;
	vert_data.normal = normalize(normal);
	vert_data.texcoord = texcoord;
	vert_data.mat_id = mat_id;
	vec4 p = view * vec4(pos * 0.25, 1);
	vert_data.cam_space_pos = p.xyz;
	gl_Position = proj * p;
}

