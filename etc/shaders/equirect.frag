#version 460
#extension GL_EXT_multiview : require
#extension GL_GOOGLE_include_directive : require

#include "lovr.glsl"

layout(location = 0) in vec3 Direction;

vec4 lovrmain() {
  vec3 dir = normalize(Direction);
  float phi = acos(dir.y);
  float theta = atan(dir.x, -dir.z);
  vec2 uv = vec2(.5 + theta / (2 * PI), phi / PI);
  return Color * getPixel(ColorTexture, uv);
}
