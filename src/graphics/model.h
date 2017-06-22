#include "loaders/model.h"
#include "graphics/mesh.h"
#include "graphics/texture.h"
#include "math/math.h"
#include "lib/glfw.h"
#include "util.h"

#pragma once

typedef struct {
  Ref ref;
  ModelData* modelData;
  Mesh* mesh;
  Texture* texture;
  float aabb[6];
} Model;

Model* lovrModelCreate(ModelData* modelData);
void lovrModelDestroy(const Ref* ref);
void lovrModelDraw(Model* model, mat4 transform);
Texture* lovrModelGetTexture(Model* model);
void lovrModelSetTexture(Model* model, Texture* texture);
float* lovrModelGetAABB(Model* model);
