#version 460
#extension GL_EXT_multiview : require

struct Camera {
  mat4 view;
  mat4 projection;
  mat4 viewProjection;
  mat4 inverseProjection;
};

struct Draw {
  mat4 transform;
  mat4 normalMatrix;
  vec4 color;
};

layout(constant_id = 1000) const bool applyGlobalColor = true;
layout(constant_id = 1001) const bool applyVertexColors = false;
layout(constant_id = 1002) const bool applyMaterialColor = true;
layout(constant_id = 1003) const bool applyMaterialTexture = true;
layout(constant_id = 1004) const bool applyUVTransform = true;

layout(set = 0, binding = 0) uniform Cameras { Camera cameras[6]; };
layout(set = 0, binding = 1) uniform Draws { Draw draws[256]; };
layout(set = 0, binding = 2) uniform sampler nearest;
layout(set = 0, binding = 3) uniform sampler bilinear;
layout(set = 0, binding = 4) uniform sampler trilinear;
layout(set = 0, binding = 5) uniform sampler anisotropic;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec4 inColor;
layout(location = 4) in vec4 inTangent;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec2 outUV;

void main() {
  uint drawId = gl_BaseInstance & 0xff;

  outColor = vec4(1.);
  if (applyGlobalColor) outColor *= draws[drawId].color;
  if (applyVertexColors) outColor *= inColor;
  outUV = inUV;

  gl_Position = cameras[gl_ViewIndex].viewProjection * (draws[drawId].transform * vec4(inPosition, 1.));
  gl_PointSize = 1.f;
}
