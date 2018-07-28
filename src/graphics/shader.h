#include "graphics/texture.h"
#include "lib/map/map.h"
#include "lib/vec/vec.h"
#include <stdbool.h>

#pragma once

#define LOVR_MAX_UNIFORM_LENGTH 64
#define LOVR_MAX_ATTRIBUTE_LENGTH 64
#define MAX_SHADER_BLOCK_UNIFORMS 32

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

typedef struct {
  char name[LOVR_MAX_UNIFORM_LENGTH];
  UniformType type;
  int components;
  int count;
  int location;
  int offset;
  int size;
  union {
    void* data;
    int* ints;
    float* floats;
    Texture** textures;
  } value;
  int baseTextureSlot;
  bool dirty;
} Uniform;

typedef vec_t(Uniform) vec_uniform_t;

typedef struct Shader Shader;
typedef struct ShaderBlock ShaderBlock;

Shader* lovrShaderCreate(const char* vertexSource, const char* fragmentSource);
Shader* lovrShaderCreateDefault(DefaultShader type);
void lovrShaderDestroy(void* ref);
void lovrShaderBind(Shader* shader);
int lovrShaderGetAttributeId(Shader* shader, const char* name);
bool lovrShaderHasUniform(Shader* shader, const char* name);
bool lovrShaderGetUniform(Shader* shader, const char* name, int* count, int* components, int* size, UniformType* type);
void lovrShaderSetFloat(Shader* shader, const char* name, float* data, int count);
void lovrShaderSetInt(Shader* shader, const char* name, int* data, int count);
void lovrShaderSetMatrix(Shader* shader, const char* name, float* data, int count);
void lovrShaderSetTexture(Shader* shader, const char* name, Texture** data, int count);
ShaderBlock* lovrShaderGetBlock(Shader* shader, const char* name);
void lovrShaderSetBlock(Shader* shader, const char* name, ShaderBlock* block);

ShaderBlock* lovrShaderBlockCreate(vec_uniform_t* uniforms);
void lovrShaderBlockDestroy(void* ref);
