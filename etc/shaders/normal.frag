#version 460
#extension GL_EXT_multiview : require
#extension GL_GOOGLE_include_directive : require

#include "lovr.glsl"

vec4 lovrmain() {
  return vec4(normalize(Normal) * .5 + .5, 1);
}
