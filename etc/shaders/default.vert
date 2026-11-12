#version 460
#ifndef WEBGPU
#extension GL_EXT_multiview : require
#endif
#extension GL_GOOGLE_include_directive : require

#include "lovr.glsl"

vec4 lovrmain() {
  return DefaultPosition;
}
