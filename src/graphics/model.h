#include "loaders/model.h"
#include "graphics/buffer.h"
#include "graphics/texture.h"
#include "math/math.h"
#include "glfw.h"
#include "util.h"

#pragma once

typedef struct {
  Ref ref;
  ModelData* modelData;
  Buffer* buffer;
  Texture* texture;
} Model;

Model* lovrModelCreate(ModelData* modelData);
void lovrModelDestroy(const Ref* ref);
void lovrModelDraw(Model* model, mat4 transform);
Texture* lovrModelGetTexture(Model* model);
void lovrModelSetTexture(Model* model, Texture* texture);
