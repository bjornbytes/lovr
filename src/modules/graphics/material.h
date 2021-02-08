#include "data/modelData.h"

#pragma once

struct Texture;
struct Shader;

typedef struct Material Material;
Material* lovrMaterialCreate(void);
void lovrMaterialDestroy(void* ref);
void lovrMaterialBind(Material* material, struct Shader* shader);
float lovrMaterialGetScalar(Material* material, MaterialScalar scalarType);
void lovrMaterialSetScalar(Material* material, MaterialScalar scalarType, float value);
Color lovrMaterialGetColor(Material* material, MaterialColor colorType);
void lovrMaterialSetColor(Material* material, MaterialColor colorType, Color color);
struct Texture* lovrMaterialGetTexture(Material* material, MaterialTexture textureType);
void lovrMaterialSetTexture(Material* material, MaterialTexture textureType, struct Texture* texture);
void lovrMaterialGetTransform(Material* material, float* ox, float* oy, float* sx, float* sy, float* angle);
void lovrMaterialSetTransform(Material* material, float ox, float oy, float sx, float sy, float angle);
