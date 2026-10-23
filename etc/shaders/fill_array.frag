#version 460
#ifndef WEBGPU
#extension GL_EXT_multiview : require
#endif
#extension GL_EXT_samplerless_texture_functions : require
#extension GL_GOOGLE_include_directive : require

#include "lovr.glsl"

layout(set = 2, binding = 0) uniform texture2DArray ArrayTexture;

vec4 lovrmain() {
  return Color * getPixel(ArrayTexture, UV, min(ViewIndex, textureSize(ArrayTexture, 0).z - 1));
}
