#ifndef LOVR_MODEL_DATA_TYPES
#define LOVR_MODEL_DATA_TYPES
typedef struct {
  unsigned int vertexCount;
  float* data;
} ModelData;
#endif

ModelData* lovrModelDataCreate(const char* filename);
void lovrModelDataDestroy(ModelData* modelData);
