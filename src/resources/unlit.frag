#version 460

layout(location = 0) in vec4 inGlobalColor;
layout(location = 1) in vec4 inVertexColor;

layout(location = 0) out vec4 outColor;

void main() {
  outColor = inGlobalColor * inVertexColor;
}
