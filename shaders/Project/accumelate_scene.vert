#version 410

uniform mat4 vertex_model_to_world;
uniform mat4 vertex_world_to_clip;

uniform bool is_water;

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

void main() {

	vs_out.texcoord = texcoord.xy;
    if (is_water) 
    {
        Wave[2] waves; waves[0] = wave1; waves[1] = wave2;
        WaveCalculation vertexData = getVertexWaveData(texcoord.xy, waves, time);

        vs_out.normal = normalize(vertexData.normal.xyz);
        vs_out.tangent = normalize(vertexData.tangent.xyz);
        vs_out.binormal = normalize(vertexData.binormal.xyz);
        gl_Position = vertex_world_to_clip * vertex_model_to_world * vertexData.vertex;
    } 
    else 
    {
        vs_out.normal   = normalize(normal);
	    vs_out.tangent  = normalize(tangent);
	    vs_out.binormal = normalize(binormal);
	    gl_Position = vertex_world_to_clip * vertex_model_to_world * vec4(vertex, 1.0);
    }
	
}
