#include "graphics/graphics.h"
#include "loaders/texture.h"
#include "loaders/font.h"
#include "event/event.h"
#include "filesystem/filesystem.h"
#include "math/mat4.h"
#include "math/vec3.h"
#include "util.h"
#include "lib/stb/stb_image.h"
#define _USE_MATH_DEFINES
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

static GraphicsState state;

static void onCloseWindow(GLFWwindow* window) {
  if (window == state.window) {
    EventType type = EVENT_QUIT;
    EventData data = { .quit = { 0 } };
    Event event = { .type = type, .data = data };
    lovrEventPush(event);
  }
}

// Base

void lovrGraphicsInit() {
  //
}

void lovrGraphicsDestroy() {
  lovrGraphicsSetShader(NULL);
  lovrGraphicsSetFont(NULL);
  for (int i = 0; i < DEFAULT_SHADER_COUNT; i++) {
    if (state.defaultShaders[i]) lovrRelease(&state.defaultShaders[i]->ref);
  }
  if (state.defaultFont) lovrRelease(&state.defaultFont->ref);
  if (state.defaultTexture) lovrRelease(&state.defaultTexture->ref);
  glDeleteVertexArrays(1, &state.streamVAO);
  glDeleteBuffers(1, &state.streamVBO);
  glDeleteBuffers(1, &state.streamIBO);
  vec_deinit(&state.streamData);
  vec_deinit(&state.streamIndices);
}

void lovrGraphicsReset() {
  int w = lovrGraphicsGetWidth();
  int h = lovrGraphicsGetHeight();
  float projection[16];
  state.transform = 0;
  state.canvas = 0;
  state.defaultShader = -1;
  lovrGraphicsSetBackgroundColor((Color) { 0, 0, 0, 255 });
  lovrGraphicsSetBlendMode(BLEND_ALPHA, BLEND_ALPHA_MULTIPLY);
  lovrGraphicsSetColor((Color) { 255, 255, 255, 255 });
  lovrGraphicsSetCullingEnabled(0);
  lovrGraphicsSetDefaultFilter((TextureFilter) { .mode = FILTER_TRILINEAR });
  lovrGraphicsSetDepthTest(COMPARE_LEQUAL);
  lovrGraphicsSetFont(NULL);
  lovrGraphicsSetLineWidth(1);
  lovrGraphicsSetPointSize(1);
  lovrGraphicsSetShader(NULL);
  lovrGraphicsSetWinding(WINDING_COUNTERCLOCKWISE);
  lovrGraphicsSetWireframe(0);
  lovrGraphicsSetViewport(0, 0, w, h);
  lovrGraphicsSetProjection(mat4_perspective(projection, .01f, 100.f, 67 * M_PI / 180., (float) w / h));
  lovrGraphicsOrigin();
}

void lovrGraphicsClear(int color, int depth) {
  if (!color && !depth) return;
  glClear((color ? GL_COLOR_BUFFER_BIT : 0) | (depth ? GL_DEPTH_BUFFER_BIT : 0));
}

void lovrGraphicsPresent() {
  glfwSwapBuffers(state.window);
}

void lovrGraphicsPrepare() {
  Shader* shader = lovrGraphicsGetActiveShader();

  if (!shader) {
    shader = state.defaultShaders[state.defaultShader] = lovrShaderCreateDefault(state.defaultShader);
  }

  mat4 model = state.transforms[state.transform][MATRIX_MODEL];
  mat4 view = state.transforms[state.transform][MATRIX_VIEW];
  mat4 projection = state.canvases[state.canvas].projection;
  lovrGraphicsBindProgram(shader->id);
  lovrShaderBind(shader, model, view, projection, state.color, 0);
}

void lovrGraphicsCreateWindow(int w, int h, int fullscreen, int msaa, const char* title, const char* icon) {
  lovrAssert(!state.window, "Window is already created");

#ifdef EMSCRIPTEN
  glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
  glfwWindowHint(GLFW_SAMPLES, msaa);
#else
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  glfwWindowHint(GLFW_SAMPLES, msaa);
  glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
#endif

  GLFWmonitor* monitor = glfwGetPrimaryMonitor();
  const GLFWvidmode* mode = glfwGetVideoMode(monitor);
  if (fullscreen) {
    glfwWindowHint(GLFW_RED_BITS, mode->redBits);
    glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
    glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
    glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
  }

  state.window = glfwCreateWindow(w ? w : mode->width, h ? h : mode->height, title, fullscreen ? monitor : NULL, NULL);
  if (!state.window) {
    glfwTerminate();
    lovrThrow("Could not create window");
  }

  if (icon) {
    GLFWimage image;
    image.pixels = stbi_load(icon, &image.width, &image.height, NULL, 3);
    glfwSetWindowIcon(state.window, 1, &image);
    free(image.pixels);
  }

  glfwMakeContextCurrent(state.window);
  glfwSetWindowCloseCallback(state.window, onCloseWindow);

#ifndef EMSCRIPTEN
  gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);
  glfwSwapInterval(0);
  glEnable(GL_LINE_SMOOTH);
#endif
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glGenVertexArrays(1, &state.streamVAO);
  glGenBuffers(1, &state.streamVBO);
  glGenBuffers(1, &state.streamIBO);
  vec_init(&state.streamData);
  vec_init(&state.streamIndices);
  lovrGraphicsReset();
  atexit(lovrGraphicsDestroy);
}

int lovrGraphicsGetWidth() {
  int width, height;
  glfwGetFramebufferSize(state.window, &width, &height);
  return width;
}

int lovrGraphicsGetHeight() {
  int width, height;
  glfwGetFramebufferSize(state.window, &width, &height);
  return height;
}

// State

Color lovrGraphicsGetBackgroundColor() {
  return state.backgroundColor;
}

void lovrGraphicsSetBackgroundColor(Color color) {
  state.backgroundColor = color;
  glClearColor(color.r / 255., color.g / 255., color.b / 255., color.a / 255.);
}

void lovrGraphicsGetBlendMode(BlendMode* mode, BlendAlphaMode* alphaMode) {
  *mode =  state.blendMode;
  *alphaMode = state.blendAlphaMode;
}

void lovrGraphicsSetBlendMode(BlendMode mode, BlendAlphaMode alphaMode) {
  GLenum srcRGB = mode == BLEND_MULTIPLY ? GL_DST_COLOR : GL_ONE;

  if (srcRGB == GL_ONE && alphaMode == BLEND_ALPHA_MULTIPLY) {
    srcRGB = GL_SRC_ALPHA;
  }

  switch (mode) {
    case BLEND_ALPHA:
      glBlendEquation(GL_FUNC_ADD);
      glBlendFuncSeparate(srcRGB, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
      break;

    case BLEND_ADD:
      glBlendEquation(GL_FUNC_ADD);
      glBlendFuncSeparate(srcRGB, GL_ONE, GL_ZERO, GL_ONE);
      break;

    case BLEND_SUBTRACT:
      glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
      glBlendFuncSeparate(srcRGB, GL_ONE, GL_ZERO, GL_ONE);
      break;

    case BLEND_MULTIPLY:
      glBlendEquation(GL_FUNC_ADD);
      glBlendFuncSeparate(srcRGB, GL_ZERO, GL_DST_COLOR, GL_ZERO);
      break;

    case BLEND_LIGHTEN:
      glBlendEquation(GL_MAX);
      glBlendFuncSeparate(srcRGB, GL_ZERO, GL_ONE, GL_ZERO);
      break;

    case BLEND_DARKEN:
      glBlendEquation(GL_MIN);
      glBlendFuncSeparate(srcRGB, GL_ZERO, GL_ONE, GL_ZERO);
      break;

    case BLEND_SCREEN:
      glBlendEquation(GL_FUNC_ADD);
      glBlendFuncSeparate(srcRGB, GL_ONE_MINUS_SRC_COLOR, GL_ONE, GL_ONE_MINUS_SRC_COLOR);
      break;

    case BLEND_REPLACE:
      glBlendEquation(GL_FUNC_ADD);
      glBlendFuncSeparate(srcRGB, GL_ZERO, GL_ONE, GL_ZERO);
      break;
  }
}

Color lovrGraphicsGetColor() {
  return state.color;
}

void lovrGraphicsSetColor(Color color) {
  state.color = color;
}

int lovrGraphicsIsCullingEnabled() {
  return state.culling;
}

void lovrGraphicsSetCullingEnabled(int culling) {
  if (culling != state.culling) {
    state.culling = culling;
    if (culling) {
      glEnable(GL_CULL_FACE);
    } else {
      glDisable(GL_CULL_FACE);
    }
  }
}

TextureFilter lovrGraphicsGetDefaultFilter() {
  return state.defaultFilter;
}

void lovrGraphicsSetDefaultFilter(TextureFilter filter) {
  state.defaultFilter = filter;
}

CompareMode lovrGraphicsGetDepthTest() {
  return state.depthTest;
}

void lovrGraphicsSetDepthTest(CompareMode depthTest) {
  if (state.depthTest != depthTest) {
    state.depthTest = depthTest;
    glDepthFunc(depthTest);
    if (depthTest) {
      glEnable(GL_DEPTH_TEST);
    } else {
      glDisable(GL_DEPTH_TEST);
    }
  }
}

Font* lovrGraphicsGetFont() {
  if (!state.font) {
    if (!state.defaultFont) {
      FontData* fontData = lovrFontDataCreate(NULL, 32);
      state.defaultFont = lovrFontCreate(fontData);
    }

    lovrGraphicsSetFont(state.defaultFont);
  }

  return state.font;
}

void lovrGraphicsSetFont(Font* font) {
  if (state.font) {
    lovrRelease(&state.font->ref);
  }

  state.font = font;

  if (font) {
    lovrRetain(&state.font->ref);
  }
}

GraphicsLimits lovrGraphicsGetLimits() {
  if (!state.limits.initialized) {
#ifdef EMSCRIPTEN
    state.limits.pointSizes[0] = state.limits.pointSizes[1] = 1.f;
#else
    glGetFloatv(GL_POINT_SIZE_RANGE, state.limits.pointSizes);
#endif
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &state.limits.textureSize);
    glGetIntegerv(GL_MAX_SAMPLES, &state.limits.textureMSAA);
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &state.limits.textureAnisotropy);
    state.limits.initialized = 1;
  }

  return state.limits;
}

float lovrGraphicsGetLineWidth() {
  return state.lineWidth;
}

void lovrGraphicsSetLineWidth(float width) {
  state.lineWidth = width;
  glLineWidth(width);
}

float lovrGraphicsGetPointSize() {
  return state.pointSize;
}

void lovrGraphicsSetPointSize(float size) {
#ifndef EMSCRIPTEN
  state.pointSize = size;
  glPointSize(size);
#endif
}

Shader* lovrGraphicsGetShader() {
  return state.shader;
}

void lovrGraphicsSetShader(Shader* shader) {
  if (shader != state.shader) {
    if (state.shader) {
      lovrRelease(&state.shader->ref);
    }

    state.shader = shader;

    if (shader) {
      lovrRetain(&state.shader->ref);
    }
  }
}

Winding lovrGraphicsGetWinding() {
  return state.winding;
}

void lovrGraphicsSetWinding(Winding winding) {
  state.winding = winding;
  glFrontFace(winding);
}

int lovrGraphicsIsWireframe() {
  return state.wireframe;
}

void lovrGraphicsSetWireframe(int wireframe) {
#ifndef EMSCRIPTEN
  if (state.wireframe != wireframe) {
    state.wireframe = wireframe;
    glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
  }
#endif
}

// Transforms

void lovrGraphicsPush() {
  if (++state.transform >= MAX_TRANSFORMS) {
    lovrThrow("Unbalanced matrix stack (more pushes than pops?)");
  }

  memcpy(state.transforms[state.transform], state.transforms[state.transform - 1], 2 * 16 * sizeof(float));
}

void lovrGraphicsPop() {
  if (--state.transform < 0) {
    lovrThrow("Unbalanced matrix stack (more pops than pushes?)");
  }
}

void lovrGraphicsOrigin() {
  mat4_identity(state.transforms[state.transform][MATRIX_MODEL]);
  mat4_identity(state.transforms[state.transform][MATRIX_VIEW]);
}

void lovrGraphicsTranslate(MatrixType type, float x, float y, float z) {
  mat4_translate(state.transforms[state.transform][type], x, y, z);
}

void lovrGraphicsRotate(MatrixType type, float angle, float ax, float ay, float az) {
  mat4_rotate(state.transforms[state.transform][type], angle, ax, ay, az);
}

void lovrGraphicsScale(MatrixType type, float x, float y, float z) {
  mat4_scale(state.transforms[state.transform][type], x, y, z);
}

void lovrGraphicsMatrixTransform(MatrixType type, mat4 transform) {
  mat4_multiply(state.transforms[state.transform][type], transform);
}

// Primitives

static void lovrGraphicsSetShapeData(float* data, int length) {
  vec_clear(&state.streamData);
  vec_pusharr(&state.streamData, data, length);
}

static void lovrGraphicsSetIndexData(unsigned int* data, int length) {
  vec_clear(&state.streamIndices);
  vec_pusharr(&state.streamIndices, data, length);
}

static void lovrGraphicsDrawPrimitive(GLenum mode, int hasNormals, int hasTexCoords, int useIndices) {
  int stride = 3 + (hasNormals ? 3 : 0) + (hasTexCoords ? 2 : 0);
  int strideBytes = stride * sizeof(float);
  float* data = state.streamData.data;
  unsigned int* indices = state.streamIndices.data;

  lovrGraphicsPrepare();
  lovrGraphicsBindVertexArray(state.streamVAO);
  lovrGraphicsBindVertexBuffer(state.streamVBO);
  glBufferData(GL_ARRAY_BUFFER, state.streamData.length * sizeof(float), data, GL_STREAM_DRAW);
  glEnableVertexAttribArray(LOVR_SHADER_POSITION);
  glVertexAttribPointer(LOVR_SHADER_POSITION, 3, GL_FLOAT, GL_FALSE, strideBytes, (void*) 0);

  if (hasNormals) {
    glEnableVertexAttribArray(LOVR_SHADER_NORMAL);
    glVertexAttribPointer(LOVR_SHADER_NORMAL, 3, GL_FLOAT, GL_FALSE, strideBytes, (void*) (3 * sizeof(float)));
  } else {
    glDisableVertexAttribArray(LOVR_SHADER_NORMAL);
  }

  if (hasTexCoords) {
    void* offset = (void*) ((hasNormals ? 6 : 3) * sizeof(float));
    glEnableVertexAttribArray(LOVR_SHADER_TEX_COORD);
    glVertexAttribPointer(LOVR_SHADER_TEX_COORD, 2, GL_FLOAT, GL_FALSE, strideBytes, offset);
  } else {
    glDisableVertexAttribArray(LOVR_SHADER_TEX_COORD);
  }

  if (useIndices) {
    lovrGraphicsBindIndexBuffer(state.streamIBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, state.streamIndices.length * sizeof(unsigned int), indices, GL_STREAM_DRAW);
    glDrawElements(mode, state.streamIndices.length, GL_UNSIGNED_INT, NULL);
  } else {
    glDrawArrays(mode, 0, state.streamData.length / stride);
  }
}

void lovrGraphicsPoints(float* points, int count) {
  lovrGraphicsBindTexture(NULL);
  lovrGraphicsSetDefaultShader(SHADER_DEFAULT);
  lovrGraphicsSetShapeData(points, count);
  lovrGraphicsDrawPrimitive(GL_POINTS, 0, 0, 0);
}

void lovrGraphicsLine(float* points, int count) {
  lovrGraphicsBindTexture(NULL);
  lovrGraphicsSetDefaultShader(SHADER_DEFAULT);
  lovrGraphicsSetShapeData(points, count);
  lovrGraphicsDrawPrimitive(GL_LINE_STRIP, 0, 0, 0);
}

void lovrGraphicsTriangle(DrawMode mode, float* points) {
  lovrGraphicsBindTexture(NULL);
  lovrGraphicsSetDefaultShader(SHADER_DEFAULT);

  if (mode == DRAW_MODE_LINE) {
    lovrGraphicsSetShapeData(points, 9);
    lovrGraphicsDrawPrimitive(GL_LINE_LOOP, 0, 0, 0);
  } else {
    float normal[3];
    vec3_cross(vec3_init(normal, &points[0]), &points[3]);

    float data[18] = {
      points[0], points[1], points[2], normal[0], normal[1], normal[2],
      points[3], points[4], points[5], normal[0], normal[1], normal[2],
      points[6], points[7], points[8], normal[0], normal[1], normal[2]
    };

    lovrGraphicsSetShapeData(data, 18);
    lovrGraphicsDrawPrimitive(GL_TRIANGLE_STRIP, 1, 0, 0);
  }
}

void lovrGraphicsPlane(DrawMode mode, Texture* texture, mat4 transform) {
  lovrGraphicsPush();
  lovrGraphicsMatrixTransform(MATRIX_MODEL, transform);

  if (mode == DRAW_MODE_LINE) {
    float points[] = {
      -.5, .5, 0,
      .5, .5, 0,
      .5, -.5, 0,
      -.5, -.5, 0
    };

    lovrGraphicsBindTexture(NULL);
    lovrGraphicsSetDefaultShader(SHADER_DEFAULT);
    lovrGraphicsSetShapeData(points, 12);
    lovrGraphicsDrawPrimitive(GL_LINE_LOOP, 0, 0, 0);
  } else if (mode == DRAW_MODE_FILL) {
    float data[] = {
      -.5, .5, 0,  0, 0, -1, 0, 0,
      -.5, -.5, 0, 0, 0, -1, 0, 1,
      .5, .5, 0,   0, 0, -1, 1, 0,
      .5, -.5, 0,  0, 0, -1, 1, 1
    };

    lovrGraphicsBindTexture(texture);
    lovrGraphicsSetDefaultShader(SHADER_DEFAULT);
    lovrGraphicsSetShapeData(data, 32);
    lovrGraphicsDrawPrimitive(GL_TRIANGLE_STRIP, 1, 1, 0);
  }

  lovrGraphicsPop();
}

void lovrGraphicsPlaneFullscreen(Texture* texture) {
  float data[] = {
    -1, 1, 0,  0, 1,
    -1, -1, 0, 0, 0,
    1, 1, 0,   1, 1,
    1, -1, 0,  1, 0
  };

  lovrGraphicsBindTexture(texture);
  lovrGraphicsSetDefaultShader(SHADER_FULLSCREEN);
  lovrGraphicsSetShapeData(data, 20);
  lovrGraphicsDrawPrimitive(GL_TRIANGLE_STRIP, 0, 1, 0);
}

void lovrGraphicsBox(DrawMode mode, Texture* texture, mat4 transform) {
  lovrGraphicsSetDefaultShader(SHADER_DEFAULT);
  lovrGraphicsPush();
  lovrGraphicsMatrixTransform(MATRIX_MODEL, transform);

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

    lovrGraphicsBindTexture(NULL);
    lovrGraphicsSetDefaultShader(SHADER_DEFAULT);
    lovrGraphicsSetShapeData(points, 24);
    lovrGraphicsSetIndexData(indices, 24);
    lovrGraphicsDrawPrimitive(GL_LINES, 0, 0, 1);
  } else {
    float data[] = {
      // Front
      -.5, -.5, -.5,  0, 0, -1, 0, 0,
      .5, -.5, -.5,   0, 0, -1, 1, 0,
      -.5, .5, -.5,   0, 0, -1, 0, 1,
      .5, .5, -.5,    0, 0, -1, 1, 1,

      // Right
      .5, .5, -.5,    1, 0, 0,  0, 1,
      .5, -.5, -.5,   1, 0, 0,  0, 0,
      .5, .5, .5,     1, 0, 0,  1, 1,
      .5, -.5, .5,    1, 0, 0,  1, 0,

      // Back
      .5, -.5, .5,    0, 0, 1,  0, 0,
      -.5, -.5, .5,   0, 0, 1,  1, 0,
      .5, .5, .5,     0, 0, 1,  0, 1,
      -.5, .5, .5,    0, 0, 1,  1, 1,

      // Left
      -.5, .5, .5,   -1, 0, 0,  0, 1,
      -.5, -.5, .5,  -1, 0, 0,  0, 0,
      -.5, .5, -.5,  -1, 0, 0,  1, 1,
      -.5, -.5, -.5, -1, 0, 0,  1, 0,

      // Bottom
      -.5, -.5, -.5,  0, -1, 0, 0, 0,
      -.5, -.5, .5,   0, -1, 0, 0, 1,
      .5, -.5, -.5,   0, -1, 0, 1, 0,
      .5, -.5, .5,    0, -1, 0, 1, 1,

      // Adjust
      .5, -.5, .5,    0, 1, 0,  0, 1,
      -.5, .5, -.5,   0, 1, 0,  0, 1,

      // Top
      -.5, .5, -.5,   0, 1, 0,  0, 1,
      .5, .5, -.5,    0, 1, 0,  1, 1,
      -.5, .5, .5,    0, 1, 0,  0, 0,
      .5, .5, .5,     0, 1, 0,  1, 0
    };

    lovrGraphicsBindTexture(texture);
    lovrGraphicsSetDefaultShader(SHADER_DEFAULT);
    lovrGraphicsSetShapeData(data, 208);
    lovrGraphicsDrawPrimitive(GL_TRIANGLE_STRIP, 1, 1, 0);
  }

  lovrGraphicsPop();
}

void lovrGraphicsCylinder(float x1, float y1, float z1, float x2, float y2, float z2, float r1, float r2, int capped, int segments) {
  float axis[3] = { x1 - x2, y1 - y2, z1 - z2 };
  float n[3] = { x1 - x2, y1 - y2, z1 - z2 };
  float p[3];
  float q[3];

  int stride = 6;
  int dataSize = stride * ((capped && r1) * (segments + 2) + (capped && r2) * (segments + 2) + 2 * (segments + 1));
  int indexSize = 3 * segments * ((capped && r1) + (capped && r2) + 2);

  vec_clear(&state.streamData);
  vec_reserve(&state.streamData, dataSize);
  state.streamData.length = 0;
  float* data = state.streamData.data;

  vec_clear(&state.streamIndices);
  vec_reserve(&state.streamIndices, indexSize);
  state.streamIndices.length = 0;
  unsigned int* indices = state.streamIndices.data;

  vec3_init(p, n);

  if (n[0] == 0 && n[2] == 0) {
    p[0] += 1;
  } else {
    p[1] += 1;
  }

  vec3_init(q, p);
  vec3_cross(q, n);
  vec3_cross(n, q);
  vec3_init(p, n);
  vec3_normalize(p);
  vec3_normalize(q);
  vec3_normalize(axis);

#define PUSH_CYLINDER_VERTEX(x, y, z, nx, ny, nz) \
  data[state.streamData.length++] = x; \
  data[state.streamData.length++] = y; \
  data[state.streamData.length++] = z; \
  data[state.streamData.length++] = nx; \
  data[state.streamData.length++] = ny; \
  data[state.streamData.length++] = nz;

#define PUSH_CYLINDER_TRIANGLE(i1, i2, i3) \
  indices[state.streamIndices.length++] = i1; \
  indices[state.streamIndices.length++] = i2; \
  indices[state.streamIndices.length++] = i3;

  // Ring
  int ringOffset = state.streamData.length / 6;
  for (int i = 0; i <= segments; i++) {
    float theta = i * (2 * M_PI) / segments;

    n[0] = cos(theta) * p[0] + sin(theta) * q[0];
    n[1] = cos(theta) * p[1] + sin(theta) * q[1];
    n[2] = cos(theta) * p[2] + sin(theta) * q[2];

    PUSH_CYLINDER_VERTEX(x1 + r1 * n[0], y1 + r1 * n[1], z1 + r1 * n[2], n[0], n[1], n[2]);
    PUSH_CYLINDER_VERTEX(x2 + r2 * n[0], y2 + r2 * n[1], z2 + r2 * n[2], n[0], n[1], n[2]);
  }

  // Top
  int topOffset = state.streamData.length / 6;
  if (capped && r1 != 0) {
    PUSH_CYLINDER_VERTEX(x1, y1, z1, axis[0], axis[1], axis[2]);

    for (int i = 0; i <= segments; i++) {
      int j = i * 2 * stride;
      PUSH_CYLINDER_VERTEX(data[j + 0], data[j + 1], data[j + 2], axis[0], axis[1], axis[2]);
    }
  }

  // Bottom
  int bottomOffset = state.streamData.length / 6;
  if (capped && r2 != 0) {
    PUSH_CYLINDER_VERTEX(x2, y2, z2, -axis[0], -axis[1], -axis[2]);

    for (int i = 0; i <= segments; i++) {
      int j = i * 2 * stride + stride;
      PUSH_CYLINDER_VERTEX(data[j + 0], data[j + 1], data[j + 2], -axis[0], -axis[1], -axis[2]);
    }
  }

  // Indices
  for (int i = 0; i < segments; i++) {
    int j = ringOffset + 2 * i;
    PUSH_CYLINDER_TRIANGLE(j, j + 1, j + 2);
    PUSH_CYLINDER_TRIANGLE(j + 1, j + 3, j + 2);

    if (capped && r1 != 0) {
      PUSH_CYLINDER_TRIANGLE(topOffset, topOffset + i + 1, topOffset + i + 2);
    }

    if (capped && r2 != 0) {
      PUSH_CYLINDER_TRIANGLE(bottomOffset, bottomOffset + i + 1, bottomOffset + i + 2);
    }
  }

  lovrGraphicsBindTexture(NULL);
  lovrGraphicsSetDefaultShader(SHADER_DEFAULT);
  lovrGraphicsDrawPrimitive(GL_TRIANGLES, 1, 0, 1);
#undef PUSH_CYLINDER_VERTEX
#undef PUSH_CYLINDER_TRIANGLE
}

void lovrGraphicsSphere(Texture* texture, mat4 transform, int segments, Skybox* skybox) {
  vec_clear(&state.streamData);
  vec_clear(&state.streamIndices);

  for (int i = 0; i <= segments; i++) {
    float v = i / (float) segments;
    for (int j = 0; j <= segments; j++) {
      float u = j / (float) segments;

      float x = sin(u * 2 * M_PI) * sin(v * M_PI);
      float y = cos(v * M_PI);
      float z = -cos(u * 2 * M_PI) * sin(v * M_PI);

      vec_push(&state.streamData, x);
      vec_push(&state.streamData, y);
      vec_push(&state.streamData, z);

      if (!skybox) {
        vec_push(&state.streamData, x);
        vec_push(&state.streamData, y);
        vec_push(&state.streamData, z);
      }

      vec_push(&state.streamData, u);
      vec_push(&state.streamData, v);
    }
  }

  for (int i = 0; i < segments; i++) {
    unsigned int offset0 = i * (segments + 1);
    unsigned int offset1 = (i + 1) * (segments + 1);
    for (int j = 0; j < segments; j++) {
      unsigned int index0 = offset0 + j;
      unsigned int index1 = offset1 + j;
      vec_push(&state.streamIndices, index0);
      vec_push(&state.streamIndices, index1);
      vec_push(&state.streamIndices, index0 + 1);
      vec_push(&state.streamIndices, index1);
      vec_push(&state.streamIndices, index1 + 1);
      vec_push(&state.streamIndices, index0 + 1);
    }
  }

  lovrGraphicsSetDefaultShader(SHADER_DEFAULT);

  if (skybox) {
    Texture* oldTexture = lovrGraphicsGetTexture();
    glBindTexture(GL_TEXTURE_2D, skybox->texture);
    lovrGraphicsDrawPrimitive(GL_TRIANGLES, 0, 1, 1);
    glBindTexture(GL_TEXTURE_2D, oldTexture->id);
  } else {
    lovrGraphicsBindTexture(texture);
    lovrGraphicsPush();
    lovrGraphicsMatrixTransform(MATRIX_MODEL, transform);
    lovrGraphicsDrawPrimitive(GL_TRIANGLES, 1, 1, 1);
    lovrGraphicsPop();
  }
}

void lovrGraphicsSkybox(Skybox* skybox, float angle, float ax, float ay, float az) {
  lovrGraphicsPush();
  lovrGraphicsOrigin();
  lovrGraphicsRotate(MATRIX_MODEL, angle, ax, ay, az);
  glDepthMask(GL_FALSE);
  int wasCulling = lovrGraphicsIsCullingEnabled();
  lovrGraphicsSetCullingEnabled(0);

  if (skybox->type == SKYBOX_CUBE) {
    float cube[] = {
      // Front
      1.f, -1.f, -1.f,
      1.f, 1.f, -1.f,
      -1.f, -1.f, -1.f,
      -1.f, 1.f, -1.f,

      // Left
      -1.f, 1.f, -1.f,
      -1.f, 1.f, 1.f,
      -1.f, -1.f, -1.f,
      -1.f, -1.f, 1.f,

      // Back
      -1.f, -1.f, 1.f,
      1.f, -1.f, 1.f,
      -1.f, 1.f, 1.f,
      1.f, 1.f, 1.f,

      // Right
      1.f, 1.f, 1.f,
      1.f, -1.f, 1.f,
      1.f, 1.f, -1.f,
      1.f, -1.f, -1.f,

      // Bottom
      1.f, -1.f, -1.f,
      1.f, -1.f, 1.f,
      -1.f, -1.f, -1.f,
      -1.f, -1.f, 1.f,

      // Adjust
      -1.f, -1.f, 1.f,
      -1.f, 1.f, -1.f,

      // Top
      -1.f, 1.f, -1.f,
      -1.f, 1.f, 1.f,
      1.f, 1.f, -1.f,
      1.f, 1.f, 1.f
    };

    lovrGraphicsSetShapeData(cube, 78);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_CUBE_MAP, skybox->texture);
    lovrGraphicsSetDefaultShader(SHADER_SKYBOX);
    lovrGraphicsDrawPrimitive(GL_TRIANGLE_STRIP, 0, 0, 0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    glActiveTexture(GL_TEXTURE0);
  } else if (skybox->type == SKYBOX_PANORAMA) {
    lovrGraphicsSphere(NULL, NULL, 30, skybox);
  }

  lovrGraphicsSetCullingEnabled(wasCulling);
  glDepthMask(GL_TRUE);
  lovrGraphicsPop();
}

void lovrGraphicsPrint(const char* str, mat4 transform, float wrap, HorizontalAlign halign, VerticalAlign valign) {
  Font* font = lovrGraphicsGetFont();
  float scale = 1 / font->pixelDensity;
  float offsety;
  lovrFontRender(font, str, wrap, halign, valign, &state.streamData, &offsety);

  lovrGraphicsPush();
  lovrGraphicsMatrixTransform(MATRIX_MODEL, transform);
  lovrGraphicsScale(MATRIX_MODEL, scale, scale, scale);
  lovrGraphicsTranslate(MATRIX_MODEL, 0, offsety, 0);
  lovrGraphicsBindTexture(font->texture);
  lovrGraphicsSetDefaultShader(SHADER_FONT);
  glDepthMask(GL_FALSE);
  lovrGraphicsDrawPrimitive(GL_TRIANGLES, 0, 1, 0);
  glDepthMask(GL_TRUE);
  lovrGraphicsPop();
}

// Internal State
void lovrGraphicsPushCanvas() {
  if (++state.canvas >= MAX_CANVASES) {
    lovrThrow("Canvas overflow");
  }

  memcpy(&state.canvases[state.canvas], &state.canvases[state.canvas - 1], sizeof(CanvasState));
}

void lovrGraphicsPopCanvas() {
  if (--state.canvas < 0) {
    lovrThrow("Canvas underflow");
  }

  int* viewport = state.canvases[state.canvas].viewport;
  lovrGraphicsSetViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
  lovrGraphicsBindFramebuffer(state.canvases[state.canvas].framebuffer);
}

mat4 lovrGraphicsGetProjection() {
  return state.canvases[state.canvas].projection;
}

void lovrGraphicsSetProjection(mat4 projection) {
  memcpy(state.canvases[state.canvas].projection, projection, 16 * sizeof(float));
}

void lovrGraphicsSetViewport(int x, int y, int w, int h) {
  state.canvases[state.canvas].viewport[0] = x;
  state.canvases[state.canvas].viewport[1] = y;
  state.canvases[state.canvas].viewport[2] = w;
  state.canvases[state.canvas].viewport[3] = h;
  glViewport(x, y, w, h);
}

void lovrGraphicsBindFramebuffer(int framebuffer) {
  state.canvases[state.canvas].framebuffer = framebuffer;
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
}

Texture* lovrGraphicsGetTexture() {
  return state.texture;
}

void lovrGraphicsBindTexture(Texture* texture) {
  if (!texture) {
    if (!state.defaultTexture) {
      state.defaultTexture = lovrTextureCreate(lovrTextureDataGetBlank(1, 1, 0xff, FORMAT_RGBA));
    }

    texture = state.defaultTexture;
  }

  if (texture != state.texture) {
    state.texture = texture;
    glBindTexture(GL_TEXTURE_2D, texture->id);
  }
}

void lovrGraphicsSetDefaultShader(DefaultShader shader) {
  state.defaultShader = shader;
}

Shader* lovrGraphicsGetActiveShader() {
  return state.shader ? state.shader : state.defaultShaders[state.defaultShader];
}

void lovrGraphicsBindProgram(uint32_t program) {
  if (state.program != program) {
    state.program = program;
    glUseProgram(program);
  }
}

void lovrGraphicsBindVertexArray(uint32_t vertexArray) {
  if (state.vertexArray != vertexArray) {
    state.vertexArray = vertexArray;
    glBindVertexArray(vertexArray);
  }
}

void lovrGraphicsBindVertexBuffer(uint32_t vertexBuffer) {
  if (state.vertexBuffer != vertexBuffer) {
    state.vertexBuffer = vertexBuffer;
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
  }
}

void lovrGraphicsBindIndexBuffer(uint32_t indexBuffer) {
  if (state.indexBuffer != indexBuffer) {
    state.indexBuffer = indexBuffer;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
  }
}
