#include "graphics/texture.h"
#include "graphics/shader.h"
#include "util.h"
#include <stdbool.h>

#pragma once

typedef enum {
  SCALAR_METALNESS,
  SCALAR_ROUGHNESS,
  MAX_MATERIAL_SCALARS
} MaterialScalar;

typedef enum {
  COLOR_DIFFUSE,
  COLOR_EMISSIVE,
  MAX_MATERIAL_COLORS
} MaterialColor;

typedef enum {
  TEXTURE_DIFFUSE,
  TEXTURE_EMISSIVE,
  TEXTURE_METALNESS,
  TEXTURE_ROUGHNESS,
  TEXTURE_OCCLUSION,
  TEXTURE_NORMAL,
  TEXTURE_ENVIRONMENT_MAP,
  MAX_MATERIAL_TEXTURES
} MaterialTexture;

typedef struct {
  Ref ref;
  float scalars[MAX_MATERIAL_SCALARS];
  Color colors[MAX_MATERIAL_COLORS];
  Texture* textures[MAX_MATERIAL_TEXTURES];
  float transform[9];
} Material;

Material* lovrMaterialInit(Material* material);
#define lovrMaterialCreate() lovrMaterialInit(lovrAlloc(Material))
void lovrMaterialDestroy(void* ref);
void lovrMaterialBind(Material* material, Shader* shader);
float lovrMaterialGetScalar(Material* material, MaterialScalar scalarType);
void lovrMaterialSetScalar(Material* material, MaterialScalar scalarType, float value);
Color lovrMaterialGetColor(Material* material, MaterialColor colorType);
void lovrMaterialSetColor(Material* material, MaterialColor colorType, Color color);
Texture* lovrMaterialGetTexture(Material* material, MaterialTexture textureType);
void lovrMaterialSetTexture(Material* material, MaterialTexture textureType, Texture* texture);
void lovrMaterialGetTransform(Material* material, float* ox, float* oy, float* sx, float* sy, float* angle);
void lovrMaterialSetTransform(Material* material, float ox, float oy, float sx, float sy, float angle);
