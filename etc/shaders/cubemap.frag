#version 460
#extension GL_EXT_multiview : require
#extension GL_GOOGLE_include_directive : require

#include "lovr.glsl"

layout(set = 1, binding = 1) uniform textureCube SkyboxTexture;
layout(location = 0) in vec3 Direction;

void main() {
  PixelColors[0] = Color * getPixel(SkyboxTexture, Direction);
}
