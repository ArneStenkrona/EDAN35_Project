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
uniform mat4 normal_model_to_world;

uniform mat4 view_projection_inverse;
uniform vec3 camera_position;
uniform mat4 shadow_view_projection;

uniform vec3 sun_dir;

uniform vec2 inv_res;

in VS_OUT {
    vec3 normal;
    vec2 texcoord;
    vec3 tangent;
    vec3 binormal;
    vec4 worldPos;
    vec2 refractedPosition[3];
    vec3 reflected;
    float reflectionFactor;
} fs_in;

out vec4 colour;

void main()
{
// Color coming from the sky reflection
  vec3 reflectedColor = texture(cubemap_texture, fs_in.reflected).xyz;

  // Color coming from the environment refraction, applying chromatic aberration
  vec3 refractedColor = vec3(1.);
  refractedColor.r = texture2D(underwater_texture, fs_in.refractedPosition[0] * 0.5 + 0.5).r;
  refractedColor.g = texture2D(underwater_texture, fs_in.refractedPosition[1] * 0.5 + 0.5).g;
  refractedColor.b = texture2D(underwater_texture, fs_in.refractedPosition[2] * 0.5 + 0.5).b;

  colour = vec4(mix(refractedColor, reflectedColor, clamp(fs_in.reflectionFactor, 0., 1.)), 1.);
}
