#include "buffer.h"
#include "model.h"
#include "shader.h"

void lovrGraphicsInit();
void lovrGraphicsClear();
void lovrGraphicsPresent();
void lovrGraphicsGetClearColor(float* r, float* g, float* b, float* a);
void lovrGraphicsSetClearColor(float r, float g, float b, float a);
void lovrGraphicsSetShader(Shader* shader);
Buffer* lovrGraphicsNewBuffer(int size);
Model* lovrGraphicsNewModel(const char* path);
Shader* lovrGraphicsNewShader(const char* vertexSource, const char* fragmentSource);
