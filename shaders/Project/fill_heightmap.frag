#version 410

// Uniforms
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

const uint NBR_WAVES = 2;

// IO
in VS_OUT {
	vec2 texcoord;
} fs_in;

out vec4 normal_and_height;

// Helper functions
vec4 getNormalHeightVector(vec2 xy, Wave[NBR_WAVES] waves, float t) {

    float height = 0;
    float dHdx = 0;
    float dHdz = 0;
    Wave w;
    for(int i = 0; i < NBR_WAVES; i++) {
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
    // FORMAT: (vec3 Normal, float height)
    return vec4(-dHdx, 1, -dHdz, height);
}

void main()
{
    Wave[NBR_WAVES] waves; waves[0] = wave1; waves[1] = wave2;
    normal_and_height = getNormalHeightVector(fs_in.texcoord, waves, time); 
}
