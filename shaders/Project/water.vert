#version 410

uniform mat4 vertex_model_to_world;
uniform mat4 vertex_world_to_clip;

layout (location = 0) in vec3 vertex;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec3 texcoord;
layout (location = 3) in vec3 tangent;
layout (location = 4) in vec3 binormal;

uniform vec2 inv_res;
uniform float t;
uniform vec3 camera_position;

uniform sampler2D heightmap_texture;

const float refractionFactor = 1.;

const float fresnelBias = 0.1;
const float fresnelPower = 2.;
const float fresnelScale = 0.25;

const float eta = 1 / 1.33;

uniform bool IN_WATER;

out VS_OUT {
    vec3 refractedDir[3];
    float reflectionFactor;
    float renderFromBelow;
    vec3 reflected;
    vec3 extra;
    float waveHeight;
    vec3 projectedReflected;
} vs_out;


void main() 
{
    vec4 modelPos;
    vec3 waveNormal;

    vec4 info = texture(heightmap_texture, texcoord.xy);

    modelPos = vec4(vertex + vec3(0,1,0) * info.r/*info.w*/, 1.0);
    waveNormal = normalize(vec3(info.b, sqrt(1.0 - dot(info.ba, info.ba)), info.a)).xyz;//normalize(normal_and_height.xyz);
    vs_out.waveHeight = info.r;

    vec4 worldPos = vertex_model_to_world * modelPos;
    worldPos = worldPos / worldPos.w;

    vec3 eye = normalize(worldPos.xyz - camera_position.xyz);
    vs_out.renderFromBelow = -eye.y;

    mat4 proj = vertex_world_to_clip;
    vec4 projUnderwaterTextPos;

    float etaA = eta, etaB = eta * 0.98, etaC = eta * 0.92;

    if (vs_out.renderFromBelow <= 0) {
        waveNormal = -waveNormal;
        etaB = eta * 0.99; etaC = eta * 0.98;
        etaA = 1 / etaA; etaB = 1 / etaB; etaC = 1 / etaC; 
    }

    vs_out.reflectionFactor = fresnelBias + fresnelScale * pow(1. + dot(eye, waveNormal), fresnelPower);

    vec3 reflected = normalize(reflect(eye, waveNormal));

    vec3 refactA = refract(eye, waveNormal, etaA);
    vec3 refactB = refract(eye, waveNormal, etaB);
    vec3 refactC = refract(eye, waveNormal, etaC);

    vs_out.extra = reflected;

    if (vs_out.renderFromBelow <= 0) {
        vs_out.reflected = reflected;

        vs_out.refractedDir[0] = refactA;
        vs_out.refractedDir[1] = refactB;
        vs_out.refractedDir[2] = refactC;
    } else {
        vs_out.reflected = reflected;

        projUnderwaterTextPos = proj * vec4(worldPos.xyz + refractionFactor * refactA, 1.0);
        vs_out.refractedDir[0] = projUnderwaterTextPos.xyz / projUnderwaterTextPos.w;

        projUnderwaterTextPos = proj * vec4(worldPos.xyz + refractionFactor * refactB, 1.0);
        vs_out.refractedDir[1] = projUnderwaterTextPos.xyz / projUnderwaterTextPos.w;

        projUnderwaterTextPos = proj * vec4(worldPos.xyz + refractionFactor * refactC, 1.0);
        vs_out.refractedDir[2] = projUnderwaterTextPos.xyz / projUnderwaterTextPos.w;
    }

    vs_out.projectedReflected = (vertex_world_to_clip * vec4(vs_out.reflected, 0.)).xyz;

    vec4 outpos = vertex_world_to_clip * worldPos;
    gl_Position = outpos;
    
}
