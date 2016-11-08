#define _USE_MATH_DEFINES
#include "graphics.h"
#include "../glfw.h"
#include "../util.h"
#include <stdlib.h>
#include <math.h>
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

static GraphicsState state;

void lovrGraphicsInit() {
  vec_init(&state.transforms);
  state.projection = mat4_init();
  state.lastTransform = mat4_init();
  state.lastProjection = mat4_init();
  state.defaultShader = lovrShaderCreate(lovrDefaultVertexShader, lovrDefaultFragmentShader);
  state.skyboxShader = lovrShaderCreate(lovrSkyboxVertexShader, lovrSkyboxFragmentShader);
  state.lastShader = NULL;
  state.lastColor = 0;
  glGenBuffers(1, &state.shapeBuffer);
  glGenBuffers(1, &state.shapeIndexBuffer);
  glGenVertexArrays(1, &state.shapeArray);
  vec_init(&state.shapeData);
  vec_init(&state.shapeIndices);
  lovrGraphicsReset();
}

void lovrGraphicsDestroy() {
  vec_deinit(&state.transforms);
  mat4_deinit(state.projection);
  mat4_deinit(state.lastTransform);
  mat4_deinit(state.lastProjection);
  lovrShaderDestroy(state.defaultShader);
  lovrShaderDestroy(state.skyboxShader);
  glDeleteBuffers(1, &state.shapeBuffer);
  glDeleteBuffers(1, &state.shapeIndexBuffer);
  glDeleteVertexArrays(1, &state.shapeArray);
  vec_deinit(&state.shapeData);
  vec_deinit(&state.shapeIndices);
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
  lovrGraphicsSetCullingEnabled(0);
  lovrGraphicsSetPolygonWinding(POLYGON_WINDING_COUNTERCLOCKWISE);
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

  int shaderChanged = shader != state.lastShader;
  state.lastShader = shader;

  mat4 transform = vec_last(&state.transforms);
  mat4 lastTransform = state.lastTransform;

  if (shaderChanged || memcmp(transform, lastTransform, 16 * sizeof(float))) {
    int uniformId = lovrShaderGetUniformId(shader, "lovrTransform");
    lovrShaderSendFloatMat4(shader, uniformId, transform);
    memcpy(lastTransform, transform, 16 * sizeof(float));
  }

  mat4 projection = state.projection;
  mat4 lastProjection = state.lastProjection;

  if (shaderChanged || memcmp(projection, lastProjection, 16 * sizeof(float))) {
    int uniformId = lovrShaderGetUniformId(shader, "lovrProjection");
    lovrShaderSendFloatMat4(shader, uniformId, projection);
    memcpy(lastProjection, projection, 16 * sizeof(float));
  }

  if (shaderChanged || state.lastColor != state.color) {
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

char lovrGraphicsIsCullingEnabled() {
  return state.isCullingEnabled;
}

void lovrGraphicsSetCullingEnabled(char isEnabled) {
  state.isCullingEnabled = isEnabled;
  if (isEnabled) {
    glEnable(GL_CULL_FACE);
  } else {
    glDisable(GL_CULL_FACE);
  }
}

PolygonWinding lovrGraphicsGetPolygonWinding() {
  return state.polygonWinding;
}

void lovrGraphicsSetPolygonWinding(PolygonWinding winding) {
  state.polygonWinding = winding;
  glFrontFace(winding);
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

void lovrGraphicsTransform(float tx, float ty, float tz, float sx, float sy, float sz, float angle, float ax, float ay, float az) {

  // Normalize rotation vector
  float len = sqrtf(ax * ax + ay * ay + az * az);
  if (len != 1 && len != 0) {
    len = 1 / len;
    ax *= len;
    ay *= len;
    az *= len;
  }

  // Convert angle-axis to quaternion
  float cos2 = cos(angle / 2.f);
  float sin2 = sin(angle / 2.f);
  float qw = cos2;
  float qx = sin2 * ax;
  float qy = sin2 * ay;
  float qz = sin2 * az;

  // M *= T * S * R
  float transform[16];
  mat4_setTranslation(transform, tx, ty, tz);
  mat4_scale(transform, sx, sy, sz);
  mat4_rotate(transform, qw, qx, qy, qz);
  lovrGraphicsMatrixTransform(transform);
}

void lovrGraphicsMatrixTransform(mat4 transform) {
  mat4_multiply(vec_last(&state.transforms), transform);
}

void lovrGraphicsGetDimensions(int* width, int* height) {
  glfwGetFramebufferSize(window, width, height);
}

void lovrGraphicsSetShapeData(float* data, int dataCount, unsigned int* indices, int indicesCount) {
  vec_clear(&state.shapeIndices);
  vec_clear(&state.shapeData);

  if (data) {
    vec_pusharr(&state.shapeData, data, dataCount);
  }

  if (indices) {
    vec_pusharr(&state.shapeIndices, indices, indicesCount);
  }
}

void lovrGraphicsDrawLinedShape(GLenum mode) {
  vec_float_t* vertices = &state.shapeData;
  vec_uint_t* indices = &state.shapeIndices;

  lovrGraphicsPrepare();
  glBindVertexArray(state.shapeArray);
  glBindBuffer(GL_ARRAY_BUFFER, state.shapeBuffer);
  glBufferData(GL_ARRAY_BUFFER, vertices->length * sizeof(float), vertices->data, GL_STREAM_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);

  if (indices->length > 0) {
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state.shapeIndexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices->length * sizeof(unsigned int), indices->data, GL_STREAM_DRAW);
    glDrawElements(mode, indices->length, GL_UNSIGNED_INT, NULL);
  } else {
    glDrawArrays(mode, 0, vertices->length / 3);
  }
}

void lovrGraphicsDrawFilledShape() {
  vec_float_t* vertices = &state.shapeData;
  int stride = 6;
  int strideBytes = stride * sizeof(float);

  lovrGraphicsPrepare();
  glBindVertexArray(state.shapeArray);
  glBindBuffer(GL_ARRAY_BUFFER, state.shapeBuffer);
  glBufferData(GL_ARRAY_BUFFER, vertices->length * sizeof(float), vertices->data, GL_STREAM_DRAW);
  glEnableVertexAttribArray(LOVR_SHADER_POSITION);
  glVertexAttribPointer(LOVR_SHADER_POSITION, 3, GL_FLOAT, GL_FALSE, strideBytes, (void*) 0);
  glEnableVertexAttribArray(LOVR_SHADER_NORMAL);
  glVertexAttribPointer(LOVR_SHADER_NORMAL, 3, GL_FLOAT, GL_FALSE, strideBytes, (void*) (3 * sizeof(float)));
  glDrawArrays(GL_TRIANGLE_STRIP, 0, vertices->length / stride);
  glBindVertexArray(0);
}

void lovrGraphicsPoints(float* points, int count) {
  lovrGraphicsSetShapeData(points, count, NULL, 0);
  lovrGraphicsDrawLinedShape(GL_POINTS);
}

void lovrGraphicsLine(float* points, int count) {
  lovrGraphicsSetShapeData(points, count, NULL, 0);
  lovrGraphicsDrawLinedShape(GL_LINE_STRIP);
}

void lovrGraphicsPlane(DrawMode mode, float x, float y, float z, float size, float nx, float ny, float nz) {

  // Normalize the normal vector
  float len = sqrt(nx * nx + ny * ny + nz + nz);
  if (len != 1) {
    len = 1 / len;
    nx *= len;
    ny *= len;
    nz *= len;
  }

  // Vector perpendicular to the normal vector and the vector of the default geometry (cross product)
  float cx = -ny;
  float cy = nx;
  float cz = 0.f;

  // Angle between normal vector and the normal vector of the default geometry (dot product)
  float theta = acos(nz);

  lovrGraphicsPush();
  lovrGraphicsTransform(x, y, z, size, size, size, theta, cx, cy, cz);

  if (mode == DRAW_MODE_LINE) {
    float points[] = {
      -.5, .5, 0,
      .5, .5, 0,
      .5, -.5, 0,
      -.5, -.5, 0
    };

    lovrGraphicsSetShapeData(points, 12, NULL, 0);
    lovrGraphicsDrawLinedShape(GL_LINE_LOOP);
  } else if (mode == DRAW_MODE_FILL) {
    float data[] = {
      -.5, .5, 0,  0, 0, -1,
      -.5, -.5, 0, 0, 0, -1,
      .5, .5, 0,   0, 0, -1,
      .5, -.5, 0,  0, 0, -1
    };

    lovrGraphicsSetShapeData(data, 24, NULL, 0);
    lovrGraphicsDrawFilledShape();
  }

  lovrGraphicsPop();
}

void lovrGraphicsCube(DrawMode mode, float x, float y, float z, float size, float angle, float axisX, float axisY, float axisZ) {
  lovrGraphicsPush();
  lovrGraphicsTransform(x, y, z, size, size, size, angle, axisX, axisY, axisZ);

  if (mode == DRAW_MODE_LINE) {
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

    unsigned int indices[] = {
      0, 1, 1, 2, 2, 3, 3, 0, // Front
      4, 5, 5, 6, 6, 7, 7, 4, // Back
      0, 4, 1, 5, 2, 6, 3, 7  // Connections
    };

    lovrGraphicsSetShapeData(points, 24, indices, 24);
    lovrGraphicsDrawLinedShape(GL_LINES);
  } else {
    float data[] = {
      // Front
      -.5, -.5, -.5,  0, 0, -1,
      .5, -.5, -.5,   0, 0, -1,
      -.5, .5, -.5,   0, 0, -1,
      .5, .5, -.5,    0, 0, -1,

      // Right
      .5, .5, -.5,    1, 0, 0,
      .5, -.5, -.5,   1, 0, 0,
      .5, .5, .5,     1, 0, 0,
      .5, -.5, .5,    1, 0, 0,

      // Back
      .5, -.5, .5,    0, 0, 1,
      -.5, -.5, .5,   0, 0, 1,
      .5, .5, .5,     0, 0, 1,
      -.5, .5, .5,    0, 0, 1,

      // Left
      -.5, .5, .5,    -1, 0, 0,
      -.5, -.5, .5,   -1, 0, 0,
      -.5, .5, -.5,   -1, 0, 0,
      -.5, -.5, -.5,  -1, 0, 0,

      // Bottom
      -.5, -.5, -.5,   0, -1, 0,
      -.5, -.5, .5,    0, -1, 0,
      .5, -.5, -.5,    0, -1, 0,
      .5, -.5, .5,     0, -1, 0,

      // Adjust
      .5, -.5, .5,     0, 1, 0,
      -.5, .5, -.5,    0, 1, 0,

      // Top
      -.5, .5, -.5,    0, 1, 0,
      .5, .5, -.5,     0, 1, 0,
      -.5, .5, .5,     0, 1, 0,
      .5, .5, .5,      0, 1, 0
    };

    lovrGraphicsSetShapeData(data, 156, NULL, 0);
    lovrGraphicsDrawFilledShape();
  }

  lovrGraphicsPop();
}

void lovrGraphicsSkybox(Skybox* skybox, float angle, float ax, float ay, float az) {
  if (!skybox) {
    return;
  }

  Shader* lastShader = lovrGraphicsGetShader();
  lovrGraphicsSetShader(state.skyboxShader);

  float cos2 = cos(angle / 2);
  float sin2 = sin(angle / 2);
  float rw = cos2;
  float rx = sin2 * ax;
  float ry = sin2 * ay;
  float rz = sin2 * az;

  lovrGraphicsPrepare();
  lovrGraphicsPush();
  lovrGraphicsOrigin();
  lovrGraphicsRotate(rw, rx, ry, rz);

  float cube[] = {
    // Front
    1.f, -1.f, -1.f,  0, 0, 0,
    1.f, 1.f, -1.f,   0, 0, 0,
    -1.f, -1.f, -1.f, 0, 0, 0,
    -1.f, 1.f, -1.f,  0, 0, 0,

    // Left
    -1.f, 1.f, -1.f,  0, 0, 0,
    -1.f, 1.f, 1.f,   0, 0, 0,
    -1.f, -1.f, -1.f, 0, 0, 0,
    -1.f, -1.f, 1.f,  0, 0, 0,

    // Back
    -1.f, -1.f, 1.f,  0, 0, 0,
    1.f, -1.f, 1.f,   0, 0, 0,
    -1.f, 1.f, 1.f,   0, 0, 0,
    1.f, 1.f, 1.f,    0, 0, 0,

    // Right
    1.f, 1.f, 1.f,    0, 0, 0,
    1.f, -1.f, 1.f,   0, 0, 0,
    1.f, 1.f, -1.f,   0, 0, 0,
    1.f, -1.f, -1.f,  0, 0, 0,

    // Bottom
    1.f, -1.f, -1.f,  0, 0, 0,
    1.f, -1.f, 1.f,   0, 0, 0,
    -1.f, -1.f, -1.f, 0, 0, 0,
    -1.f, -1.f, 1.f,  0, 0, 0,

    // Adjust
    -1.f, -1.f, 1.f,  0, 0, 0,
    -1.f, 1.f, -1.f,  0, 0, 0,

    // Top
    -1.f, 1.f, -1.f,  0, 0, 0,
    -1.f, 1.f, 1.f,   0, 0, 0,
    1.f, 1.f, -1.f,   0, 0, 0,
    1.f, 1.f, 1.f,    0, 0, 0
  };

  glDepthMask(GL_FALSE);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_CUBE_MAP, skybox->texture);

  lovrGraphicsSetShapeData(cube, 156, NULL, 0);
  lovrGraphicsDrawFilledShape();

  glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
  glDepthMask(GL_TRUE);

  lovrGraphicsSetShader(lastShader);
  lovrGraphicsPop();
}
