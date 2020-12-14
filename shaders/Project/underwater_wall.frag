#version 410

uniform vec3 camera_position;
uniform sampler2D underwater_texture;
uniform samplerCube cubemap_texture;

uniform vec3 atmosphereColour;
uniform vec3 underwaterColour;

uniform bool IN_WATER;

const vec3 outide_water_tint = vec3(0.4, 0.45, 0.5);

in VS_OUT {
    vec3 worldPos;
} fs_in;

layout (location = 0) out vec4 underwater_shade;

void main()
{
    vec3 skyboxMixColour = IN_WATER ? underwaterColour : outide_water_tint;
    vec4 result = mix( vec4(outide_water_tint,1), 
        texture(cubemap_texture, normalize(fs_in.worldPos - camera_position)), 0.7); 

    if (IN_WATER) 
        result = mix(texture(cubemap_texture, normalize(fs_in.worldPos - camera_position)), vec4(underwaterColour, 1), 0.2);

    underwater_shade = result;
    //underwater_shade = vec4(1,0,0,1);
}