#include "../glfw.h"

#ifndef LOVR_SHADER_TYPES
#define LOVR_SHADER_TYPES

#define LOVR_SHADER_POSITION 0
#define LOVR_SHADER_NORMAL 1

typedef struct {
  GLuint id;
} Shader;
#endif

extern const char* lovrShaderVertexPrefix;
extern const char* lovrShaderFragmentPrefix;
extern const char* lovrDefaultVertexShader;
extern const char* lovrDefaultFragmentShader;

GLuint compileShader(GLuint type, const char* filename);
GLuint linkShaders(GLuint vertexShader, GLuint fragmentShader);

Shader* lovrShaderCreate(const char* vertexSource, const char* fragmentSource);
void lovrShaderDestroy(Shader* shader);
int lovrShaderGetUniformId(Shader* shader, const char* name);
void lovrShaderGetUniformType(Shader* shader, int id, GLenum* type, int* size);
void lovrShaderSendFloat(Shader* shader, int id, float value);
void lovrShaderSendFloatVec2(Shader* shader, int id, float* vector);
void lovrShaderSendFloatVec3(Shader* shader, int id, float* vector);
void lovrShaderSendFloatVec4(Shader* shader, int id, float* vector);
void lovrShaderSendFloatMat2(Shader* shader, int id, float* matrix);
void lovrShaderSendFloatMat3(Shader* shader, int id, float* matrix);
void lovrShaderSendFloatMat4(Shader* shader, int id, float* matrix);
