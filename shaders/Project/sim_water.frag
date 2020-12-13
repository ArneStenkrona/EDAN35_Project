#version 410

precision highp float;
precision highp int;

uniform sampler2D sim_texture;
vec2 delta = vec2(1.0/216.0f,1.0/216.0f);

in VS_OUT {
    vec2 texcoord;
} fs_in;


void main() {
  /* get vertex info */
  vec4 info = texture2D(sim_texture, fs_in.texcoord);

  /* calculate average neighbor height */
  vec2 dx = vec2(delta.x, 0.0);
  vec2 dy = vec2(0.0, delta.y);
  float average = (
    texture2D(sim_texture, fs_in.texcoord - dx).r +
    texture2D(sim_texture, fs_in.texcoord - dy).r +
    texture2D(sim_texture, fs_in.texcoord + dx).r +
    texture2D(sim_texture, fs_in.texcoord + dy).r
  ) * 0.25;

  /* change the velocity to move toward the average */
  info.g += (average - info.r) * 2.0;

  /* attenuate the velocity a little so waves do not last forever */
  info.g *= 0.995;

  /* move the vertex along the velocity */
  info.r += info.g;

  /* update the normal */
  vec3 ddx = vec3(delta.x, texture2D(sim_texture, vec2(fs_in.texcoord.x + delta.x, fs_in.texcoord.y)).r - info.r, 0.0);
  vec3 ddy = vec3(0.0, texture2D(sim_texture, vec2(fs_in.texcoord.x, fs_in.texcoord.y + delta.y)).r - info.r, delta.y);
  info.ba = normalize(cross(ddy, ddx)).xz;

  gl_FragColor = info;
}