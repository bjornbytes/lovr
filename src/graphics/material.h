#include "util.h"
#include "graphics/texture.h"
#include <stdbool.h>

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
  Ref ref;
  Color colors[MAX_MATERIAL_COLORS];
  Texture* textures[MAX_MATERIAL_TEXTURES];
  bool isDefault;
} Material;

Material* lovrMaterialCreate(bool isDefault);
void lovrMaterialDestroy(const Ref* ref);
Color lovrMaterialGetColor(Material* material, MaterialColor colorType);
void lovrMaterialSetColor(Material* material, MaterialColor colorType, Color color);
Texture* lovrMaterialGetTexture(Material* material, MaterialTexture textureType);
void lovrMaterialSetTexture(Material* material, MaterialTexture textureType, Texture* texture);
