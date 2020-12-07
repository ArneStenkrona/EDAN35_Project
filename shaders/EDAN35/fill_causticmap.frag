// TODO: Refract ray from sun with water 
// Using waters normal
#version 410

uniform sampler2D environmentmap_texture;

in VS_OUT {
    vec4 fragPos;
    vec2 texcoord;
} fs_in;

out vec4 frag_color;

void main()
{
    frag_color =  vec4(0,1,0,1);
}
