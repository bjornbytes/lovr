#version 460
#extension GL_EXT_multiview : require

layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec4 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec4 inColor;

layout(location = 0) flat out uint outDrawIndex;
layout(location = 1) out vec4 outVertexColor;
layout(location = 2) out vec2 outUV;

struct Camera {
  mat4 view[6];
  mat4 projection[6];
  mat4 viewProjection[6];
  mat4 inverseViewProjection[6];
};

struct DrawData {
  vec4 color;
};

layout(set = 0, binding = 0) uniform CameraBuffer { Camera camera; };
layout(set = 0, binding = 1) uniform TransformBuffer { mat4 transforms[256]; };
layout(set = 0, binding = 2) uniform DrawDataBuffer { DrawData draws[256]; };
layout(set = 0, binding = 3) uniform sampler nearest;
layout(set = 0, binding = 4) uniform sampler bilinear;
layout(set = 0, binding = 5) uniform sampler trilinear;
layout(set = 0, binding = 6) uniform sampler anisotropic;

void main() {
  outDrawIndex = gl_BaseInstance & 0xff;
  outVertexColor = inColor;
  outUV = inUV;
  gl_Position = camera.viewProjection[gl_ViewIndex] * transforms[outDrawIndex] * inPosition;
  gl_PointSize = 1.f;
}
