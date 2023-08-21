#version 460
#extension GL_EXT_multiview : require
#extension GL_GOOGLE_include_directive : require

#include "lovr.glsl"

vec4 lovrmain() {
  vec3 dir = normalize(Normal);

  float theta1 = -atan(dir.x, dir.z) / (2. * PI);
  float theta2 = fract(theta1);

  float dx1 = dFdx(theta1);
  float dy1 = dFdy(theta1);
  float dx2 = dFdx(theta2);
  float dy2 = dFdy(theta2);

  float phi = acos(dir.y) / PI;

  vec2 dx = vec2(abs(dx1) - 1e-5 < abs(dx2) ? dx1 : dx2, dFdx(phi));
  vec2 dy = vec2(abs(dy1) - 1e-5 < abs(dy2) ? dy1 : dy2, dFdy(phi));

  return Color * textureGrad(sampler2D(ColorTexture, Sampler), vec2(theta1, phi), dx, dy);
}
