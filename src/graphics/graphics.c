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

void lovrGraphicsInit() {
  map_init(&BufferDrawModes);
  map_set(&BufferDrawModes, "points", GL_POINTS);
  map_set(&BufferDrawModes, "strip", GL_TRIANGLE_STRIP);
  map_set(&BufferDrawModes, "triangles", GL_TRIANGLES);
  map_set(&BufferDrawModes, "fan", GL_TRIANGLE_FAN);
}

void lovrGraphicsClear() {
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
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

// TODO default shader
void lovrGraphicsSetShader(Shader* shader) {
  glUseProgram(shader->id);
}

Buffer* lovrGraphicsNewBuffer(int size) {
  Buffer* buffer = malloc(sizeof(Buffer));

  buffer->drawMode = "fan";
  buffer->size = size;
  buffer->data = malloc(buffer->size * 3 * sizeof(GLfloat));
  buffer->rangeStart = 0;
  buffer->rangeCount = buffer->size;

  glGenBuffers(1, &buffer->vbo);
  glBindBuffer(GL_ARRAY_BUFFER, buffer->vbo);
  glBufferData(GL_ARRAY_BUFFER, buffer->size * 3 * sizeof(GLfloat), buffer->data, GL_STATIC_DRAW);

  glGenVertexArrays(1, &buffer->vao);

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
  GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
  GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
  GLuint id = linkShaders(vertexShader, fragmentShader);
  Shader* shader = (Shader*) malloc(sizeof(Shader));
  shader->id = id;
  return shader;
}
