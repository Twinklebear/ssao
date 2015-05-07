#version 430 core

#define PI 3.14159265358979323846f
#define TAU 6.28318530717958647692f

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

in VertexData {
	vec3 obj_pos;
	vec3 world_pos;
	vec3 cam_space_pos;
	vec3 normal;
	vec2 texcoord;
	flat uint mat_id;
} vert_data[];

out VertexData {
	vec3 world_pos;
	vec3 cam_space_pos;
	vec3 normal;
	vec2 texcoord;
	vec3 tangent;
	vec3 bitangent;
	flat uint mat_id;
} frag_data;

void main(void){
	vec3 tangents[3];
	vec3 bitangents[3];
	// Compute tangents and bitangent's using method from Mark Kilgard's slides
	vec3 dp_du = vert_data[1].obj_pos - vert_data[0].obj_pos;
	vec3 dp_dv = vert_data[2].obj_pos - vert_data[0].obj_pos;
	float ds_du = vert_data[1].texcoord.s - vert_data[0].texcoord.s;
	float ds_dv = vert_data[2].texcoord.s - vert_data[0].texcoord.s;
	vec3 t = normalize(ds_dv * dp_du - ds_du * dp_dv);
	vec2 dst_du = vert_data[1].texcoord - vert_data[0].texcoord;
	vec2 dst_dv = vert_data[2].texcoord - vert_data[0].texcoord;
	float area = determinant(mat2(dst_du, dst_dv));
	t = area >= 0 ? t : -t;
	for (int i = 0; i < 3; ++i){
		bitangents[i] = cross(t, vert_data[i].normal);
		tangents[i] = normalize(cross(vert_data[i].normal, bitangents[i]));
	}
	for (int i = 0; i < 3; ++i){
		frag_data.world_pos = vert_data[i].world_pos;
		frag_data.normal = vert_data[i].normal;
		frag_data.texcoord = vert_data[i].texcoord;
		frag_data.mat_id = vert_data[i].mat_id;
		frag_data.cam_space_pos = vert_data[i].cam_space_pos;
		frag_data.tangent = tangents[i];
		frag_data.bitangent = -bitangents[i];
		gl_Position = gl_in[i].gl_Position;
		EmitVertex();
	}
}

