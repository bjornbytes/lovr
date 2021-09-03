#version 460

layout(location = 0) in vec4 inGlobalColor;
layout(location = 1) in vec4 inVertexColor;

layout(location = 0) out vec4 outColor;

struct DrawData {
  uint id;
  uint material;
  uint padding0;
  uint padding1;
  vec4 color;
};

layout(set = 0, binding = 2) uniform DrawDataBuffer { DrawData draws[256]; };

void main() {
  outColor = inGlobalColor * inVertexColor;
}
