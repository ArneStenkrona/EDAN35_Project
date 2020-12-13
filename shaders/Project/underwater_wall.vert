#version 410

uniform sampler2D heightmap_texture;
uniform mat4 vertex_model_to_world;
uniform mat4 normal_model_to_world;
uniform mat4 vertex_world_to_clip;

uniform bool is_water;

uniform vec3 camera_position;

layout (location = 0) in vec3 vertex;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec3 texcoord;
layout (location = 3) in vec3 tangent;
layout (location = 4) in vec3 binormal;

out VS_OUT {
    vec3 worldPos;
} vs_out;


void main() {
    vec4 modelPos;
    vec3 worldNormal = normalize(vec3(normal_model_to_world * -vec4(normal,0)));
    float dx = abs(worldNormal.x); float dz = abs(worldNormal.z);
    
    vec2 uv = vec2(0,0); // uses normal to determine which edge to sample
    float alpha = texcoord.x;
    if (dz > dx) {
        if (worldNormal.z > 0)
            uv = vec2(1 - alpha, 1);
        else if (worldNormal.z < 0)
            uv = vec2(alpha, 0);
    } else if (dx > dz) {
        if (worldNormal.x > 0)
            uv = vec2(0, 1 - alpha);
        else if (worldNormal.x < 0)
            uv = vec2(1, alpha);
    }

    vec4 info = texture(heightmap_texture, uv);

    float height = texcoord.y == 1 ? info.r : 0;
    modelPos = vec4(vertex, 1.0); // offset before SRT

    vec4 worldPos = vertex_model_to_world * modelPos;
    worldPos = worldPos / worldPos.w;

    worldPos += vec4(0,height,0,0); // offset after SRT

    vs_out.worldPos = worldPos.xyz;
    
    vec4 outpos = vertex_world_to_clip * worldPos;
    gl_Position = outpos;
	
}
