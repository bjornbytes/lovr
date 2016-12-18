#include "graphics/shader.h"
#include "util.h"
#include <stdlib.h>

const char* lovrShaderVertexPrefix = ""
#ifdef EMSCRIPTEN
"#version 100 \n"
"precision mediump float; \n"
#else
"#version 120 \n"
#endif
"uniform mat4 lovrTransform; \n"
"uniform mat4 lovrProjection; \n"
"attribute vec3 lovrPosition; \n"
"attribute vec3 lovrNormal; \n"
"attribute vec2 lovrTexCoord; \n"
"varying vec2 texCoord; \n"
"";

const char* lovrShaderFragmentPrefix = ""
#ifdef EMSCRIPTEN
"#version 100 \n"
"precision mediump float; \n"
#else
"#version 120 \n"
#endif
"uniform vec4 lovrColor; \n"
"uniform sampler2D lovrTexture; \n"
"varying vec2 texCoord; \n"
"";

const char* lovrShaderVertexSuffix = ""
"void main() { \n"
"  texCoord = lovrTexCoord; \n"
"  gl_Position = position(lovrProjection, lovrTransform, vec4(lovrPosition, 1.0)); \n"
"}"
"";

const char* lovrShaderFragmentSuffix = ""
"void main() { \n"
"  gl_FragColor = color(lovrColor, lovrTexture, texCoord); \n"
"}"
"";

const char* lovrDefaultVertexShader = ""
"vec4 position(mat4 projection, mat4 transform, vec4 vertex) { \n"
"  return projection * transform * vertex; \n"
"}"
"";

const char* lovrDefaultFragmentShader = ""
"vec4 color(vec4 graphicsColor, sampler2D image, vec2 uv) { \n"
"  return graphicsColor * texture2D(image, uv); \n"
"}"
"";

const char* lovrSkyboxVertexShader = ""
"varying vec3 texturePosition; \n"
"vec4 position(mat4 projection, mat4 transform, vec4 vertex) { \n"
"  texturePosition = vertex.xyz; \n"
"  return projection * transform * vertex; \n"
"}"
"";

const char* lovrSkyboxFragmentShader = ""
"varying vec3 texturePosition; \n"
"uniform samplerCube cube; \n"
"vec4 color(vec4 graphicsColor, sampler2D image, vec2 uv) { \n"
"  return graphicsColor * textureCube(cube, texturePosition); \n"
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

  glBindAttribLocation(shader, LOVR_SHADER_POSITION, "lovrPosition");
  glBindAttribLocation(shader, LOVR_SHADER_NORMAL, "lovrNormal");
  glBindAttribLocation(shader, LOVR_SHADER_TEX_COORD, "lovrTexCoord");

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
  Shader* shader = lovrAlloc(sizeof(Shader), lovrShaderDestroy);
  if (!shader) return NULL;

  // Vertex
  vertexSource = vertexSource == NULL ? lovrDefaultVertexShader : vertexSource;
  char fullVertexSource[4096];
  snprintf(fullVertexSource, sizeof(fullVertexSource), "%s\n%s\n%s", lovrShaderVertexPrefix, vertexSource, lovrShaderVertexSuffix);
  GLuint vertexShader = compileShader(GL_VERTEX_SHADER, fullVertexSource);

  // Fragment
  fragmentSource = fragmentSource == NULL ? lovrDefaultFragmentShader : fragmentSource;
  char fullFragmentSource[4096];
  snprintf(fullFragmentSource, sizeof(fullFragmentSource), "%s\n%s\n%s", lovrShaderFragmentPrefix, fragmentSource, lovrShaderFragmentSuffix);
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

void lovrShaderDestroy(const Ref* ref) {
  Shader* shader = containerof(ref, Shader);
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

void lovrShaderSendInt(Shader* shader, int id, int value) {
  glUniform1i(id, value);
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
