#version 410


uniform sampler2D underwater_texture;
uniform mat4 normal_model_to_world;
uniform vec3 camera_position;

in VS_OUT {
    vec2 refractedPosition[3];
    float reflectionFactor;
    vec3 normal;
    vec3 worldPos;
} fs_in;


out vec4 colour;

const vec4 atmosphere = vec4(0.529, .808, .426, 1.0);
const vec4 deep_water = vec4(0, 0, 0.1, 1.0);
const vec4 shallow_water = vec4(0, 0.5, 0.5, 1.0);

void main()
{
// Color coming from the sky reflection
  vec3 reflectedColor = vec3(1);//textureCube(skybox, reflected).xyz;

  // Color coming from the environment refraction, applying chromatic aberration
  vec3 refractedColor = vec3(1.);
  refractedColor.r = texture2D(underwater_texture, fs_in.refractedPosition[0] * 0.5 + 0.5).r;
  refractedColor.g = texture2D(underwater_texture, fs_in.refractedPosition[1] * 0.5 + 0.5).g;
  refractedColor.b = texture2D(underwater_texture, fs_in.refractedPosition[2] * 0.5 + 0.5).b;

  vec4 deep = vec4(mix(refractedColor, reflectedColor, clamp(fs_in.reflectionFactor, 0., 1.)), 1.);
  //deep = mix(deep, shallow_water, 0.1);

  vec3 n = normalize(fs_in.normal);
  vec3 V = normalize(fs_in.worldPos - camera_position);

  //colour = vec4((normalize(fs_in.normal) + 1.0) * 0.5, 1.0);
  //colour = mix( atmosphere, deep, 1 - max(dot(V,n), 0));
  colour = deep;
}
