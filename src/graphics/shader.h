#include "graphics/texture.h"
#include "math/math.h"
#include "lib/map/map.h"
#include "lib/glfw.h"
#include "util.h"
#include <stdbool.h>

#pragma once

#define LOVR_SHADER_POSITION 0
#define LOVR_SHADER_NORMAL 1
#define LOVR_SHADER_TEX_COORD 2
#define LOVR_SHADER_VERTEX_COLOR 3
#define LOVR_SHADER_TANGENT 4
#define LOVR_SHADER_BONES 5
#define LOVR_SHADER_BONE_WEIGHTS 6
#define LOVR_MAX_UNIFORM_LENGTH 256

typedef enum {
  UNIFORM_FLOAT,
  UNIFORM_MATRIX,
  UNIFORM_INT,
  UNIFORM_SAMPLER
} UniformType;

typedef union {
  void* data;
  int* ints;
  float* floats;
  Texture** textures;
} UniformValue;

typedef enum {
  SHADER_DEFAULT,
  SHADER_SKYBOX,
  SHADER_FONT,
  SHADER_FULLSCREEN
} DefaultShader;

typedef struct {
  GLchar name[LOVR_MAX_UNIFORM_LENGTH];
  GLenum glType;
  int index;
  int location;
  int count;
  int components;
  size_t size;
  UniformType type;
  UniformValue value;
  int baseTextureSlot;
  bool dirty;
} Uniform;

typedef map_t(Uniform) map_uniform_t;

typedef struct {
  Ref ref;
  uint32_t program;
  map_uniform_t uniforms;
  float model[16];
  float view[16];
  float projection[16];
  Color color;
} Shader;

Shader* lovrShaderCreate(const char* vertexSource, const char* fragmentSource);
Shader* lovrShaderCreateDefault(DefaultShader type);
void lovrShaderDestroy(void* ref);
void lovrShaderBind(Shader* shader);
int lovrShaderGetAttributeId(Shader* shader, const char* name);
Uniform* lovrShaderGetUniform(Shader* shader, const char* name);
void lovrShaderSetFloat(Shader* shader, const char* name, float* data, int count);
void lovrShaderSetInt(Shader* shader, const char* name, int* data, int count);
void lovrShaderSetMatrix(Shader* shader, const char* name, float* data, int count);
void lovrShaderSetTexture(Shader* shader, const char* name, Texture** data, int count);
