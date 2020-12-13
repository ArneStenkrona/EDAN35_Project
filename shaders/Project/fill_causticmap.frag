// TODO: Refract ray from sun with water 
// Using waters normal
#version 410

in VS_OUT {
    vec3 oldPos;
    vec3 newPos;
    vec3 normal;
//    vec3 tangent;
//    vec3 binormal;
    float waterDepth;
    float depth;
} fs_in;

layout (location = 0) out vec4 caustic_map;

const float causticsFactor = 0.15;

void main()
{
    float causticsIntensity = 0.;

    if (fs_in.depth >= fs_in.waterDepth) {
        float oldArea = length(dFdx(fs_in.oldPos)) * length(dFdy(fs_in.oldPos));
        float newArea = length(dFdx(fs_in.newPos)) * length(dFdy(fs_in.newPos));

        float ratio;

        // Prevent dividing by zero (debug NVidia drivers)
        if (newArea == 0.) {
        // Arbitrary large value
            ratio = 2.0e+20;
        } else {
            ratio = oldArea / newArea;
        }
        causticsIntensity = causticsFactor * ratio;
    }

    caustic_map = vec4(fs_in.normal, 1.0);//vec4(vec3(causticsIntensity), fs_in.depth);
}
