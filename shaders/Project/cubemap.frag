#version 410

uniform mat4 normal_model_to_world; // transform between model space and world space for vectors
uniform samplerCube cubemap_texture;
uniform int has_cubemap_texture;

in VS_OUT{
    vec3 world_view;	// view vector in world space
} fs_in;

out vec4 frag_color;

void main()
{
    if(has_cubemap_texture != 0) {
        vec3 v = normalize(fs_in.world_view);
        frag_color.xyz = texture(cubemap_texture, -v).xyz; 
    } else
        frag_color.xyz = vec3(1.0f);
    
    frag_color.w = 1.0f;
}
