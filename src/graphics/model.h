#include "data/modelData.h"
#include "graphics/mesh.h"
#include "lib/math.h"
#include "util.h"

#pragma once

typedef struct {
  Ref ref;
  ModelData* data;
  Buffer** buffers;
  Mesh** meshes;
} Model;

Model* lovrModelInit(Model* model, ModelData* data);
#define lovrModelCreate(...) lovrModelInit(lovrAlloc(Model), __VA_ARGS__)
void lovrModelDestroy(void* ref);
void lovrModelDraw(Model* model, mat4 transform, int instances);
