#include "shader.h"
#include "../util.h"
#include <stdlib.h>

const char* lovrShaderVertexPrefix = ""
"#version 150 \n"
"uniform mat4 lovrTransform; \n"
"uniform mat4 lovrProjection; \n"
"in vec3 position;"
"";

const char* lovrShaderFragmentPrefix = ""
"#version 150 \n"
"uniform vec4 lovrColor; \n"
"out vec4 color;"
"";

const char* lovrDefaultVertexShader = ""
"void main() { \n"
"  gl_Position = lovrProjection * lovrTransform * vec4(position.xyz, 1.0); \n"
"}"
"";

const char* lovrDefaultFragmentShader = ""
"void main() { \n"
"  color = lovrColor; \n"
"}"
"";

GLuint compileShader(GLenum type, const char* source) {
  GLuint shader = glCreateShader(type);

  glShaderSource(shader, 1, (const GLchar**)&source, NULL);
  glCompileShader(shader);

  int isShaderCompiled;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &isShaderCompiled);
  if(!isShaderCompiled) {
    int logLength;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);

    char* log = (char*) malloc(logLength);
    glGetShaderInfoLog(shader, logLength, &logLength, log);
    error(log);
  }

  return shader;
}

GLuint linkShaders(GLuint vertexShader, GLuint fragmentShader) {
  GLuint shader = glCreateProgram();

  if (vertexShader) {
    glAttachShader(shader, vertexShader);
  }

  if (fragmentShader) {
    glAttachShader(shader, fragmentShader);
  }

  glBindAttribLocation(shader, LOVR_SHADER_POSITION, "position");
  glBindAttribLocation(shader, LOVR_SHADER_NORMAL, "normal");

  glLinkProgram(shader);

  int isShaderLinked;
  glGetProgramiv(shader, GL_LINK_STATUS, (int*)&isShaderLinked);
  if(!isShaderLinked) {
    int logLength;
    glGetProgramiv(shader, GL_INFO_LOG_LENGTH, &logLength);

    char* log = (char*) malloc(logLength);
    glGetProgramInfoLog(shader, logLength, &logLength, log);
    error(log);
  }

  glDetachShader(shader, vertexShader);
  glDeleteShader(vertexShader);
  glDetachShader(shader, fragmentShader);
  glDeleteShader(fragmentShader);

  return shader;
}

Shader* lovrShaderCreate(const char* vertexSource, const char* fragmentSource) {
  char fullVertexSource[1024];
  snprintf(fullVertexSource, sizeof(fullVertexSource), "%s\n%s", lovrShaderVertexPrefix, vertexSource);
  GLuint vertexShader = compileShader(GL_VERTEX_SHADER, fullVertexSource);

  char fullFragmentSource[1024];
  snprintf(fullFragmentSource, sizeof(fullFragmentSource), "%s\n%s", lovrShaderFragmentPrefix, fragmentSource);
  GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fullFragmentSource);

  GLuint id = linkShaders(vertexShader, fragmentShader);
  Shader* shader = (Shader*) malloc(sizeof(Shader));
  shader->id = id;
  return shader;
}

void lovrShaderDestroy(Shader* shader) {
  glDeleteProgram(shader->id);
  free(shader);
}

int lovrShaderGetUniformId(Shader* shader, const char* name) {
  return glGetUniformLocation(shader->id, name);
}

void lovrShaderGetUniformType(Shader* shader, int id, GLenum* type, int* size) {
  glGetActiveUniform(shader->id, id, 0, NULL, size, type, NULL);
}

void lovrShaderSendFloat(Shader* shader, int id, float value) {
  glUniform1f(id, value);
}

void lovrShaderSendFloatVec2(Shader* shader, int id, float* vector) {
  glUniform2fv(id, 1, vector);
}

void lovrShaderSendFloatVec3(Shader* shader, int id, float* vector) {
  glUniform3fv(id, 1, vector);
}

void lovrShaderSendFloatVec4(Shader* shader, int id, float* vector) {
  glUniform4fv(id, 1, vector);
}

void lovrShaderSendFloatMat2(Shader* shader, int id, float* matrix) {
  glUniformMatrix2fv(id, 1, GL_FALSE, matrix);
}

void lovrShaderSendFloatMat3(Shader* shader, int id, float* matrix) {
  glUniformMatrix3fv(id, 1, GL_FALSE, matrix);
}

void lovrShaderSendFloatMat4(Shader* shader, int id, float* matrix) {
  glUniformMatrix4fv(id, 1, GL_FALSE, matrix);
}
