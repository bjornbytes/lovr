#include "graphics/graphics.h"
#include "loaders/texture.h"
#include "loaders/font.h"
#include "event/event.h"
#include "filesystem/filesystem.h"
#include "math/mat4.h"
#include "math/vec3.h"
#include "util.h"
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
#ifdef EMSCRIPTEN
  glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#else
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  glfwWindowHint(GLFW_SAMPLES, 4);
  glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
#endif

  // Create window
  const char* title = lovrFilesystemGetIdentity();
  state.window = glfwCreateWindow(800, 600, title ? title : "LÖVR", NULL, NULL);
  if (!state.window) {
    glfwTerminate();
    error("Could not create window");
  }

  // Initialize all the things
  glfwMakeContextCurrent(state.window);
  glfwSetWindowCloseCallback(state.window, onCloseWindow);
#ifndef EMSCRIPTEN
  gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);
#endif
  glfwSetTime(0);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
#ifndef EMSCRIPTEN
  glfwSwapInterval(0);
  glEnable(GL_LINE_SMOOTH);
#endif

  // Allocations
  glGenBuffers(1, &state.buffer.vbo);
  glGenBuffers(1, &state.buffer.ibo);
  glGenVertexArrays(1, &state.buffer.vao);
  vec_init(&state.buffer.data);
  vec_init(&state.buffer.indices);

  // Objects
  state.depthTest = -1;
  state.defaultFilter.mode = FILTER_TRILINEAR;
  state.defaultShader = lovrShaderCreate(lovrDefaultVertexShader, lovrDefaultFragmentShader);
  state.skyboxShader = lovrShaderCreate(lovrSkyboxVertexShader, lovrSkyboxFragmentShader);
  int uniformId = lovrShaderGetUniformId(state.skyboxShader, "cube");
  lovrShaderSendInt(state.skyboxShader, uniformId, 1);
  state.fontShader = lovrShaderCreate(lovrDefaultVertexShader, lovrFontFragmentShader);
  state.fullscreenShader = lovrShaderCreate(lovrNoopVertexShader, lovrDefaultFragmentShader);
  state.defaultTexture = lovrTextureCreate(lovrTextureDataGetBlank(1, 1, 0xff, FORMAT_RGBA));

  // State
  lovrGraphicsReset();
  atexit(lovrGraphicsDestroy);
}

void lovrGraphicsDestroy() {
  lovrGraphicsSetFont(NULL);
  lovrGraphicsSetShader(NULL);
  glUseProgram(0);
  if (state.defaultFont) {
    lovrRelease(&state.defaultFont->ref);
  }
  lovrRelease(&state.defaultShader->ref);
  lovrRelease(&state.skyboxShader->ref);
  lovrRelease(&state.fullscreenShader->ref);
  lovrRelease(&state.defaultTexture->ref);
  glDeleteBuffers(1, &state.buffer.vbo);
  glDeleteBuffers(1, &state.buffer.ibo);
  glDeleteVertexArrays(1, &state.buffer.vao);
  vec_deinit(&state.buffer.data);
  vec_deinit(&state.buffer.indices);
}

void lovrGraphicsReset() {
  int w = lovrGraphicsGetWidth();
  int h = lovrGraphicsGetHeight();
  float projection[16];
  state.transform = 0;
  state.canvas = 0;
  lovrGraphicsSetViewport(0, 0, w, h);
  lovrGraphicsSetProjection(mat4_perspective(projection, .01f, 100.f, 67 * M_PI / 180., (float) w / h));
  lovrGraphicsSetShader(NULL);
  lovrGraphicsBindTexture(NULL);
  lovrGraphicsSetBackgroundColor((Color) { 0, 0, 0, 0 });
  lovrGraphicsSetBlendMode(BLEND_ALPHA, BLEND_ALPHA_MULTIPLY);
  lovrGraphicsSetColor((Color) { 255, 255, 255, 255 });
  lovrGraphicsSetLineWidth(1);
  lovrGraphicsSetPointSize(1);
  lovrGraphicsSetCullingEnabled(0);
  lovrGraphicsSetWinding(WINDING_COUNTERCLOCKWISE);
  lovrGraphicsSetDepthTest(COMPARE_LEQUAL);
  lovrGraphicsSetWireframe(0);
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
  Shader* shader = lovrGraphicsGetShader();
  mat4 transform = state.transforms[state.transform];
  mat4 projection = state.canvases[state.canvas].projection;
  lovrShaderBind(shader, transform, projection, state.color, 0);
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
  if (!state.font && !state.defaultFont) {
    FontData* fontData = lovrFontDataCreate(NULL, 32);
    state.defaultFont = lovrFontCreate(fontData);
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
    glGetFloatv(GL_POINT_SIZE_RANGE, state.limits.pointSizes);
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
  if (!shader) {
    shader = state.defaultShader;
  }

  if (shader != state.shader) {
    if (state.shader) {
      lovrRelease(&state.shader->ref);
    }

    state.shader = shader;
    glUseProgram(shader->id);
    lovrRetain(&state.shader->ref);
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

Texture* lovrGraphicsGetTexture() {
  return state.activeTexture;
}

void lovrGraphicsBindTexture(Texture* texture) {
  if (!texture) {
    texture = state.defaultTexture;
  }

  if (texture != state.activeTexture) {
    state.activeTexture = texture;
    glBindTexture(GL_TEXTURE_2D, texture->id);
  }
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

void lovrGraphicsPushCanvas() {
  if (++state.canvas >= MAX_CANVASES) {
    error("Canvas overflow");
  }

  memcpy(&state.canvases[state.canvas], &state.canvases[state.canvas - 1], sizeof(CanvasState));
}

void lovrGraphicsPopCanvas() {
  if (--state.canvas < 0) {
    error("Canvas underflow");
  }

  int* viewport = state.canvases[state.canvas].viewport;
  lovrGraphicsSetViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
  lovrGraphicsBindFramebuffer(state.canvases[state.canvas].framebuffer);
}

void lovrGraphicsSetViewport(int x, int y, int w, int h) {
  state.canvases[state.canvas].viewport[0] = x;
  state.canvases[state.canvas].viewport[1] = y;
  state.canvases[state.canvas].viewport[2] = w;
  state.canvases[state.canvas].viewport[3] = h;
  glViewport(x, y, w, h);
}

void lovrGraphicsBindFramebuffer(int framebuffer) {
  if (state.canvases[state.canvas].framebuffer != framebuffer) {
    state.canvases[state.canvas].framebuffer = framebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  }
}

// Transforms

int lovrGraphicsPush() {
  if (++state.transform >= MAX_TRANSFORMS) { return 1; }
  memcpy(state.transforms[state.transform], state.transforms[state.transform - 1], 16 * sizeof(float));
  return 0;
}

int lovrGraphicsPop() {
  return --state.transform < 0;
}

void lovrGraphicsOrigin() {
  mat4_identity(state.transforms[state.transform]);
}

void lovrGraphicsTranslate(float x, float y, float z) {
  mat4_translate(state.transforms[state.transform], x, y, z);
}

void lovrGraphicsRotate(float angle, float ax, float ay, float az) {
  mat4_rotate(state.transforms[state.transform], angle, ax, ay, az);
}

void lovrGraphicsScale(float x, float y, float z) {
  mat4_scale(state.transforms[state.transform], x, y, z);
}

void lovrGraphicsMatrixTransform(mat4 transform) {
  mat4_multiply(state.transforms[state.transform], transform);
}

mat4 lovrGraphicsGetProjection() {
  return state.canvases[state.canvas].projection;
}

void lovrGraphicsSetProjection(mat4 projection) {
  memcpy(state.canvases[state.canvas].projection, projection, 16 * sizeof(float));
}

// Primitives

static void lovrGraphicsSetShapeData(float* data, int length) {
  vec_clear(&state.buffer.data);
  vec_pusharr(&state.buffer.data, data, length);
}

static void lovrGraphicsSetIndexData(unsigned int* data, int length) {
  vec_clear(&state.buffer.indices);
  vec_pusharr(&state.buffer.indices, data, length);
}

static void lovrGraphicsDrawPrimitive(GLenum mode, int hasNormals, int hasTexCoords, int useIndices) {
  int stride = 3 + (hasNormals ? 3 : 0) + (hasTexCoords ? 2 : 0);
  int strideBytes = stride * sizeof(float);
  float* data = state.buffer.data.data;
  unsigned int* indices = state.buffer.indices.data;

  lovrGraphicsPrepare();
  lovrGraphicsBindVertexArray(state.buffer.vao);
  lovrGraphicsBindVertexBuffer(state.buffer.vbo);
  glBufferData(GL_ARRAY_BUFFER, state.buffer.data.length * sizeof(float), data, GL_STREAM_DRAW);
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
    lovrGraphicsBindIndexBuffer(state.buffer.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, state.buffer.indices.length * sizeof(unsigned int), indices, GL_STREAM_DRAW);
    glDrawElements(mode, state.buffer.indices.length, GL_UNSIGNED_INT, NULL);
  } else {
    glDrawArrays(mode, 0, state.buffer.data.length / stride);
  }
}

void lovrGraphicsPoints(float* points, int count) {
  lovrGraphicsBindTexture(NULL);
  lovrGraphicsSetShapeData(points, count);
  lovrGraphicsDrawPrimitive(GL_POINTS, 0, 0, 0);
}

void lovrGraphicsLine(float* points, int count) {
  lovrGraphicsBindTexture(NULL);
  lovrGraphicsSetShapeData(points, count);
  lovrGraphicsDrawPrimitive(GL_LINE_STRIP, 0, 0, 0);
}

void lovrGraphicsTriangle(DrawMode mode, float* points) {
  lovrGraphicsBindTexture(NULL);

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
  lovrGraphicsMatrixTransform(transform);

  if (mode == DRAW_MODE_LINE) {
    float points[] = {
      -.5, .5, 0,
      .5, .5, 0,
      .5, -.5, 0,
      -.5, -.5, 0
    };

    lovrGraphicsBindTexture(NULL);
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

  Shader* lastShader = lovrGraphicsGetShader();

  if (lastShader == state.defaultShader) {
    lovrRetain(&lastShader->ref);
    lovrGraphicsSetShader(state.fullscreenShader);
  }

  lovrGraphicsBindTexture(texture);
  lovrGraphicsSetShapeData(data, 20);
  lovrGraphicsDrawPrimitive(GL_TRIANGLE_STRIP, 0, 1, 0);

  if (lastShader == state.defaultShader) {
    lovrGraphicsSetShader(lastShader);
    lovrRelease(&lastShader->ref);
  }
}

void lovrGraphicsBox(DrawMode mode, Texture* texture, mat4 transform) {
  lovrGraphicsPush();
  lovrGraphicsMatrixTransform(transform);

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

  vec_clear(&state.buffer.data);
  vec_reserve(&state.buffer.data, dataSize);
  state.buffer.data.length = 0;
  float* data = state.buffer.data.data;

  vec_clear(&state.buffer.indices);
  vec_reserve(&state.buffer.indices, indexSize);
  state.buffer.indices.length = 0;
  unsigned int* indices = state.buffer.indices.data;

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
  data[state.buffer.data.length++] = x; \
  data[state.buffer.data.length++] = y; \
  data[state.buffer.data.length++] = z; \
  data[state.buffer.data.length++] = nx; \
  data[state.buffer.data.length++] = ny; \
  data[state.buffer.data.length++] = nz;

#define PUSH_CYLINDER_TRIANGLE(i1, i2, i3) \
  indices[state.buffer.indices.length++] = i1; \
  indices[state.buffer.indices.length++] = i2; \
  indices[state.buffer.indices.length++] = i3;

  // Ring
  int ringOffset = state.buffer.data.length / 6;
  for (int i = 0; i <= segments; i++) {
    float theta = i * (2 * M_PI) / segments;

    n[0] = cos(theta) * p[0] + sin(theta) * q[0];
    n[1] = cos(theta) * p[1] + sin(theta) * q[1];
    n[2] = cos(theta) * p[2] + sin(theta) * q[2];

    PUSH_CYLINDER_VERTEX(x1 + r1 * n[0], y1 + r1 * n[1], z1 + r1 * n[2], n[0], n[1], n[2]);
    PUSH_CYLINDER_VERTEX(x2 + r2 * n[0], y2 + r2 * n[1], z2 + r2 * n[2], n[0], n[1], n[2]);
  }

  // Top
  int topOffset = state.buffer.data.length / 6;
  if (capped && r1 != 0) {
    PUSH_CYLINDER_VERTEX(x1, y1, z1, axis[0], axis[1], axis[2]);

    for (int i = 0; i <= segments; i++) {
      int j = i * 2 * stride;
      PUSH_CYLINDER_VERTEX(data[j + 0], data[j + 1], data[j + 2], axis[0], axis[1], axis[2]);
    }
  }

  // Bottom
  int bottomOffset = state.buffer.data.length / 6;
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
  lovrGraphicsDrawPrimitive(GL_TRIANGLES, 1, 0, 1);
#undef PUSH_CYLINDER_VERTEX
#undef PUSH_CYLINDER_TRIANGLE
}

void lovrGraphicsSphere(Texture* texture, mat4 transform, int segments) {
  vec_clear(&state.buffer.data);
  vec_clear(&state.buffer.indices);

  for (int i = 0; i <= segments; i++) {
    float v = i / (float) segments;
    for (int j = 0; j <= segments; j++) {
      float u = j / (float) segments;

      float x = -cos(u * 2 * M_PI) * sin(v * M_PI);
      float y = cos(v * M_PI);
      float z = sin(u * 2 * M_PI) * sin(v * M_PI);

      vec_push(&state.buffer.data, x);
      vec_push(&state.buffer.data, y);
      vec_push(&state.buffer.data, z);

      vec_push(&state.buffer.data, x);
      vec_push(&state.buffer.data, y);
      vec_push(&state.buffer.data, z);

      vec_push(&state.buffer.data, u);
      vec_push(&state.buffer.data, v);
    }
  }

  for (int i = 0; i < segments; i++) {
    unsigned int offset0 = i * (segments + 1);
    unsigned int offset1 = (i + 1) * (segments + 1);
    for (int j = 0; j < segments; j++) {
      unsigned int index0 = offset0 + j;
      unsigned int index1 = offset1 + j;
      vec_push(&state.buffer.indices, index0);
      vec_push(&state.buffer.indices, index1);
      vec_push(&state.buffer.indices, index0 + 1);
      vec_push(&state.buffer.indices, index1);
      vec_push(&state.buffer.indices, index1 + 1);
      vec_push(&state.buffer.indices, index0 + 1);
    }
  }

  lovrGraphicsBindTexture(texture);
  lovrGraphicsPush();
  lovrGraphicsMatrixTransform(transform);
  lovrGraphicsDrawPrimitive(GL_TRIANGLES, 1, 1, 1);
  lovrGraphicsPop();
}

void lovrGraphicsSkybox(Skybox* skybox, float angle, float ax, float ay, float az) {
  lovrGraphicsPrepare();
  lovrGraphicsPush();
  lovrGraphicsOrigin();
  lovrGraphicsRotate(angle, ax, ay, az);

  if (skybox->type == SKYBOX_CUBE) {
    Shader* lastShader = lovrGraphicsGetShader();

    if (lastShader == state.defaultShader) {
      lovrRetain(&lastShader->ref);
      lovrGraphicsSetShader(state.skyboxShader);
    }

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

    glDepthMask(GL_FALSE);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_CUBE_MAP, skybox->texture);

    int wasCulling = lovrGraphicsIsCullingEnabled();
    lovrGraphicsSetCullingEnabled(0);
    lovrGraphicsDrawPrimitive(GL_TRIANGLE_STRIP, 0, 0, 0);
    lovrGraphicsSetCullingEnabled(wasCulling);

    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    glActiveTexture(GL_TEXTURE0);
    glDepthMask(GL_TRUE);

    if (lastShader == state.defaultShader) {
      lovrGraphicsSetShader(lastShader);
      lovrRelease(&lastShader->ref);
    }
  } else if (skybox->type == SKYBOX_PANORAMA) {
    int resolution = 40;
    int idx = 0;
    float sphere[8610]; // (resolution + 1) * (resolution + 1) * 5

    for (int i = 0; i <= resolution; i++) {
      float theta = i * M_PI / (float) resolution;
      float sinTheta = sin(theta);
      float cosTheta = cos(theta);

      for (int j = 0; j <= resolution; j++) {
        float phi = j * 2 * M_PI / (float) resolution;
        float sinPhi = sin(phi);
        float cosPhi = cos(phi);

        float x = sinPhi * sinTheta;
        float y = cosTheta;
        float z = -cosPhi * sinTheta;
        float u = j / (float) resolution;
        float v = i / (float) resolution;

        sphere[idx++] = x;
        sphere[idx++] = y;
        sphere[idx++] = z;
        sphere[idx++] = u;
        sphere[idx++] = v;
      }
    }

    idx = 0;
    unsigned int indices[9600]; // resolution * resolution * 6
    for (int i = 0; i < resolution; i++) {
      unsigned int offset0 = i * (resolution + 1);
      unsigned int offset1 = (i + 1) * (resolution + 1);
      for (int j = 0; j < resolution; j++) {
        unsigned int index0 = offset0 + j;
        unsigned int index1 = offset1 + j;
        indices[idx++] = index0;
        indices[idx++] = index1;
        indices[idx++] = index0 + 1;
        indices[idx++] = index1;
        indices[idx++] = index1 + 1;
        indices[idx++] = index0 + 1;
      }
    }

    Texture* oldTexture = lovrGraphicsGetTexture();
    glBindTexture(GL_TEXTURE_2D, skybox->texture);

    lovrGraphicsSetShapeData(sphere, 8610);
    lovrGraphicsSetIndexData(indices, 9600);
    glDepthMask(GL_FALSE);
    lovrGraphicsDrawPrimitive(GL_TRIANGLES, 0, 1, 1);
    glDepthMask(GL_TRUE);

    glBindTexture(GL_TEXTURE_2D, oldTexture->id);
  }

  lovrGraphicsPop();
}

void lovrGraphicsPrint(const char* str, mat4 transform, float wrap, HorizontalAlign halign, VerticalAlign valign) {
  Shader* lastShader = lovrGraphicsGetShader();
  if (lastShader == state.defaultShader) {
    lovrRetain(&lastShader->ref);
    lovrGraphicsSetShader(state.fontShader);
  }

  Font* font = lovrGraphicsGetFont();
  float scale = 1 / font->pixelDensity;
  float offsety;
  lovrFontRender(font, str, wrap, halign, valign, &state.buffer.data, &offsety);

  lovrGraphicsPush();
  lovrGraphicsMatrixTransform(transform);
  lovrGraphicsScale(scale, scale, scale);
  lovrGraphicsTranslate(0, offsety, 0);
  lovrGraphicsBindTexture(font->texture);
  lovrGraphicsDrawPrimitive(GL_TRIANGLES, 0, 1, 0);
  lovrGraphicsPop();

  if (lastShader == state.defaultShader) {
    lovrGraphicsSetShader(lastShader);
    lovrRelease(&lastShader->ref);
  }
}

// Internal State
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
