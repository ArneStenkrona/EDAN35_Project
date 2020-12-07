#version 410

uniform sampler2D diffuse_texture;
uniform sampler2D specular_texture;
uniform sampler2D light_d_texture;
uniform sampler2D light_s_texture;
uniform sampler2D depth_texture;
uniform sampler2D caustic_texture;
uniform mat4 depth_to_world;
uniform mat4 world_to_sun;


in VS_OUT {
	vec2 texcoord;
} fs_in;

out vec4 frag_color;

void main()
{
	vec3 diffuse  = texture(diffuse_texture,  fs_in.texcoord).rgb;
	vec3 specular = texture(specular_texture, fs_in.texcoord).rgb;

	vec3 light_d  = texture(light_d_texture,  fs_in.texcoord).rgb;
	vec3 light_s  = texture(light_s_texture,  fs_in.texcoord).rgb;

	float depth = texture(depth_texture,  fs_in.texcoord).r;
	vec4 wPos = depth_to_world * vec4(fs_in.texcoord * 2. - 1., depth, 1.);
	wPos = wPos / wPos.w;
	vec2 caustic_coord = (world_to_sun * wPos).xy * 0.5 + 0.5;
	vec3 caustic = texture(caustic_texture, caustic_coord).rgb;

	const vec3 ambient = vec3(0.15);

	frag_color =  vec4((ambient + light_d) * diffuse + light_s * specular + caustic, 1.0);
}
