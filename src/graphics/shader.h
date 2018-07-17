#include "graphics/texture.h"
#include <stdbool.h>

#pragma once

typedef enum {
  UNIFORM_FLOAT,
  UNIFORM_MATRIX,
  UNIFORM_INT,
  UNIFORM_SAMPLER
} UniformType;

typedef enum {
  SHADER_DEFAULT,
  SHADER_CUBE,
  SHADER_PANO,
  SHADER_FONT,
  SHADER_FILL,
  MAX_DEFAULT_SHADERS
} DefaultShader;

typedef struct Shader Shader;

Shader* lovrShaderCreate(const char* vertexSource, const char* fragmentSource);
Shader* lovrShaderCreateDefault(DefaultShader type);
void lovrShaderDestroy(void* ref);
uint32_t lovrShaderGetProgram(Shader* shader);
void lovrShaderBind(Shader* shader);
int lovrShaderGetAttributeId(Shader* shader, const char* name);
bool lovrShaderHasUniform(Shader* shader, const char* name);
bool lovrShaderGetUniform(Shader* shader, const char* name, int* count, int* components, size_t* size, UniformType* type);
void lovrShaderSetFloat(Shader* shader, const char* name, float* data, int count);
void lovrShaderSetInt(Shader* shader, const char* name, int* data, int count);
void lovrShaderSetMatrix(Shader* shader, const char* name, float* data, int count);
void lovrShaderSetTexture(Shader* shader, const char* name, Texture** data, int count);
