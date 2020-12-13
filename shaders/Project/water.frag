#version 410

uniform bool has_diffuse_texture;
uniform bool has_specular_texture;
uniform bool has_normals_texture;
uniform bool has_opacity_texture;
uniform bool has_cubemap_texture;
uniform sampler2D diffuse_texture;
uniform sampler2D specular_texture;
uniform sampler2D normals_texture;
uniform sampler2D opacity_texture;

uniform sampler2D underwater_texture;
uniform samplerCube cubemap_texture;
uniform sampler2D underwater_depth_texture;
uniform mat4 normal_model_to_world;
uniform vec3 camera_position;
uniform mat4 shadow_view_projection;

uniform vec3 sun_dir;

uniform vec2 inv_res;
uniform float t;

in VS_OUT {
    vec2 refractedPosition[3];
    float reflectionFactor;
    vec3 worldPos;
    float distCamSquared;
    vec3 reflected;
} fs_in;


float lineariseDepth(float value)
{
	return (2.0 * 0.01) / (30 + 0.01 - value * (30 - 0.01));
}

out vec4 colour;

uniform vec3 atmosphereColour;
uniform vec3 underwaterColour;

void main()
{
// Color coming from the sky reflection
  vec3 reflectedColor = texture(cubemap_texture, fs_in.reflected).xyz;
//  vec3 reflectedColor = (fs_in.worldPos.y < camera_position.y) ? underwaterColour : skycolor;

  // Color coming from the environment refraction, applying chromatic aberration
  vec3 refractedColor = vec3(1.);
  refractedColor.r = texture2D(underwater_texture, fs_in.refractedPosition[0] * 0.5 + 0.5).r;
  refractedColor.g = texture2D(underwater_texture, fs_in.refractedPosition[1] * 0.5 + 0.5).g;
  refractedColor.b = texture2D(underwater_texture, fs_in.refractedPosition[2] * 0.5 + 0.5).b;

  vec4 deep = vec4(mix(refractedColor, reflectedColor, clamp(fs_in.reflectionFactor, 0., 1.)), 1.);
  //deep = mix(deep, shallow_water, 0.1);

  colour = deep;
}
