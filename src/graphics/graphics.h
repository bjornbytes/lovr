#include "buffer.h"
#include "model.h"
#include "shader.h"
#include "../matrix.h"

#ifndef LOVR_GRAPHICS_TYPES
#define LOVR_GRAPHICS_TYPES
typedef vec_t(mat4) vec_mat4_t;
#endif

void lovrGraphicsInit();
void lovrGraphicsClear(int color, int depth);
void lovrGraphicsPresent();
void lovrGraphicsPrepare();
void lovrGraphicsGetClearColor(float* r, float* g, float* b, float* a);
void lovrGraphicsSetClearColor(float r, float g, float b, float a);
Shader* lovrGraphicsGetShader();
void lovrGraphicsSetShader(Shader* shader);
int lovrGraphicsPush();
int lovrGraphicsPop();
void lovrGraphicsOrigin();
void lovrGraphicsTranslate(float x, float y, float z);
void lovrGraphicsRotate(float w, float x, float y, float z);
void lovrGraphicsScale(float x, float y, float z);
Buffer* lovrGraphicsNewBuffer(int size, BufferDrawMode drawMode, BufferUsage usage);
Model* lovrGraphicsNewModel(const char* path);
Shader* lovrGraphicsNewShader(const char* vertexSource, const char* fragmentSource);
