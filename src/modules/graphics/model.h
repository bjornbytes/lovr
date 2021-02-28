#include <stdint.h>
#include <stdbool.h>

#pragma once

struct Material;
struct ModelData;

typedef enum {
  SPACE_LOCAL,
  SPACE_GLOBAL
} CoordinateSpace;

typedef struct Model Model;
Model* lovrModelCreate(struct ModelData* data);
void lovrModelDestroy(void* ref);
struct ModelData* lovrModelGetModelData(Model* model);
void lovrModelDraw(Model* model, float* transform, uint32_t instances);
void lovrModelAnimate(Model* model, uint32_t animationIndex, float time, float alpha);
void lovrModelGetNodePose(Model* model, uint32_t nodeIndex, float position[4], float rotation[4], CoordinateSpace space);
void lovrModelPose(Model* model, uint32_t nodeIndex, float position[4], float rotation[4], float alpha);
void lovrModelResetPose(Model* model);
struct Material* lovrModelGetMaterial(Model* model, uint32_t material);
void lovrModelGetAABB(Model* model, float aabb[6]);
void lovrModelGetTriangles(Model* model, float** vertices, uint32_t* vertexCount, uint32_t** indices, uint32_t* indexCount);
