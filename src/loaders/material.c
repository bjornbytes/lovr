#include "loaders/material.h"

MaterialData* lovrMaterialDataCreateEmpty() {
  MaterialData* materialData = malloc(sizeof(MaterialData));

  for (int i = 0; i < MAX_MATERIAL_COLORS; i++) {
    materialData->colors[i] = (Color) { 0xff, 0xff, 0xff, 0xff };
  }

  for (int i = 0; i < MAX_MATERIAL_TEXTURES; i++) {
    materialData->textures[i] = NULL;
  }

  return materialData;
}

void lovrMaterialDataDestroy(MaterialData* materialData) {
  free(materialData);
}
