// TODO: Refract ray from sun with water 
// Using waters normal
#version 410


uniform bool has_normals_texture;
uniform bool has_environmentmap_texture;
uniform sampler2D normals_texture;
uniform sampler2D environmentmap_texture;
uniform mat4 normal_model_to_world;

in VS_OUT {
    vec4 fragPos;
    vec2 texcoord;
//    vec2 texcoord_env;
} fs_in;

layout (location = 0) out vec4 caustic_map;

void main()
{
    vec3 col = texture(environmentmap_texture, gl_FragCoord.xy / 2048).xyz;

    caustic_map = vec4(col, 1.0);
}
