#include "model.h"
#include <stdlib.h>

Model* lovrModelCreate(const char* filename) {
  Model* model = malloc(sizeof(model));
  model->modelData = lovrModelDataCreate(filename);
  return model;
}

void lovrModelDestroy(Model* model) {
  lovrModelDataDestroy(model->modelData);
  free(model);
}
