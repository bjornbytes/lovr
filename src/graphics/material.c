#include "graphics/graphics.h"
#include "graphics/material.h"

Material* lovrMaterialCreate(bool isDefault) {
  Material* material = lovrAlloc(sizeof(Material), lovrMaterialDestroy);
  if (!material) return NULL;

  material->isDefault = isDefault;

  for (int i = 0; i < MAX_MATERIAL_SCALARS; i++) {
    material->scalars[i] = 1.f;
  }

  for (int i = 0; i < MAX_MATERIAL_COLORS; i++) {
    material->colors[i] = (Color) { 1, 1, 1, 1 };
  }

  for (int i = 0; i < MAX_MATERIAL_TEXTURES; i++) {
    material->textures[i] = NULL;
  }

  return material;
}

void lovrMaterialDestroy(void* ref) {
  Material* material = ref;
  for (int i = 0; i < MAX_MATERIAL_TEXTURES; i++) {
    lovrRelease(material->textures[i]);
  }
  free(material);
}

float lovrMaterialGetScalar(Material* material, MaterialScalar scalarType) {
  return material->scalars[scalarType];
}

void lovrMaterialSetScalar(Material* material, MaterialScalar scalarType, float value) {
  material->scalars[scalarType] = value;
}

Color lovrMaterialGetColor(Material* material, MaterialColor colorType) {
  return material->colors[colorType];
}

void lovrMaterialSetColor(Material* material, MaterialColor colorType, Color color) {
  material->colors[colorType] = color;
}

Texture* lovrMaterialGetTexture(Material* material, MaterialTexture textureType) {
  return material->textures[textureType];
}

void lovrMaterialSetTexture(Material* material, MaterialTexture textureType, Texture* texture) {
  if (texture != material->textures[textureType]) {
    lovrRetain(texture);
    lovrRelease(material->textures[textureType]);
    material->textures[textureType] = texture;
  }
}
