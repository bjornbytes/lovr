#version 460
#extension GL_EXT_multiview : require
#extension GL_GOOGLE_include_directive : require

#include "lovr.glsl"

layout(set = 1, binding = 1) uniform textureCube SkyboxTexture;

layout(location = 2) in vec3 FragDirection;

void main() {
  PixelColors[0] = FragColor * texture(samplerCube(SkyboxTexture, Sampler), FragDirection);
}
