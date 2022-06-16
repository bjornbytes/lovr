#version 460
#extension GL_EXT_multiview : require
#extension GL_GOOGLE_include_directive : require

#include "lovr.glsl"

void main() {
  FragColor = VertexColor * Color;
  FragUV = VertexUV;
  Position = DefaultPosition;
  PointSize = 1.f;
}
