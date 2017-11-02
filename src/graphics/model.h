#include "loaders/model.h"
#include "graphics/material.h"
#include "graphics/mesh.h"
#include "graphics/texture.h"
#include "math/math.h"
#include "lib/glfw.h"
#include "util.h"
#include <stdbool.h>

#pragma once

typedef struct {
  Ref ref;
  ModelData* modelData;
  Mesh* mesh;
  Material** materials;
  float aabb[6];
  bool aabbDirty;
} Model;

Model* lovrModelCreate(ModelData* modelData);
void lovrModelDestroy(const Ref* ref);
void lovrModelDraw(Model* model, mat4 transform);
Mesh* lovrModelGetMesh(Model* model);
const float* lovrModelGetAABB(Model* model);
