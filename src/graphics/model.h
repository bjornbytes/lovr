#include "graphics/buffer.h"
#include "graphics/texture.h"
#include "model/modelData.h"
#include "glfw.h"
#include "util.h"

#ifndef LOVR_MODEL_TYPES
#define LOVR_MODEL_TYPES
typedef struct {
  Ref ref;
  ModelData* modelData;
  Buffer* buffer;
  Texture* texture;
} Model;
#endif

Model* lovrModelCreate(ModelData* modelData);
void lovrModelDestroy(const Ref* ref);
void lovrModelDraw(Model* model, float x, float y, float z, float scale, float angle, float ax, float ay, float az);
Texture* lovrModelGetTexture(Model* model);
void lovrModelSetTexture(Model* model, Texture* texture);
