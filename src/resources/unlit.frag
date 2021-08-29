#version 460

layout(push_constant) uniform PushConstants { uint index; } push;

layout(location = 0) in vec4 inColor;

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
  outColor = inColor * draws[push.index].color;
}
