#version 460
#extension GL_EXT_multiview : require

layout(constant_id = 1002) const uint materialCount = 1;
layout(constant_id = 1003) const uint textureCount = 1;

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
  mat4 inverseViewProjection[6];
};

struct DrawData {
  vec4 color;
  /*uint material;
  uint pad[3];*/
};

layout(set = 0, binding = 0) uniform CameraBuffer { Camera camera; };
layout(set = 0, binding = 1) uniform TransformBuffer { mat4 transforms[256]; };
layout(set = 0, binding = 2) uniform DrawDataBuffer { DrawData draws[256]; };
layout(set = 0, binding = 3) uniform sampler nearest;
layout(set = 0, binding = 4) uniform sampler bilinear;
layout(set = 0, binding = 5) uniform sampler trilinear;
layout(set = 0, binding = 6) uniform sampler anisotropic;

struct BasicMaterial {
  uint color;
  uint texture;
  vec2 uvOffset;
  vec2 uvScale;
};

#define Material(T) layout(set = 1, binding = 0) uniform Materials { T materials[materialCount]; };
layout(set = 1, binding = 1) uniform texture2D textures2d[textureCount];
layout(set = 1, binding = 1) uniform textureCube texturesCube[textureCount];

Material(BasicMaterial);

void main() {
  uint localIndex = gl_BaseInstance & 0xff;
  outGlobalColor = draws[localIndex].color;
  outVertexColor = inColor;
  gl_Position = camera.viewProjection[gl_ViewIndex] * transforms[localIndex] * inPosition;
  gl_PointSize = 1.f;
}
