#include "graphics.h"
#include "model.h"
#include "buffer.h"
#include "shader.h"
#include "../glfw.h"
#include "../util.h"
#include <stdlib.h>
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "../headset/headset.h"

typedef struct {
  Shader* activeShader;
  vec_mat4_t transforms;
} GraphicsState;

GraphicsState graphicsState;

void lovrGraphicsInit() {
  vec_init(&graphicsState.transforms);
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

void lovrGraphicsGetClearColor(float* r, float* g, float* b, float* a) {
  GLfloat clearColor[4];
  glGetFloatv(GL_COLOR_CLEAR_VALUE, clearColor);
  *r = clearColor[0];
  *g = clearColor[1];
  *b = clearColor[2];
  *a = clearColor[3];
}

void lovrGraphicsSetClearColor(float r, float g, float b, float a) {
  glClearColor(r / 255, g / 255, b / 255, a / 255);
}

Shader* lovrGraphicsGetShader() {
  return graphicsState.activeShader;
}

// TODO default shader
void lovrGraphicsSetShader(Shader* shader) {
  graphicsState.activeShader = shader;
  glUseProgram(shader->id);
}

int lovrGraphicsPush() {
  vec_mat4_t* transforms = &graphicsState.transforms;
  if (transforms->length > 64) { return 1; }
  vec_push(transforms, transforms->length > 0 ? mat4_copy(vec_last(transforms)) : mat4_init());
  return 0;
}

int lovrGraphicsPop() {
  vec_mat4_t* transforms = &graphicsState.transforms;
  if (transforms->length == 0) { return 1; }
  mat4_deinit(vec_pop(transforms));
  return 0;
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
