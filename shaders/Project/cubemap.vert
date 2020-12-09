#version 410

layout (location = 0) in vec3 vertex; // vertex in model space

uniform mat4 vertex_model_to_world; // transform between model space and world space for positions
uniform mat4 vertex_world_to_clip;  // transform between world space to clip space for positions

uniform vec3 camera_position;	// Defined in world space

out VS_OUT{
	vec3 world_view;	// view vector in world space
} vs_out;

void main()
{
	vec3 world_vertex = (vertex_model_to_world * vec4(vertex, 1.0)).xyz; // vertex position in world space
	vs_out.world_view = camera_position - world_vertex; // create view vector in world space
	gl_Position = vertex_world_to_clip * vec4(world_vertex, 1.0f); // final transform from world pos to clip space
}
