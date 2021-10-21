#version 460

layout(constant_id = 1000) const uint materialCount = 1;
layout(constant_id = 1001) const uint textureCount = 1;
layout(constant_id = 1002) const bool useGlobalColor = true;
layout(constant_id = 1003) const bool useVertexColors = false;
layout(constant_id = 1004) const bool useDefaultTexture = true;

layout(location = 0) flat in uint inDrawIndex;
layout(location = 1) in vec4 inVertexColor;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec4 outColor;

struct DrawData {
  vec4 color;
  uint material;
  uint pad[3];
};

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
  outColor = vec4(1.);

  if (useGlobalColor) {
    outColor *= draws[inDrawIndex].color;
  }

  if (useVertexColors) {
    outColor *= inVertexColor;
  }

  if (useDefaultTexture) {
    outColor *= texture(sampler2D(textures2d[materials[draws[inDrawIndex].material].texture], nearest), inUV);
  }
}
