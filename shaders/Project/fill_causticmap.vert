#version 410

uniform bool has_environmentmap_texture;
uniform sampler2D environmentmap_texture;
uniform mat4 normal_model_to_world;

uniform mat4 vertex_model_to_world;
uniform mat4 vertex_world_to_clip;

layout (location = 0) in vec3 vertex;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec3 texcoord;
layout (location = 3) in vec3 tangent;
layout (location = 4) in vec3 binormal;



out VS_OUT {
    vec3 oldPos;
    vec3 newPos;
    vec3 normal;
//    vec3 tangent;
//    vec3 binormal;
    float waterDepth;
    float depth;
} vs_out;

uniform vec2 inv_res;
//uniform vec3 light_color;
uniform vec3 light_direction;
uniform vec2 environmentmap_texel_size;

const float eta = 0.7504;
const int max_iter = 50;

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

WaveCalculation getVertexWaveData(vec2 uv, Wave[2] waves, float t) {

    float height = 0;
    float dHdx = 0;
    float dHdz = 0;
    Wave w;
    for(int i = 0; i < 2; i++) {
        w = waves[i];
        float dirFact = w.Direction.x * uv.x + w.Direction.y * uv.y;
        float sinFact = sin(dirFact * w.Frequency + t * w.Phase) * 0.5 + 0.5;
        float cosFact = 0.5 * w.Sharpness * w.Frequency * w.Amplitude * cos(dirFact * w.Frequency + t * w.Phase);
        height += w.Amplitude * pow(sinFact, w.Sharpness);
        float cosSinFact = pow(sinFact, w.Sharpness - 1) * cosFact;
        dHdx += cosSinFact * w.Direction.x;
        dHdz += cosSinFact * w.Direction.y;
    }

    // Fill output in struct
    WaveCalculation result;
    result.vertex = vec4(vertex + vec3(0, height, 0), 1.0);
    // Convert from tangent space to model space
    result.normal = vec4(vec3(-dHdx, 1, -dHdz), 0.0);
    result.tangent = vec4(vec3(1, dHdx, 0), 0.0);
    result.binormal = vec4(vec3(0, dHdz, 1), 0.0);
    return result;
}

void main()
{
    Wave[2] waves; waves[0] = wave1; waves[1] = wave2;
    WaveCalculation vertexData = getVertexWaveData(texcoord.xy, waves, time);

    vs_out.normal = vec3(normalize(normal_model_to_world * vertexData.normal));
//    vs_out.tangent  = normalize(tangent);
//    vs_out.binormal = normalize(binormal);
    
    vec4 worldPos = vertex_model_to_world * vertexData.vertex;

    vs_out.oldPos = worldPos.xyz;

    vec4 projectedPos = vertex_world_to_clip * worldPos;

    vec2 currPos = projectedPos.xy;
    vec2 coords = 0.5 + 0.5 * currPos;

    vec3 refracted = refract(normalize(light_direction), normalize(vs_out.normal), eta);
    vec4 projectedRefraction = vertex_world_to_clip * vec4(refracted, 1.); // 1.???

    vec3 refractedDirection = projectedRefraction.xyz;

    vs_out.waterDepth = 0.5 + 0.5 * projectedPos.z / projectedPos.w;
    float currentDepth = projectedPos.z;
    vec4 environment = texture(environmentmap_texture, coords);

    float factor = environmentmap_texel_size.x / length(refractedDirection.xy); // should be 2D
//    float factor = length(environmentmap_texel_size) / length(refractedDirection.xy); 

    vec2 deltaDirection = refractedDirection.xy * factor;
    float deltaDepth = refractedDirection.z * factor;

    for (int i = 0; i < max_iter; ++i) {
        currPos += deltaDirection;
        currentDepth += deltaDepth;

        if (environment.w <= currentDepth) {
            break;
        }

        environment = texture2D(environmentmap_texture, 0.5 + 0.5 * currPos);
    }

    vs_out.newPos = environment.xyz;
    vec4 projectedEnvPos = vertex_world_to_clip * vec4(vs_out.newPos, 1.0);
    vs_out.depth = 0.5 + 0.5 * projectedEnvPos.z / projectedEnvPos.w;

    gl_Position = projectedEnvPos;
}
