#version 460
#extension GL_EXT_multiview : require
#extension GL_GOOGLE_include_directive : require

#define PI 3.141592653589793238462643383

#include "lovr.glsl"

layout(location = 2) in vec3 FragDirection;

void main() {
  vec3 dir = normalize(FragDirection);
  float phi = acos(dir.y);
  float theta = atan(dir.x, -dir.z);
  vec2 uv = vec2(.5 + theta / (2 * PI), phi / PI);
  PixelColors[0] = FragColor * texture(sampler2D(Texture, Sampler), uv);
}
