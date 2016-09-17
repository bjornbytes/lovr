#include "buffer.h"
#include "model.h"
#include "shader.h"

void lovrGraphicsInit();
void lovrGraphicsClear(int color, int depth);
void lovrGraphicsPresent();
void lovrGraphicsGetClearColor(float* r, float* g, float* b, float* a);
void lovrGraphicsSetClearColor(float r, float g, float b, float a);
Shader* lovrGraphicsGetShader();
void lovrGraphicsSetShader(Shader* shader);
Buffer* lovrGraphicsNewBuffer(int size, BufferDrawMode drawMode, BufferUsage usage);
Model* lovrGraphicsNewModel(const char* path);
Shader* lovrGraphicsNewShader(const char* vertexSource, const char* fragmentSource);
