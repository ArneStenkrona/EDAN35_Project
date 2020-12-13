#version 410

uniform vec3 camera_position;
uniform sampler2D underwater_texture;
uniform samplerCube cubemap_texture;

uniform vec3 atmosphereColour;
uniform vec3 underwaterColour;

in VS_OUT {
    vec3 worldPos;
} fs_in;

layout (location = 0) out vec4 underwater_shade;

void main()
{
    underwater_shade = mix( vec4(vec3(0.5),1), texture(cubemap_texture, normalize(fs_in.worldPos - camera_position)), 0.7); 
    //underwater_shade = vec4(1,0,0,1);
}