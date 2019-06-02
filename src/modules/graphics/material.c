#include "graphics/material.h"
#include "graphics/graphics.h"
#include "graphics/shader.h"
#include "graphics/texture.h"
#include "resources/shaders.h"
#include "core/ref.h"
#include <stdlib.h>
#include <math.h>

Material* lovrMaterialInit(Material* material) {
  for (int i = 0; i < MAX_MATERIAL_SCALARS; i++) {
    material->scalars[i] = 1.f;
  }

  for (int i = 0; i < MAX_MATERIAL_COLORS; i++) {
    if (i == COLOR_EMISSIVE) {
      material->colors[i] = (Color) { 0.f, 0.f, 0.f, 0.f };
    } else {
      material->colors[i] = (Color) { 1.f, 1.f, 1.f, 1.f };
    }
  }

  lovrMaterialSetTransform(material, 0.f, 0.f, 1.f, 1.f, 0.f);
  return material;
}

void lovrMaterialDestroy(void* ref) {
  Material* material = ref;
  lovrGraphicsFlushMaterial(material);
  for (int i = 0; i < MAX_MATERIAL_TEXTURES; i++) {
    lovrRelease(Texture, material->textures[i]);
  }
}

void lovrMaterialBind(Material* material, Shader* shader) {
  for (int i = 0; i < MAX_MATERIAL_SCALARS; i++) {
    lovrShaderSetFloats(shader, lovrShaderScalarUniforms[i], &material->scalars[i], 0, 1);
  }

  for (int i = 0; i < MAX_MATERIAL_COLORS; i++) {
    lovrShaderSetColor(shader, lovrShaderColorUniforms[i], material->colors[i]);
  }

  for (int i = 0; i < MAX_MATERIAL_TEXTURES; i++) {
    lovrShaderSetTextures(shader, lovrShaderTextureUniforms[i], &material->textures[i], 0, 1);
  }

  lovrShaderSetMatrices(shader, "lovrMaterialTransform", material->transform, 0, 9);
}

float lovrMaterialGetScalar(Material* material, MaterialScalar scalarType) {
  return material->scalars[scalarType];
}

void lovrMaterialSetScalar(Material* material, MaterialScalar scalarType, float value) {
  if (material->scalars[scalarType] != value) {
    lovrGraphicsFlushMaterial(material);
    material->scalars[scalarType] = value;
  }
}

Color lovrMaterialGetColor(Material* material, MaterialColor colorType) {
  return material->colors[colorType];
}

void lovrMaterialSetColor(Material* material, MaterialColor colorType, Color color) {
  if (memcmp(&material->colors[colorType], &color, 4 * sizeof(float))) {
    lovrGraphicsFlushMaterial(material);
    material->colors[colorType] = color;
  }
}

Texture* lovrMaterialGetTexture(Material* material, MaterialTexture textureType) {
  return material->textures[textureType];
}

void lovrMaterialSetTexture(Material* material, MaterialTexture textureType, Texture* texture) {
  if (material->textures[textureType] != texture) {
    lovrGraphicsFlushMaterial(material);
    lovrRetain(texture);
    lovrRelease(Texture, material->textures[textureType]);
    material->textures[textureType] = texture;
  }
}

void lovrMaterialGetTransform(Material* material, float* ox, float* oy, float* sx, float* sy, float* angle) {
  *ox = material->transform[6];
  *oy = material->transform[7];
  *sx = sqrtf(material->transform[0] * material->transform[0] + material->transform[1] * material->transform[1]);
  *sy = sqrtf(material->transform[3] * material->transform[3] + material->transform[4] * material->transform[4]);
  *angle = atan2f(-material->transform[3], material->transform[0]);
}

void lovrMaterialSetTransform(Material* material, float ox, float oy, float sx, float sy, float angle) {
  lovrGraphicsFlushMaterial(material);
  float c = cosf(angle);
  float s = sinf(angle);
  material->transform[0] = c * sx;
  material->transform[1] = s * sx;
  material->transform[2] = 0.f;
  material->transform[3] = -s * sy;
  material->transform[4] = c * sy;
  material->transform[5] = 0.f;
  material->transform[6] = ox;
  material->transform[7] = oy;
  material->transform[8] = 1.f;
}
