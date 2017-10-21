#include "loaders/texture.h"

#pragma once

typedef enum {
  COLOR_DIFFUSE,
  MAX_MATERIAL_COLORS
} MaterialColor;

typedef enum {
  TEXTURE_DIFFUSE,
  TEXTURE_ENVIRONMENT_MAP,
  MAX_MATERIAL_TEXTURES
} MaterialTexture;

typedef struct {
  Color colors[MAX_MATERIAL_COLORS];
  TextureData* textures[MAX_MATERIAL_TEXTURES];
} MaterialData;

MaterialData* lovrMaterialDataCreateEmpty();
void lovrMaterialDataDestroy(MaterialData* materialData);
