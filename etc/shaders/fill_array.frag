#version 460
#extension GL_EXT_multiview : require
#extension GL_EXT_samplerless_texture_functions : require
#extension GL_GOOGLE_include_directive : require

#include "lovr.glsl"

layout(set = 1, binding = 1) uniform texture2DArray ArrayTexture;

vec4 lovrmain() {
  return Color * getPixel(ArrayTexture, UV, min(ViewIndex, textureSize(ArrayTexture, 0).z - 1));
}
