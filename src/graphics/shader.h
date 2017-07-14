#include "math/math.h"
#include "lib/map/map.h"
#include "lib/glfw.h"
#include "util.h"

#pragma once

#define LOVR_SHADER_POSITION 0
#define LOVR_SHADER_NORMAL 1
#define LOVR_SHADER_TEX_COORD 2

#define LOVR_MAX_UNIFORM_LENGTH 256

typedef struct {
  GLchar name[LOVR_MAX_UNIFORM_LENGTH];
  int index;
  int location;
  GLenum type;
  int count;
} Uniform;

typedef map_t(Uniform) map_uniform_t;

typedef struct {
  Ref ref;
  int id;
  map_uniform_t uniforms;
  float transform[16];
  float projection[16];
  unsigned int color;
} Shader;

extern const char* lovrShaderVertexPrefix;
extern const char* lovrShaderFragmentPrefix;
extern const char* lovrDefaultVertexShader;
extern const char* lovrDefaultFragmentShader;
extern const char* lovrSkyboxVertexShader;
extern const char* lovrSkyboxFragmentShader;
extern const char* lovrFontFragmentShader;
extern const char* lovrNoopVertexShader;

GLuint compileShader(GLuint type, const char* source);
GLuint linkShaders(GLuint vertexShader, GLuint fragmentShader);

Shader* lovrShaderCreate(const char* vertexSource, const char* fragmentSource);
void lovrShaderDestroy(const Ref* ref);
void lovrShaderBind(Shader* shader, mat4 transform, mat4 projection, unsigned int color, int force);
int lovrShaderGetAttributeId(Shader* shader, const char* name);
int lovrShaderGetUniformId(Shader* shader, const char* name);
int lovrShaderGetUniformType(Shader* shader, const char* name, GLenum* type, int* count);
void lovrShaderSendInt(Shader* shader, int id, int value);
void lovrShaderSendFloat(Shader* shader, int id, float value);
void lovrShaderSendFloatVec2(Shader* shader, int id, int count, float* vector);
void lovrShaderSendFloatVec3(Shader* shader, int id, int count,float* vector);
void lovrShaderSendFloatVec4(Shader* shader, int id, int count, float* vector);
void lovrShaderSendFloatMat2(Shader* shader, int id, float* matrix);
void lovrShaderSendFloatMat3(Shader* shader, int id, float* matrix);
void lovrShaderSendFloatMat4(Shader* shader, int id, float* matrix);
