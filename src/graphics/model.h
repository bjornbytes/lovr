#include "buffer.h"
#include "../model/modelData.h"
#include "../glfw.h"

#ifndef LOVR_MODEL_TYPES
#define LOVR_MODEL_TYPES
typedef struct {
  ModelData* modelData;
  Buffer* buffer;
} Model;
#endif

Model* lovrModelCreate(const char* filename);
void lovrModelDestroy(Model* model);
void lovrModelDraw(Model* model, float x, float y, float z, float scale, float angle, float ax, float ay, float az);
