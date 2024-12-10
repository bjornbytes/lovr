#version 460
#extension GL_EXT_multiview : require
#extension GL_GOOGLE_include_directive : require

#include "lovr.glsl"

vec4 lovrmain() {
  if (VertexPosition.z != ViewIndex) {
    return vec4(0.);
  }

  vec4 clip = Projection * vec4(VertexPosition.xy, -1., 1.);
  clip.z = clip.w; // Sets NDC z to 1 (near plane)
  return clip;
}
