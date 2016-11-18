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
"  gl_Position = lovrProjection * lovrTransform * vec4(position, 1.0); \n"
"}"
"";

const char* lovrDefaultFragmentShader = ""
"void main() { \n"
"  color = lovrColor; \n"
"}"
"";

const char* lovrSkyboxVertexShader = ""
"out vec3 texturePosition; \n"
"void main() { \n"
"  texturePosition = position; \n"
"  gl_Position = lovrProjection * lovrTransform * vec4(position, 1.0); \n"
"}"
"";

const char* lovrSkyboxFragmentShader = ""
"in vec3 texturePosition; \n"
"uniform samplerCube cube; \n"
"void main() { \n"
"  color = texture(cube, texturePosition) * lovrColor; \n"
"}"
"";

GLuint compileShader(GLenum type, const char* source) {
  GLuint shader = glCreateShader(type);

  glShaderSource(shader, 1, (const GLchar**)&source, NULL);
  glCompileShader(shader);

  int isShaderCompiled;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &isShaderCompiled);
  if (!isShaderCompiled) {
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
  if (!isShaderLinked) {
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
  Shader* shader = (Shader*) malloc(sizeof(Shader));
  if (!shader) return NULL;

  // Vertex
  vertexSource = vertexSource == NULL ? lovrDefaultVertexShader : vertexSource;
  char fullVertexSource[1024];
  snprintf(fullVertexSource, sizeof(fullVertexSource), "%s\n%s", lovrShaderVertexPrefix, vertexSource);
  GLuint vertexShader = compileShader(GL_VERTEX_SHADER, fullVertexSource);

  // Fragment
  fragmentSource = fragmentSource == NULL ? lovrDefaultFragmentShader : fragmentSource;
  char fullFragmentSource[1024];
  snprintf(fullFragmentSource, sizeof(fullFragmentSource), "%s\n%s", lovrShaderFragmentPrefix, fragmentSource);
  GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fullFragmentSource);

  // Link
  GLuint id = linkShaders(vertexShader, fragmentShader);

  // Compute information about uniforms
  GLint uniformCount;
  GLsizei bufferSize = LOVR_MAX_UNIFORM_LENGTH / sizeof(GLchar);
  map_init(&shader->uniforms);
  glGetProgramiv(id, GL_ACTIVE_UNIFORMS, &uniformCount);
  for (int i = 0; i < uniformCount; i++) {
    Uniform uniform;
    glGetActiveUniform(id, i, bufferSize, NULL, &uniform.count, &uniform.type, uniform.name);
    uniform.location = glGetUniformLocation(id, uniform.name);
    uniform.index = i;
    map_set(&shader->uniforms, uniform.name, uniform);
  }

  // Initial state
  shader->id = id;
  shader->transform = mat4_init();
  shader->projection = mat4_init();
  shader->color = 0;

  // Send initial uniform values to shader
  lovrShaderBind(shader, shader->transform, shader->projection, shader->color, 1);

  return shader;
}

void lovrShaderDestroy(Shader* shader) {
  glDeleteProgram(shader->id);
  mat4_deinit(shader->transform);
  mat4_deinit(shader->projection);
  map_deinit(&shader->uniforms);
  free(shader);
}

void lovrShaderBind(Shader* shader, mat4 transform, mat4 projection, unsigned int color, int force) {

  // Bind shader if necessary
  int program;
  glGetIntegerv(GL_CURRENT_PROGRAM, &program);
  if (program != shader->id) {
    glUseProgram(shader->id);
  }

  // Update transform if necessary
  if (force || memcmp(shader->transform, transform, 16 * sizeof(float))) {
    int uniformId = lovrShaderGetUniformId(shader, "lovrTransform");
    lovrShaderSendFloatMat4(shader, uniformId, transform);
    memcpy(shader->transform, transform, 16 * sizeof(float));
  }

  // Update projection if necessary
  if (force || memcmp(shader->projection, projection, 16 * sizeof(float))) {
    int uniformId = lovrShaderGetUniformId(shader, "lovrProjection");
    lovrShaderSendFloatMat4(shader, uniformId, projection);
    memcpy(shader->projection, projection, 16 * sizeof(float));
  }

  // Update color if necessary
  if (force || shader->color != color) {
    int uniformId = lovrShaderGetUniformId(shader, "lovrColor");
    float c[4] = {
      LOVR_COLOR_R(color) / 255.f,
      LOVR_COLOR_G(color) / 255.f,
      LOVR_COLOR_B(color) / 255.f,
      LOVR_COLOR_A(color) / 255.f
    };
    lovrShaderSendFloatVec4(shader, uniformId, c);
    shader->color = color;
  }
}

int lovrShaderGetAttributeId(Shader* shader, const char* name) {
  if (!shader) {
    return -1;
  }

  return glGetAttribLocation(shader->id, name);
}

int lovrShaderGetUniformId(Shader* shader, const char* name) {
  Uniform* uniform = map_get(&shader->uniforms, name);
  return uniform ? uniform->location : -1;
}

int lovrShaderGetUniformType(Shader* shader, const char* name, GLenum* type, int* count) {
  Uniform* uniform = map_get(&shader->uniforms, name);

  if (!uniform) {
    return 1;
  }

  *type = uniform->type;
  *count = uniform->count;
  return 0;
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
