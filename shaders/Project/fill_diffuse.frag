#version 410

uniform bool has_diffuse_texture;
uniform bool has_opacity_texture;
uniform sampler2D diffuse_texture;

in VS_OUT {
	vec3 normal;
	vec2 texcoord;
	vec3 tangent;
	vec3 binormal;
} fs_in;

layout (location = 0) out vec4 diffuse_output;

void main()
{
	if (has_opacity_texture)
		discard;

	// Diffuse color
	diffuse_output = vec4(0.0f);
	if (has_diffuse_texture)
		diffuse_output = texture(diffuse_texture, fs_in.texcoord);
}
