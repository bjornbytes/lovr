#version 460
#extension GL_EXT_multiview : require
#extension GL_GOOGLE_include_directive : require

#include "lovr.glsl"

void main() {
  Color = PassColor * VertexColor;
  Normal = normalize(NormalMatrix * VertexNormal);
  UV = VertexUV;
  Position = DefaultPosition;
  PointSize = 1.f;
}
