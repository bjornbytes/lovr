#include "glfw.h"
#include "matrix.h"
#include "vendor/map/map.h"
#include "util.h"

#ifndef LOVR_SHADER_TYPES
#define LOVR_SHADER_TYPES

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
  mat4 transform;
  mat4 projection;
  unsigned int color;
} Shader;
#endif

extern const char* lovrShaderVertexPrefix;
extern const char* lovrShaderFragmentPrefix;
extern const char* lovrDefaultVertexShader;
extern const char* lovrDefaultFragmentShader;
extern const char* lovrSkyboxVertexShader;
extern const char* lovrSkyboxFragmentShader;

GLuint compileShader(GLuint type, const char* filename);
GLuint linkShaders(GLuint vertexShader, GLuint fragmentShader);

Shader* lovrShaderCreate(const char* vertexSource, const char* fragmentSource);
void lovrShaderDestroy(const Ref* ref);
void lovrShaderBind(Shader* shader, mat4 transform, mat4 projection, unsigned int color, int force);
int lovrShaderGetAttributeId(Shader* shader, const char* name);
int lovrShaderGetUniformId(Shader* shader, const char* name);
int lovrShaderGetUniformType(Shader* shader, const char* name, GLenum* type, int* count);
void lovrShaderSendInt(Shader* shader, int id, int value);
void lovrShaderSendFloat(Shader* shader, int id, float value);
void lovrShaderSendFloatVec2(Shader* shader, int id, float* vector);
void lovrShaderSendFloatVec3(Shader* shader, int id, float* vector);
void lovrShaderSendFloatVec4(Shader* shader, int id, float* vector);
void lovrShaderSendFloatMat2(Shader* shader, int id, float* matrix);
void lovrShaderSendFloatMat3(Shader* shader, int id, float* matrix);
void lovrShaderSendFloatMat4(Shader* shader, int id, float* matrix);
