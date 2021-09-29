#version 460

layout(location = 0) out vec4 outGlobalColor;

struct DrawData {
  vec4 color;
};

layout(set = 0, binding = 2) uniform DrawDataBuffer { DrawData draws[256]; };

void main() {
  uint localIndex = gl_BaseInstance & 0xff;
  outGlobalColor = draws[localIndex].color;
  float x = -1. + float((gl_VertexIndex & 1) << 2);
  float y = -1. + float((gl_VertexIndex & 2) << 1);
  gl_Position = vec4(x, y, 0., 1.);
}
