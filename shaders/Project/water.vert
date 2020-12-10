#version 410

uniform mat4 vertex_model_to_world;
uniform mat4 vertex_world_to_clip;

layout (location = 0) in vec3 vertex;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec3 texcoord;
layout (location = 3) in vec3 tangent;
layout (location = 4) in vec3 binormal;

uniform vec2 inv_res;
uniform vec3 camera_position;

uniform sampler2D heightmap_texture;

const float refractionFactor = 0.25;

const float fresnelBias = 0.1;
const float fresnelPower = 2.;
const float fresnelScale = 0.1;

const float eta = 1 / 1.33;

out VS_OUT {
    vec2 refractedPosition[3];
    float reflectionFactor;
    vec3 normal;
    vec3 worldPos;
} vs_out;

void main() 
{
    vec4 modelPos;
    vec3 waveNormal;

    vec4 normal_and_height = texture(heightmap_texture, texcoord.xy);

    modelPos = vec4(vertex + vec3(0,1,0) * normal_and_height.w, 1.0);
    waveNormal = normalize(normal_and_height.xyz);


    vec4 worldPos = vertex_model_to_world * modelPos;
    worldPos = worldPos / worldPos.w;

    vs_out.normal = waveNormal;
    vs_out.worldPos = worldPos.xyz;

    vec3 eye = normalize(worldPos.xyz - camera_position.xyz);
    vec3 reflected = normalize(reflect(eye, normal));

    vs_out.reflectionFactor = fresnelBias + fresnelScale * pow(1. + dot(eye, waveNormal), fresnelPower);

    mat4 proj = vertex_world_to_clip;// * vertex_model_to_world;

    vec4 projectedRefractedPosition;
    projectedRefractedPosition = proj * vec4(worldPos.xyz + refractionFactor * normalize(refract(eye, waveNormal, eta * 1.00)), 1.0);
    vs_out.refractedPosition[0] = projectedRefractedPosition.xy / projectedRefractedPosition.w;

    projectedRefractedPosition = proj * vec4(worldPos.xyz + refractionFactor * normalize(refract(eye, waveNormal, eta * 0.96)), 1.0);
    vs_out.refractedPosition[1] = projectedRefractedPosition.xy / projectedRefractedPosition.w;

    projectedRefractedPosition = proj * vec4(worldPos.xyz + refractionFactor * normalize(refract(eye, waveNormal, eta * 0.92)), 1.0);
    vs_out.refractedPosition[2] = projectedRefractedPosition.xy / projectedRefractedPosition.w;

    gl_Position = vertex_world_to_clip * worldPos;
    
}
