#version 410

precision highp float;
precision highp int;

const float PI = 3.141592653589793;
uniform sampler2D sim_texture;

uniform vec2 center;
uniform float radius;
uniform float strength;

in VS_OUT {
    vec2 texcoord;
} fs_in;

void main() {
  /* Get vertex info */
  vec4 info = texture2D(sim_texture, fs_in.texcoord);

  /* Add the drop to the height */
  float drop = max(0.0, 1.0 - length(center * 0.5 + 0.5 - fs_in.texcoord) / radius);
  drop = 0.5 - cos(drop * PI) * 0.5;
  info.r += drop * strength;

  gl_FragColor = info;
}