#version 460

layout(push_constant) uniform PushConstants { uint index; } push;

layout(location = 0) out vec4 color;

struct DrawData {
  uint id;
  uint material;
  uint padding0;
  uint padding1;
  vec4 color;
};

layout(set = 0, binding = 2) uniform DrawDataBuffer { DrawData draws[256]; };

void main() {
  color = draws[push.index].color;
}
