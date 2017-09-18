#include "graphics/shader.h"
#include "graphics/graphics.h"
#include "math/mat4.h"
#include <stdio.h>
#include <stdlib.h>

static const char* lovrShaderVertexPrefix = ""
#ifdef EMSCRIPTEN
"#version 300 es \n"
"precision mediump float; \n"
#else
"#version 150 \n"
#endif
"in vec3 lovrPosition; \n"
"in vec3 lovrNormal; \n"
"in vec2 lovrTexCoord; \n"
"out vec2 texCoord; \n"
"uniform mat4 lovrModel; \n"
"uniform mat4 lovrView; \n"
"uniform mat4 lovrProjection; \n"
"uniform mat4 lovrTransform; \n"
"uniform mat3 lovrNormalMatrix; \n";

static const char* lovrShaderFragmentPrefix = ""
#ifdef EMSCRIPTEN
"#version 300 es \n"
"precision mediump float; \n"
#else
"#version 150 \n"
"in vec4 gl_FragCoord; \n"
#endif
"in vec2 texCoord; \n"
"out vec4 lovrFragColor; \n"
"uniform vec4 lovrColor; \n"
"uniform sampler2D lovrTexture; \n";

static const char* lovrShaderVertexSuffix = ""
"void main() { \n"
"  texCoord = lovrTexCoord; \n"
"  gl_Position = position(lovrProjection, lovrTransform, vec4(lovrPosition, 1.0)); \n"
"}";

static const char* lovrShaderFragmentSuffix = ""
"void main() { \n"
"  lovrFragColor = color(lovrColor, lovrTexture, texCoord); \n"
"}";

static const char* lovrDefaultVertexShader = ""
"vec4 position(mat4 projection, mat4 transform, vec4 vertex) { \n"
"  return projection * transform * vertex; \n"
"}";

static const char* lovrDefaultFragmentShader = ""
"vec4 color(vec4 graphicsColor, sampler2D image, vec2 uv) { \n"
"  return graphicsColor * texture(image, uv); \n"
"}";

static const char* lovrSkyboxVertexShader = ""
"out vec3 texturePosition; \n"
"vec4 position(mat4 projection, mat4 transform, vec4 vertex) { \n"
"  texturePosition = vertex.xyz; \n"
"  return projection * transform * vertex; \n"
"}";

static const char* lovrSkyboxFragmentShader = ""
"in vec3 texturePosition; \n"
"uniform samplerCube cube; \n"
"vec4 color(vec4 graphicsColor, sampler2D image, vec2 uv) { \n"
"  return graphicsColor * texture(cube, texturePosition); \n"
"}";

static const char* lovrFontFragmentShader = ""
"float median(float r, float g, float b) { \n"
"  return max(min(r, g), min(max(r, g), b)); \n"
"} \n"
"vec4 color(vec4 graphicsColor, sampler2D image, vec2 uv) { \n"
"  vec3 col = texture(image, uv).rgb; \n"
"  float sdf = median(col.r, col.g, col.b); \n"
"  float w = fwidth(sdf); \n"
"  float alpha = smoothstep(.5 - w, .5 + w, sdf); \n"
"  return vec4(graphicsColor.rgb, graphicsColor.a * alpha); \n"
"}";

static const char* lovrNoopVertexShader = ""
"vec4 position(mat4 projection, mat4 transform, vec4 vertex) { \n"
"  return vertex; \n"
"}";

static GLuint compileShader(GLenum type, const char* source) {
  GLuint shader = glCreateShader(type);

  glShaderSource(shader, 1, (const GLchar**)&source, NULL);
  glCompileShader(shader);

  int isShaderCompiled;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &isShaderCompiled);
  if (!isShaderCompiled) {
    int logLength;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);

    char* log = malloc(logLength);
    glGetShaderInfoLog(shader, logLength, &logLength, log);
    lovrThrow("Could not compile shader %s", log);
  }

  return shader;
}

static GLuint linkShaders(GLuint vertexShader, GLuint fragmentShader) {
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

    char* log = malloc(logLength);
    glGetProgramInfoLog(shader, logLength, &logLength, log);
    lovrThrow("Could not link shader %s", log);
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
    char* subscript = strchr(uniform.name, '[');
    if (subscript) {
      *subscript = '\0';
    }
    uniform.location = glGetUniformLocation(id, uniform.name);
    uniform.index = i;
    map_set(&shader->uniforms, uniform.name, uniform);
  }

  // Initial state
  shader->id = id;
  mat4_identity(shader->model);
  mat4_identity(shader->view);
  mat4_identity(shader->projection);
  shader->color = (Color) { 0, 0, 0, 0 };

  // Send initial uniform values to shader
  lovrGraphicsBindProgram(id);
  lovrShaderBind(shader, shader->model, shader->view, shader->projection, shader->color, 1);

  return shader;
}

Shader* lovrShaderCreateDefault(DefaultShader type) {
  switch (type) {
    case SHADER_DEFAULT: return lovrShaderCreate(NULL, NULL);
    case SHADER_SKYBOX: {
      Shader* shader = lovrShaderCreate(lovrSkyboxVertexShader, lovrSkyboxFragmentShader);
      lovrShaderSendInt(shader, lovrShaderGetUniformId(shader, "cube"), 1);
      return shader;
    }
    case SHADER_FONT: return lovrShaderCreate(NULL, lovrFontFragmentShader);
    case SHADER_FULLSCREEN: return lovrShaderCreate(lovrNoopVertexShader, NULL);
    default: lovrThrow("Unknown default shader type");
  }
}

void lovrShaderDestroy(const Ref* ref) {
  Shader* shader = containerof(ref, Shader);
  glDeleteProgram(shader->id);
  map_deinit(&shader->uniforms);
  free(shader);
}

void lovrShaderBind(Shader* shader, mat4 model, mat4 view, mat4 projection, Color color, int force) {
  int dirtyModel = force || memcmp(shader->model, model, 16 * sizeof(float));
  int dirtyView = force || memcmp(shader->view, view, 16 * sizeof(float));
  int dirtyTransform = dirtyModel || dirtyView;
  int dirtyProjection = force || memcmp(shader->projection, projection, 16 * sizeof(float));
  int dirtyColor = force || memcmp(&shader->color, &color, 4 * sizeof(uint8_t));

  if (dirtyModel) {
    int uniformId = lovrShaderGetUniformId(shader, "lovrModel");
    lovrShaderSendFloatMat4(shader, uniformId, model);
    memcpy(shader->model, model, 16 * sizeof(float));
  }

  if (dirtyView) {
    int uniformId = lovrShaderGetUniformId(shader, "lovrView");
    lovrShaderSendFloatMat4(shader, uniformId, view);
    memcpy(shader->view, view, 16 * sizeof(float));
  }

  if (dirtyTransform) {
    float matrix[16];
    int uniformId = lovrShaderGetUniformId(shader, "lovrTransform");

    mat4_multiply(mat4_set(matrix, view), model);

    if (uniformId > -1) {
      lovrShaderSendFloatMat4(shader, uniformId, matrix);
    }

    if (mat4_invert(matrix)) {
      mat4_transpose(matrix);
    } else {
      mat4_identity(matrix);
    }

    float normalMatrix[9] = {
      matrix[0], matrix[1], matrix[2],
      matrix[4], matrix[5], matrix[6],
      matrix[8], matrix[9], matrix[10]
    };

    uniformId = lovrShaderGetUniformId(shader, "lovrNormalMatrix");
    if (uniformId > -1) {
      lovrShaderSendFloatMat3(shader, uniformId, normalMatrix);
    }
  }

  if (dirtyProjection) {
    int uniformId = lovrShaderGetUniformId(shader, "lovrProjection");
    lovrShaderSendFloatMat4(shader, uniformId, projection);
    memcpy(shader->projection, projection, 16 * sizeof(float));
  }

  if (dirtyColor) {
    int uniformId = lovrShaderGetUniformId(shader, "lovrColor");
    float c[4] = { color.r / 255., color.g / 255., color.b / 255., color.a / 255. };
    lovrShaderSendFloatVec4(shader, uniformId, 1, c);
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

void lovrShaderSendFloatVec2(Shader* shader, int id, int count, float* vector) {
  glUniform2fv(id, count, vector);
}

void lovrShaderSendFloatVec3(Shader* shader, int id, int count, float* vector) {
  glUniform3fv(id, count, vector);
}

void lovrShaderSendFloatVec4(Shader* shader, int id, int count, float* vector) {
  glUniform4fv(id, count, vector);
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
