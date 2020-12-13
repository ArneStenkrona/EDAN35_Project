#version 410

uniform mat4 vertex_model_to_world;
uniform mat4 vertex_world_to_clip;

uniform sampler2D heightmap_texture;

layout (location = 0) in vec3 vertex;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec3 texcoord;
layout (location = 3) in vec3 tangent;
layout (location = 4) in vec3 binormal;

void main()
{
    vec4 info = texture(heightmap_texture, texcoord.xy);
    vec4 modelPos = vec4(vertex + vec3(0,1,0) * info.r/*info.w*/, 1.0);

    vec4 worldPos = vertex_model_to_world * modelPos;
    worldPos = worldPos / worldPos.w;

    gl_Position = vertex_world_to_clip * worldPos;
}
