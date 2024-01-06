#version 460
#extension GL_EXT_multiview : require
#extension GL_GOOGLE_include_directive : require

#include "lovr.glsl"

layout(set = 1, binding = 1) uniform textureCube SkyboxTexture;

vec4 lovrmain() {
  return Color * getPixel(SkyboxTexture, Normal * vec3(1, 1, -1));
}
