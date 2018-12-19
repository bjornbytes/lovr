#include "graphics/material.h"
#include "resources/shaders.h"
#include <math.h>
#include <stdlib.h>

Material* lovrMaterialInit(Material* material) {
  for (int i = 0; i < MAX_MATERIAL_SCALARS; i++) {
    material->scalars[i] = 1.f;
  }

  for (int i = 0; i < MAX_MATERIAL_COLORS; i++) {
    if (i == COLOR_EMISSIVE) {
      material->colors[i] = (Color) { 0, 0, 0, 0 };
    } else {
      material->colors[i] = (Color) { 1, 1, 1, 1 };
    }
  }

  lovrMaterialSetTransform(material, 0, 0, 1, 1, 0);
  return material;
}

void lovrMaterialDestroy(void* ref) {
  Material* material = ref;
  for (int i = 0; i < MAX_MATERIAL_TEXTURES; i++) {
    lovrRelease(material->textures[i]);
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

  material->dirty = false;
}

bool lovrMaterialIsDirty(Material* material) {
  return material->dirty;
}

float lovrMaterialGetScalar(Material* material, MaterialScalar scalarType) {
  return material->scalars[scalarType];
}

void lovrMaterialSetScalar(Material* material, MaterialScalar scalarType, float value) {
  if (material->scalars[scalarType] != value) {
    material->scalars[scalarType] = value;
    material->dirty = true;
  }
}

Color lovrMaterialGetColor(Material* material, MaterialColor colorType) {
  return material->colors[colorType];
}

void lovrMaterialSetColor(Material* material, MaterialColor colorType, Color color) {
  if (memcmp(&material->colors[colorType], &color, 4 * sizeof(float))) {
    material->colors[colorType] = color;
    material->dirty = true;
  }
}

Texture* lovrMaterialGetTexture(Material* material, MaterialTexture textureType) {
  return material->textures[textureType];
}

void lovrMaterialSetTexture(Material* material, MaterialTexture textureType, Texture* texture) {
  if (material->textures[textureType] != texture) {
    lovrRetain(texture);
    lovrRelease(material->textures[textureType]);
    material->textures[textureType] = texture;
    material->dirty = true;
  }
}

void lovrMaterialGetTransform(Material* material, float* ox, float* oy, float* sx, float* sy, float* angle) {
  *ox = material->transform[6];
  *oy = material->transform[7];
  *sx = sqrt(material->transform[0] * material->transform[0] + material->transform[1] * material->transform[1]);
  *sy = sqrt(material->transform[3] * material->transform[3] + material->transform[4] * material->transform[4]);
  *angle = atan2(-material->transform[3], material->transform[0]);
}

void lovrMaterialSetTransform(Material* material, float ox, float oy, float sx, float sy, float angle) {
  float c = cos(angle);
  float s = sin(angle);
  material->transform[0] = c * sx;
  material->transform[1] = s * sx;
  material->transform[2] = 0;
  material->transform[3] = -s * sy;
  material->transform[4] = c * sy;
  material->transform[5] = 0;
  material->transform[6] = ox;
  material->transform[7] = oy;
  material->transform[8] = 1;
  material->dirty = true;
}
