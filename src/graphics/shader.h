#include "../glfw.h"

#ifndef LOVR_SHADER_TYPES
#define LOVR_SHADER_TYPES
typedef struct {
  GLuint id;
} Shader;
#endif

GLuint compileShader(GLuint type, const char* filename);
GLuint linkShaders(GLuint vertexShader, GLuint fragmentShader);

void lovrShaderDestroy(Shader* shader);
