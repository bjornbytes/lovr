#version 460
#extension GL_EXT_multiview : require
#extension GL_GOOGLE_include_directive : require

#include "lovr.glsl"

void main() {
  FragColor = VertexColor * Color;
  float x = -1 + float((VertexIndex & 1) << 2);
  float y = -1 + float((VertexIndex & 2) << 1);
  FragUV = vec2(x, y) * .5 + .5;
  Position = vec4(x, y, 0., 1.);
  PointSize = 1.f;
}
