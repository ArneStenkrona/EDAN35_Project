#version 410

uniform mat4 vertex_model_to_world;
uniform mat4 vertex_world_to_clip;

layout (location = 0) in vec3 vertex;

out VS_OUT {
    vec4 fragPos;
    vec2 texcoord;
//    vec2 texcoord_env;
} vs_out;

void main()
{
//    float x = -1.0 + float((gl_VertexID & 1) << 2);
//    float y = -1.0 + float((gl_VertexID & 2) << 1);
//	vs_out.texcoord_env = vec2((x + 1.0) * 0.5, (y + 1.0) * 0.5);

    vec4 worldPos = vertex_model_to_world * vec4(vertex, 1.0);
    vec4 pos = vertex_world_to_clip * worldPos;
    pos = pos / pos.w;
    vs_out.fragPos = vec4(worldPos.xyz, pos.z);
    gl_Position = pos;
}
