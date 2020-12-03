#version 410

uniform mat4 vertex_model_to_world;
uniform mat4 vertex_world_to_clip;

layout (location = 0) in vec3 vertex;

out VS_OUT {
	vec3 fragPos;
} vs_out;

void main()
{
	vec4 pos = vertex_world_to_clip * vertex_model_to_world * vec4(vertex, 1.0);;
	vs_out.fragPos = pos.xyz;
	gl_Position = pos;
}
