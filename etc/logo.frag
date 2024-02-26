#version 460
#extension GL_EXT_multiview : require
#extension GL_GOOGLE_include_directive : require

#include "shaders/lovr.glsl"

vec4 lovrmain() {
  float y = UV.y;
  vec2 uv = vec2(UV.x, 1. - UV.y);
  uv = uv * 4. - 2.;
  const float k = sqrt(3.);
  uv.x = abs(uv.x) - 1.;
  uv.y = uv.y + 1. / k + .25;
  if (uv.x + k * uv.y > 0.) {
    uv = vec2(uv.x - k * uv.y, -k * uv.x - uv.y) / 2.;
  }
  uv.x -= clamp(uv.x, -2., 0.);
  float sdf = -length(uv) * sign(uv.y) - .5;
  float w = fwidth(sdf) * .5;
  float alpha = smoothstep(.22 + w, .22 - w, sdf);
  vec3 color = mix(vec3(.094, .662, .890), vec3(.913, .275, .6), clamp(y * 1.5 - .25, 0., 1.));
  color = mix(color, vec3(.2, .2, .24), smoothstep(-.12 + w, -.12 - w, sdf));
  return vec4(pow(color, vec3(2.2)), alpha);
}
