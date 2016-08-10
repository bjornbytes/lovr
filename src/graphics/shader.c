#include "shader.h"
#include "../util.h"
#include <stdlib.h>

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

void lovrShaderDestroy(Shader* shader) {
  glDeleteProgram(shader->id);
  free(shader);
}
