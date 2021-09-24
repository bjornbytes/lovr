#version 460

layout(constant_id = 1000) const bool useGlobalColor = true;
layout(constant_id = 1001) const bool useVertexColors = false;

layout(location = 0) in vec4 inGlobalColor;
layout(location = 1) in vec4 inVertexColor;

layout(location = 0) out vec4 outColor;

void main() {
  outColor = vec4(1.);

  if (useGlobalColor) {
    outColor *= inGlobalColor;
  }

  if (useVertexColors) {
    outColor *= inVertexColor;
  }
}
