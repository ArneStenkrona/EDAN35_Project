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

const float refractionFactor = 0.5;

const float fresnelBias = 0.1;
const float fresnelPower = 2.;
const float fresnelScale = 0.1;

const float eta = 0.7504;

out VS_OUT {
    vec3 normal;
    vec2 texcoord;
    vec3 tangent;
    vec3 binormal;
    vec4 worldPos;
    vec2 refractedPosition[3];
    vec3 reflected;
    float reflectionFactor;
} vs_out;

struct Wave {
    float Amplitude;
    float Frequency;
    float Phase;
    float Sharpness;
    vec2 Direction;

    vec2 padding;
};

uniform Wave wave1;
uniform Wave wave2;
uniform float time;

struct WaveCalculation {
    vec4 vertex;
    vec4 normal;
    vec4 tangent;
    vec4 binormal;
};

WaveCalculation getVertexWaveData(vec2 xy, Wave[2] waves, float t) {

    float height = 0;
    float dHdx = 0;
    float dHdz = 0;
    Wave w;
    for(int i = 0; i < 2; i++) {
        w = waves[i];
        float dirFact = w.Direction.x * xy.x + w.Direction.y * xy.y;
        float sinFact = sin(dirFact * w.Frequency + t * w.Phase) * 0.5 + 0.5;
        float cosFact = 0.5 * w.Sharpness * w.Frequency * w.Amplitude * cos(dirFact * w.Frequency + t * w.Phase);
        height += w.Amplitude * pow(sinFact, w.Sharpness);
        float cosSinFact = pow(sinFact, w.Sharpness - 1) * cosFact;
        dHdx += cosSinFact * w.Direction.x;
        dHdz += cosSinFact * w.Direction.y;
    }

    // Fill output in struct
    WaveCalculation result;
    result.vertex = vec4(vertex + normal * height, 1.0);
    // Convert from tangent space to model space
    result.normal = vec4(vec3(-dHdx, 1, -dHdz), 0.0);
    result.tangent = vec4(vec3(1, dHdx, 0), 0.0);
    result.binormal = vec4(vec3(0, dHdz, 1), 0.0);
    return result;
}

void main() {
    vec4 worldPos;

    Wave[2] waves; waves[0] = wave1; waves[1] = wave2;
    WaveCalculation vertexData = getVertexWaveData(texcoord.xy, waves, time);

    vs_out.normal = vertexData.normal.xyz;
    vs_out.tangent = vertexData.tangent.xyz;
    vs_out.binormal = vertexData.binormal.xyz;
    worldPos = vertex_model_to_world * vertexData.vertex;

    worldPos = worldPos / worldPos.w;
    // common
    vs_out.texcoord = texcoord.xy;
    vs_out.worldPos = worldPos;


    vec3 eye = normalize(worldPos.xyz - camera_position.xyz);
    vec3 refracted = normalize(refract(eye, vs_out.normal, eta));
    vs_out.reflected = normalize(reflect(eye, vs_out.normal));

    vs_out.reflectionFactor = fresnelBias + fresnelScale * pow(1. + dot(eye, vs_out.normal), fresnelPower);

    mat4 proj = vertex_world_to_clip;// * vertex_model_to_world;

    vec4 projectedRefractedPosition = proj * vec4(worldPos.xyz + refractionFactor * refracted, 1.0);
    vs_out.refractedPosition[0] = projectedRefractedPosition.xy / projectedRefractedPosition.w;

    projectedRefractedPosition = proj * vec4(worldPos.xyz + refractionFactor * normalize(refract(eye, vs_out.normal, eta * 0.96)), 1.0);
    vs_out.refractedPosition[1] = projectedRefractedPosition.xy / projectedRefractedPosition.w;

    projectedRefractedPosition = proj * vec4(worldPos.xyz + refractionFactor * normalize(refract(eye, vs_out.normal, eta * 0.92)), 1.0);
    vs_out.refractedPosition[2] = projectedRefractedPosition.xy / projectedRefractedPosition.w;

    gl_Position = vertex_world_to_clip * worldPos;
    
}
