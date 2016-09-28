#define _USE_MATH_DEFINES
#include "graphics.h"
#include "model.h"
#include "buffer.h"
#include "shader.h"
#include "../glfw.h"
#include "../util.h"
#include <stdlib.h>
#include <math.h>
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "../headset/headset.h"

static GraphicsState state;

void lovrGraphicsInit() {
  vec_init(&state.transforms);
  state.projection = mat4_init();
  state.lastTransform = mat4_init();
  state.lastProjection = mat4_init();

  char vertexShaderSource[128];
  snprintf(vertexShaderSource, sizeof(vertexShaderSource), "%s",
    "void main() { \n"
    "  gl_Position = lovrProjection * lovrTransform * vec4(position.xyz, 1.0); \n"
    "}"
  );

  char fragmentShaderSource[64];
  snprintf(fragmentShaderSource, sizeof(fragmentShaderSource), "%s",
    "void main() { \n"
    "  color = vec4(1.0); \n"
    "}"
  );

  state.defaultShader = lovrGraphicsNewShader(vertexShaderSource, fragmentShaderSource);

  lovrGraphicsReset();
}

void lovrGraphicsReset() {
  int i;
  mat4 matrix;

  vec_foreach(&state.transforms, matrix, i) {
    mat4_deinit(matrix);
  }

  vec_clear(&state.transforms);
  vec_push(&state.transforms, mat4_init());

  memset(state.lastTransform, 0, 16);
  memset(state.lastProjection, 0, 16);

  // TODO customize via lovr.conf
  lovrGraphicsSetProjection(.1f, 100.f, 67 * M_PI / 180);

  lovrGraphicsSetShader(state.defaultShader);
  lovrGraphicsSetColorMask(1, 1, 1, 1);
}

void lovrGraphicsClear(int color, int depth) {
  int bits = 0;

  if (color) {
    bits |= GL_COLOR_BUFFER_BIT;
  }

  if (depth) {
    bits |= GL_DEPTH_BUFFER_BIT;
  }

  glClear(bits);
}

void lovrGraphicsPresent() {
  glfwSwapBuffers(window);
}

void lovrGraphicsPrepare() {
  Shader* shader = lovrGraphicsGetShader();

  if (!shader) {
    return;
  }

  mat4 transform = vec_last(&state.transforms);
  mat4 lastTransform = state.lastTransform;

  if (memcmp(transform, lastTransform, 16 * sizeof(float))) {
    int uniformId = lovrShaderGetUniformId(shader, "lovrTransform");
    lovrShaderSendFloatMat4(shader, uniformId, transform);
    memcpy(lastTransform, transform, 16 * sizeof(float));
  }

  mat4 projection = state.projection;
  mat4 lastProjection = state.lastProjection;

  if (memcmp(projection, lastProjection, 16 * sizeof(float))) {
    int uniformId = lovrShaderGetUniformId(shader, "lovrProjection");
    lovrShaderSendFloatMat4(shader, uniformId, projection);
    memcpy(lastProjection, projection, 16 * sizeof(float));
  }
}

void lovrGraphicsGetBackgroundColor(float* r, float* g, float* b, float* a) {
  GLfloat clearColor[4];
  glGetFloatv(GL_COLOR_CLEAR_VALUE, clearColor);
  *r = clearColor[0];
  *g = clearColor[1];
  *b = clearColor[2];
  *a = clearColor[3];
}

void lovrGraphicsSetBackgroundColor(float r, float g, float b, float a) {
  glClearColor(r / 255, g / 255, b / 255, a / 255);
}

void lovrGraphicsGetColorMask(char* r, char* g, char* b, char* a) {
  char mask = state.colorMask;
  *r = mask & 0x1;
  *g = mask & 0x2;
  *b = mask & 0x4;
  *a = mask & 0x8;
}

void lovrGraphicsSetColorMask(char r, char g, char b, char a) {
  state.colorMask = ((r & 1) << 0) | ((g & 1) << 1) | ((b & 1) << 2) | ((a & 1) << 3);
  glColorMask(r, g, b, a);
}

Shader* lovrGraphicsGetShader() {
  return state.activeShader;
}

void lovrGraphicsSetShader(Shader* shader) {
  if (!shader) {
    shader = state.defaultShader;
  }

  state.activeShader = shader;
  glUseProgram(shader->id);
}

void lovrGraphicsSetProjection(float near, float far, float fov) {
  int width, height;
  glfwGetWindowSize(window, &width, &height);
  mat4_setProjection(state.projection, near, far, fov, (float) width / height);
}

void lovrGraphicsSetProjectionRaw(mat4 projection) {
  memcpy(state.projection, projection, 16 * sizeof(float));
}

int lovrGraphicsPush() {
  vec_mat4_t* transforms = &state.transforms;
  if (transforms->length >= 64) { return 1; }
  vec_push(transforms, mat4_copy(vec_last(transforms)));
  return 0;
}

int lovrGraphicsPop() {
  vec_mat4_t* transforms = &state.transforms;
  if (transforms->length <= 1) { return 1; }
  mat4_deinit(vec_pop(transforms));
  return 0;
}

void lovrGraphicsOrigin() {
  vec_mat4_t* transforms = &state.transforms;
  mat4_setIdentity(vec_last(transforms));
}

void lovrGraphicsTranslate(float x, float y, float z) {
  mat4_translate(vec_last(&state.transforms), x, y, z);
}

void lovrGraphicsRotate(float w, float x, float y, float z) {
  mat4_rotate(vec_last(&state.transforms), w, x, y, z);
}

void lovrGraphicsScale(float x, float y, float z) {
  mat4_scale(vec_last(&state.transforms), x, y, z);
}

Buffer* lovrGraphicsNewBuffer(int size, BufferDrawMode drawMode, BufferUsage usage) {
  Buffer* buffer = malloc(sizeof(Buffer));

  buffer->drawMode = drawMode;
  buffer->usage = usage;
  buffer->size = size;
  buffer->data = malloc(buffer->size * 3 * sizeof(GLfloat));
  buffer->vao = 0;
  buffer->vbo = 0;
  buffer->ibo = 0;
  buffer->isRangeEnabled = 0;
  buffer->rangeStart = 0;
  buffer->rangeCount = buffer->size;

  glGenBuffers(1, &buffer->vbo);
  glBindBuffer(GL_ARRAY_BUFFER, buffer->vbo);
  glBufferData(GL_ARRAY_BUFFER, buffer->size * 3 * sizeof(GLfloat), buffer->data, buffer->usage);

  glGenVertexArrays(1, &buffer->vao);

  vec_init(&buffer->map);

  return buffer;
}

Model* lovrGraphicsNewModel(const char* path) {
  const struct aiScene* scene = aiImportFile(path, aiProcessPreset_TargetRealtime_MaxQuality);

  if (scene) {
    return scene->mMeshes[0];
  }

  return NULL;
}

Shader* lovrGraphicsNewShader(const char* vertexSource, const char* fragmentSource) {
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
