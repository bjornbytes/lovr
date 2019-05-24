#include <stdint.h>
#include <stdbool.h>

#pragma once

struct Material;
struct ModelData;

typedef struct Model Model;
Model* lovrModelCreate(struct ModelData* data);
void lovrModelDestroy(void* ref);
struct ModelData* lovrModelGetModelData(Model* model);
void lovrModelDraw(Model* model, float* transform, uint32_t instances);
void lovrModelAnimate(Model* model, uint32_t animationIndex, float time, float alpha);
struct Material* lovrModelGetMaterial(Model* model);
void lovrModelSetMaterial(Model* model, struct Material* material);
void lovrModelGetAABB(Model* model, float aabb[6]);
