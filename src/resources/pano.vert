#version 460

layout(location = 0) out vec4 outGlobalColor;
layout(location = 1) out vec2 outUV;

struct DrawData {
  vec4 color;
};

layout(set = 0, binding = 2) uniform DrawDataBuffer { DrawData draws[256]; };

void main() {
  uint localIndex = gl_BaseInstance & 0xff;
  outGlobalColor = draws[localIndex].color;
  float x = -1. + float((gl_VertexIndex & 1) << 2);
  float y = -1. + float((gl_VertexIndex & 2) << 1);
  outUV = vec2(x, y);
  gl_Position = vec4(x, y, 1., 1.);
}
