#version 410

uniform mat4 vertex_model_to_world;
uniform mat4 vertex_world_to_clip;

layout (location = 0) in vec3 vertex;

out VS_OUT {
    vec4 fragPos;
    vec2 texcoord;
} vs_out;

void main()
{
    vec4 worldPos = vertex_model_to_world * vec4(vertex, 1.0);
    vec4 pos = vertex_world_to_clip * worldPos;
    pos = pos / pos.w;
    vs_out.fragPos = vec4(worldPos.xyz, pos.z);
    gl_Position = pos;
}
