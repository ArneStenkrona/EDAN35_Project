#version 410


uniform sampler2D underwater_texture;
uniform mat4 normal_model_to_world;
uniform vec3 camera_position;

in VS_OUT {
    vec2 refractedPosition[3];
    float reflectionFactor;
    vec3 worldPos;
    float distCamSquared;
} fs_in;


out vec4 colour;

uniform vec3 atmosphereColour;
uniform vec3 underwaterColour;

void main()
{
// Color coming from the sky reflection
  vec3 reflectedColor = (fs_in.worldPos.y < camera_position.y) ? underwaterColour : atmosphereColour; //vec3(1);//textureCube(skybox, reflected).xyz;

  // Color coming from the environment refraction, applying chromatic aberration
  vec3 refractedColor = vec3(1.);
  refractedColor.r = texture2D(underwater_texture, fs_in.refractedPosition[0] * 0.5 + 0.5).r;
  refractedColor.g = texture2D(underwater_texture, fs_in.refractedPosition[1] * 0.5 + 0.5).g;
  refractedColor.b = texture2D(underwater_texture, fs_in.refractedPosition[2] * 0.5 + 0.5).b;

  vec4 deep = vec4(mix(refractedColor, reflectedColor, clamp(fs_in.reflectionFactor, 0., 1.)), 1.);
  //deep = mix(deep, shallow_water, 0.1);


  //colour = vec4((normalize(fs_in.normal) + 1.0) * 0.5, 1.0);
  //colour = mix( atmosphere, deep, 1 - max(dot(V,n), 0));

  colour = deep;
  
}
