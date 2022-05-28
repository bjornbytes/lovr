#version 460
#extension GL_EXT_multiview : require

struct Camera {
  mat4 view;
  mat4 projection;
  mat4 viewProjection;
  mat4 inverseProjection;
};

struct PerDraw {
  mat4 transform;
  mat4 normalMatrix;
  vec4 color;
};

layout(set = 0, binding = 0) uniform Cameras { Camera cameras[6]; };
layout(set = 0, binding = 1) uniform Draws { PerDraw draws[6]; };
layout(set = 0, binding = 2) uniform sampler defaultSampler;

layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec4 inNormal;
layout(location = 2) in vec4 inUV;
layout(location = 3) in vec4 inColor;
layout(location = 4) in vec4 inTangent;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outUV;

void main() {
  uint drawId = gl_BaseInstance & 0xff;

  outColor = vec4(1.);
  outColor *= draws[drawId].color;
  outColor *= inColor;

  outUV = inUV;

  gl_Position = cameras[gl_ViewIndex].viewProjection * (draws[drawId].transform * vec4(inPosition.xyz, 1.));
  gl_PointSize = 1.f;
}
