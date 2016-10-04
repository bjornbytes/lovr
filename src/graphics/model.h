#include "../model/modelData.h"

#ifndef LOVR_MODEL_TYPES
#define LOVR_MODEL_TYPES
typedef struct {
  ModelData* modelData;
} Model;
#endif

Model* lovrModelCreate(const char* filename);
void lovrModelDestroy(Model* model);
