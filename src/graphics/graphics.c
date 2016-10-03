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
  state.defaultShader = lovrGraphicsNewShader(lovrDefaultVertexShader, lovrDefaultFragmentShader);
  state.lastColor = 0;
  glGenBuffers(1, &state.shapeBuffer);
  glGenBuffers(1, &state.shapeIndexBuffer);
  glGenVertexArrays(1, &state.shapeArray);
  vec_init(&state.shapeData);
  vec_init(&state.shapeIndices);
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

  lovrGraphicsSetProjection(.1f, 100.f, 67 * M_PI / 180); // TODO customize via lovr.conf
  lovrGraphicsSetShader(state.defaultShader);
  lovrGraphicsSetBackgroundColor(0, 0, 0, 0);
  lovrGraphicsSetColor(255, 255, 255, 255);
  lovrGraphicsSetColorMask(1, 1, 1, 1);
  lovrGraphicsSetScissorEnabled(0);
  lovrGraphicsSetLineWidth(1);
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

  if (state.lastColor != state.color) {
    int uniformId = lovrShaderGetUniformId(shader, "lovrColor");
    float color[4] = {
      LOVR_COLOR_R(state.color) / 255.f,
      LOVR_COLOR_G(state.color) / 255.f,
      LOVR_COLOR_B(state.color) / 255.f,
      LOVR_COLOR_A(state.color) / 255.f
    };
    lovrShaderSendFloatVec4(shader, uniformId, color);
    state.lastColor = state.color;
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

void lovrGraphicsGetColor(unsigned char* r, unsigned char* g, unsigned char* b, unsigned char* a) {
  *r = LOVR_COLOR_R(state.color);
  *g = LOVR_COLOR_G(state.color);
  *b = LOVR_COLOR_B(state.color);
  *a = LOVR_COLOR_A(state.color);
}

void lovrGraphicsSetColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
  state.color = LOVR_COLOR(r, g, b, a);
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

char lovrGraphicsIsScissorEnabled() {
  return state.isScissorEnabled;
}

void lovrGraphicsSetScissorEnabled(char isEnabled) {
  state.isScissorEnabled = isEnabled;
  if (isEnabled) {
    glEnable(GL_SCISSOR_TEST);
  } else {
    glDisable(GL_SCISSOR_TEST);
  }
}

void lovrGraphicsGetScissor(int* x, int* y, int* width, int* height) {
  *x = state.scissor.x;
  *y = state.scissor.x;
  *width = state.scissor.width;
  *height = state.scissor.height;
}

void lovrGraphicsSetScissor(int x, int y, int width, int height) {
  int windowWidth, windowHeight;
  glfwGetFramebufferSize(window, &windowWidth, &windowHeight);
  state.scissor.x = x;
  state.scissor.x = y;
  state.scissor.width = width;
  state.scissor.height = height;
  glScissor(x, windowHeight - y, width, height);
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

float lovrGraphicsGetLineWidth() {
  return state.lineWidth;
}

void lovrGraphicsSetLineWidth(float width) {
  state.lineWidth = width;
  glLineWidth(width);
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

void lovrGraphicsTransform(mat4 transform) {
  mat4_multiply(vec_last(&state.transforms), transform);
}

void lovrGraphicsGetDimensions(int* width, int* height) {
  glfwGetFramebufferSize(window, width, height);
}

void lovrGraphicsSetShapeData(float* vertexData, int vertexLength, unsigned int* indexData, int indexLength) {
  vec_clear(&state.shapeData);
  vec_pusharr(&state.shapeData, vertexData, vertexLength);
  glBindVertexArray(state.shapeArray);
  glBindBuffer(GL_ARRAY_BUFFER, state.shapeBuffer);
  glBufferData(GL_ARRAY_BUFFER, vertexLength * sizeof(float), vertexData, GL_STREAM_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);

  vec_clear(&state.shapeIndices);
  if (indexData) {
    vec_pusharr(&state.shapeIndices, indexData, indexLength);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state.shapeIndexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, state.shapeIndices.length * sizeof(unsigned int), state.shapeIndices.data, GL_STREAM_DRAW);
  }
}

void lovrGraphicsDrawShape(GLenum mode) {
  int usingIbo = state.shapeIndices.length > 0;
  lovrGraphicsPrepare();
  glBindVertexArray(state.shapeArray);
  glEnableVertexAttribArray(0);

  if (usingIbo) {
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state.shapeIndexBuffer);
    glDrawElements(mode, state.shapeIndices.length, GL_UNSIGNED_INT, NULL);
  } else {
    glDrawArrays(mode, 0, state.shapeData.length / 3);
  }
}

void lovrGraphicsLine(float* points, int count) {
  lovrGraphicsSetShapeData(points, count, NULL, 0);
  lovrGraphicsDrawShape(GL_LINE_STRIP);
}

void lovrGraphicsCube(DrawMode mode, float x, float y, float z, float size, float angle, float axisX, float axisY, float axisZ) {
  float points[] = {
    // Front
    -.5, .5, -.5,
    .5, .5, -.5,
    .5, -.5, -.5,
    -.5, -.5, -.5,

    // Back
    -.5, .5, .5,
    .5, .5, .5,
    .5, -.5, .5,
    -.5, -.5, .5
  };

  float cos2 = cos(angle / 2);
  float sin2 = sin(angle / 2);
  float rw = cos2;
  float rx = sin2 * axisX;
  float ry = sin2 * axisY;
  float rz = sin2 * axisZ;

  float transform[16];
  mat4_setTranslation(transform, x, y, z);
  mat4_scale(transform, size, size, size);
  mat4_rotate(transform, rw, rx, ry, rz);
  lovrGraphicsPush();
  lovrGraphicsTransform(transform);

  if (mode == DRAW_MODE_LINE) {
    unsigned int indices[] = {
      0, 1, 1, 2, 2, 3, 3, 0, // Front
      4, 5, 5, 6, 6, 7, 7, 4, // Back
      0, 4, 1, 5, 2, 6, 3, 7  // Connections
    };

    lovrGraphicsSetShapeData(points, 24, indices, 24);
    lovrGraphicsDrawShape(GL_LINES);
  } else {
    unsigned int indices[] = {
      3, 2, 0, 1, // Front
      1, 2, 5, 6, // Right
      6, 7, 5, 4, // Back
      4, 7, 0, 3, // Left
      3, 7, 2, 6, // Bottom
      6, 0,
      0, 1, 4, 5 // Top
    };

    lovrGraphicsSetShapeData(points, 24, indices, 26);
    lovrGraphicsDrawShape(GL_TRIANGLE_STRIP);
  }

  lovrGraphicsPop();
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
