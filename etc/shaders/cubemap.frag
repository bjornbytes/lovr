#version 460
#ifndef WEBGPU
#extension GL_EXT_multiview : require
#endif
#extension GL_GOOGLE_include_directive : require

#include "lovr.glsl"

layout(set = 2, binding = 0) uniform textureCube SkyboxTexture;

vec4 lovrmain() {
  return Color * getPixel(SkyboxTexture, Normal * vec3(1, 1, -1));
}
