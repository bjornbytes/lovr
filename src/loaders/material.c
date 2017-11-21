#include "loaders/material.h"

MaterialData* lovrMaterialDataCreateEmpty() {
  MaterialData* materialData = malloc(sizeof(MaterialData));

  for (int i = 0; i < MAX_MATERIAL_COLORS; i++) {
    materialData->colors[i] = (Color) { 1., 1., 1., 1. };
  }

  for (int i = 0; i < MAX_MATERIAL_TEXTURES; i++) {
    materialData->textures[i] = NULL;
  }

  return materialData;
}

void lovrMaterialDataDestroy(MaterialData* materialData) {
  free(materialData);
}
