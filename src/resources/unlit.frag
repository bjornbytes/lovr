#version 460

struct Material {
  vec4 color;
  vec2 uvShift;
  vec2 uvScale;
};

layout(constant_id = 1000) const bool applyGlobalColor = true;
layout(constant_id = 1001) const bool applyVertexColors = false;
layout(constant_id = 1002) const bool applyMaterialColor = true;
layout(constant_id = 1003) const bool applyMaterialTexture = true;
layout(constant_id = 1004) const bool applyUVTransform = true;

layout(location = 0) in vec4 inColor;
layout(location = 1) in vec2 inUV;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 3) uniform sampler nearest;
layout(set = 0, binding = 4) uniform sampler bilinear;
layout(set = 0, binding = 5) uniform sampler trilinear;
layout(set = 0, binding = 6) uniform sampler anisotropic;

layout(set = 1, binding = 0) uniform Materials { Material material; };
layout(set = 1, binding = 1) uniform texture2D colorTexture;

void main() {
  outColor = inColor;

  vec2 uv = inUV;
  if (applyUVTransform) {
    uv *= material.uvScale;
    uv += material.uvShift;
  }

  if (applyMaterialColor) {
    outColor *= material.color;
  }

  if (applyMaterialTexture) {
    outColor *= texture(sampler2D(colorTexture, trilinear), uv);
  }
}
