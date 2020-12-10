#version 410

uniform mat4 vertex_model_to_world;
uniform mat4 vertex_world_to_clip;

uniform bool is_water;

uniform vec3 camera_position;

layout (location = 0) in vec3 vertex;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec3 texcoord;
layout (location = 3) in vec3 tangent;
layout (location = 4) in vec3 binormal;

out VS_OUT {
	vec3 normal;
	vec2 texcoord;
	vec3 tangent;
	vec3 binormal;
    vec4 worldPos;
    float distCamSquared;
} vs_out;

void main() {
    vec4 worldPos;
    vs_out.normal   = normalize(normal);
    vs_out.tangent  = normalize(tangent);
    vs_out.binormal = normalize(binormal);
    worldPos = vertex_model_to_world * vec4(vertex, 1.0);
    worldPos = worldPos / worldPos.w;

    vs_out.distCamSquared = dot(worldPos.xyz, worldPos.xyz);
    
    // common
    vs_out.texcoord = texcoord.xy;
    vs_out.worldPos = worldPos;
    gl_Position = vertex_world_to_clip * worldPos;
	
}
