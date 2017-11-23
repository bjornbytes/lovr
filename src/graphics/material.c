#include "graphics/graphics.h"
#include "graphics/material.h"

Material* lovrMaterialCreate(MaterialData* materialData, bool isDefault) {
  Material* material = lovrAlloc(sizeof(Material), lovrMaterialDestroy);
  if (!material) return NULL;

  material->materialData = materialData;
  material->isDefault = isDefault;

  for (int i = 0; i < MAX_MATERIAL_TEXTURES; i++) {
    if (materialData->textures[i]) {
      material->textures[i] = lovrTextureCreate(TEXTURE_2D, &materialData->textures[i], 1, true);
    } else {
      material->textures[i] = NULL;
    }
  }

  return material;
}

void lovrMaterialDestroy(const Ref* ref) {
  Material* material = containerof(ref, Material);
  for (int i = 0; i < MAX_MATERIAL_TEXTURES; i++) {
    if (material->textures[i]) {
      lovrRelease(&material->textures[i]->ref);
    }
  }
  free(material);
}

Color lovrMaterialGetColor(Material* material, MaterialColor colorType) {
  return material->materialData->colors[colorType];
}

void lovrMaterialSetColor(Material* material, MaterialColor colorType, Color color) {
  material->materialData->colors[colorType] = color;
}

Texture* lovrMaterialGetTexture(Material* material, MaterialTexture textureType) {
  return material->textures[textureType];
}

void lovrMaterialSetTexture(Material* material, MaterialTexture textureType, Texture* texture) {
  if (texture != material->textures[textureType]) {
    if (material->textures[textureType]) {
      lovrRelease(&material->textures[textureType]->ref);
    }

    material->textures[textureType] = texture;

    if (texture) {
      lovrRetain(&texture->ref);
    }
  }
}
