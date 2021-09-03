#version 460
#extension GL_EXT_multiview : require

layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec4 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec4 inColor;

layout(location = 0) out vec4 outGlobalColor;
layout(location = 1) out vec4 outVertexColor;

struct Camera {
  mat4 view[6];
  mat4 projection[6];
  mat4 viewProjection[6];
};

struct DrawData {
  uint id;
  uint material;
  uint padding0;
  uint padding1;
  vec4 color;
};

layout(set = 0, binding = 0) uniform CameraBuffer { Camera camera; };
layout(set = 0, binding = 1) uniform TransformBuffer { mat4 transforms[256]; };
layout(set = 0, binding = 2) uniform DrawDataBuffer { DrawData draws[256]; };

void main() {
  outGlobalColor = draws[gl_BaseInstance].color;
  outVertexColor = inColor;
  gl_PointSize = 1.f;
  gl_Position = camera.viewProjection[gl_ViewIndex] * transforms[gl_BaseInstance] * inPosition;
}
