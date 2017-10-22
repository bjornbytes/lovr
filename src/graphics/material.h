#include "util.h"
#include "graphics/texture.h"
#include "loaders/material.h"

#pragma once

typedef struct {
  Ref ref;
  MaterialData* materialData;
  Texture* textures[MAX_MATERIAL_TEXTURES];
  int isDefault;
} Material;

Material* lovrMaterialCreate(MaterialData* materialData, int isDefault);
void lovrMaterialDestroy(const Ref* ref);
Color lovrMaterialGetColor(Material* material, MaterialColor colorType);
void lovrMaterialSetColor(Material* material, MaterialColor colorType, Color color);
Texture* lovrMaterialGetTexture(Material* material, MaterialTexture textureType);
void lovrMaterialSetTexture(Material* material, MaterialTexture textureType, Texture* texture);
