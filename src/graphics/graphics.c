#include "graphics/graphics.h"
#include "loaders/texture.h"
#include "loaders/font.h"
#include "event/event.h"
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
#else
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  glfwWindowHint(GLFW_SAMPLES, 4);
#endif

  state.window = glfwCreateWindow(800, 600, "Window", NULL, NULL);

  if (!state.window) {
    glfwTerminate();
    error("Could not create window");
  }

  glfwMakeContextCurrent(state.window);
  glfwSetWindowCloseCallback(state.window, onCloseWindow);

#ifdef EMSCRIPTEN
  gladLoadGLES2Loader((GLADloadproc) glfwGetProcAddress);
#else
  gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);
#endif

  glfwSetTime(0);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 2);

  if (GLAD_GL_VERSION_3_0) {
    glfwSwapInterval(0);
    glEnable(GL_LINE_SMOOTH);
    glEnable(GL_MULTISAMPLE);
  }

  // Allocations
  state.activeFont = NULL;
  state.defaultFont = NULL;
  state.activeTexture = NULL;
  glGenBuffers(1, &state.shapeBuffer);
  glGenBuffers(1, &state.shapeIndexBuffer);
  if (GLAD_GL_VERSION_3_0) {
    glGenVertexArrays(1, &state.shapeArray);
  }
  vec_init(&state.shapeData);
  vec_init(&state.shapeIndices);
  for (int i = 0; i < MAX_CANVASES; i++) {
    state.canvases[i] = malloc(sizeof(CanvasState));
  }

  // Objects
  state.defaultShader = lovrShaderCreate(lovrDefaultVertexShader, lovrDefaultFragmentShader);
  state.skyboxShader = lovrShaderCreate(lovrSkyboxVertexShader, lovrSkyboxFragmentShader);
  int uniformId = lovrShaderGetUniformId(state.skyboxShader, "cube");
  lovrShaderSendInt(state.skyboxShader, uniformId, 1);
  state.fullscreenShader = lovrShaderCreate(lovrNoopVertexShader, lovrDefaultFragmentShader);
  state.defaultTexture = lovrTextureCreate(lovrTextureDataGetBlank(1, 1, 0xff, FORMAT_RGBA));

  // System Limits
  if (GLAD_GL_VERSION_2_0) {
    float pointSizes[2];
    glGetFloatv(GL_POINT_SIZE_RANGE, pointSizes);
    state.maxPointSize = pointSizes[1];
    glGetIntegerv(GL_MAX_SAMPLES, &state.maxTextureMSAA);
  } else {
    state.maxPointSize = 1.f;
    state.maxTextureMSAA = 1;
  }

  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &state.maxTextureSize);

  // State
  state.depthTest = -1;
  lovrGraphicsReset();
  atexit(lovrGraphicsDestroy);
}

void lovrGraphicsDestroy() {
  lovrGraphicsSetFont(NULL);
  lovrGraphicsSetShader(NULL);
  glUseProgram(0);
  for (int i = 0; i < MAX_CANVASES; i++) {
    free(state.canvases[i]);
  }
  if (state.defaultFont) {
    lovrRelease(&state.defaultFont->ref);
  }
  lovrRelease(&state.defaultShader->ref);
  lovrRelease(&state.skyboxShader->ref);
  lovrRelease(&state.fullscreenShader->ref);
  lovrRelease(&state.defaultTexture->ref);
  glDeleteBuffers(1, &state.shapeBuffer);
  glDeleteBuffers(1, &state.shapeIndexBuffer);
  if (GLAD_GL_VERSION_3_0) {
    glDeleteVertexArrays(1, &state.shapeArray);
  }
  vec_deinit(&state.shapeData);
  vec_deinit(&state.shapeIndices);
}

void lovrGraphicsReset() {
  state.transform = 0;
  state.canvas = 0;
  lovrGraphicsSetViewport(0, 0, lovrGraphicsGetWidth(), lovrGraphicsGetHeight());
  lovrGraphicsSetPerspective(.1f, 100.f, 67 * M_PI / 180);
  lovrGraphicsSetShader(NULL);
  lovrGraphicsBindTexture(NULL);
  lovrGraphicsSetBackgroundColor(0, 0, 0, 0);
  lovrGraphicsSetBlendMode(BLEND_ALPHA, BLEND_ALPHA_MULTIPLY);
  lovrGraphicsSetColor(255, 255, 255, 255);
  lovrGraphicsSetColorMask(1, 1, 1, 1);
  lovrGraphicsSetScissorEnabled(0);
  lovrGraphicsSetLineWidth(1);
  lovrGraphicsSetPointSize(1);
  lovrGraphicsSetCullingEnabled(0);
  lovrGraphicsSetPolygonWinding(POLYGON_WINDING_COUNTERCLOCKWISE);
  lovrGraphicsSetDepthTest(COMPARE_LESS);
  lovrGraphicsSetWireframe(0);
  lovrGraphicsOrigin();
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
  glfwSwapBuffers(state.window);
}

void lovrGraphicsPrepare() {
  Shader* shader = lovrGraphicsGetShader();
  mat4 transform = state.transforms[state.transform];
  mat4 projection = state.canvases[state.canvas]->projection;
  lovrShaderBind(shader, transform, projection, state.color, 0);
}

// State

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
      if (GLAD_GL_VERSION_2_0 || GLAD_GL_EXT_blend_minmax) {
        glBlendEquation(GL_MAX);
        glBlendFuncSeparate(srcRGB, GL_ZERO, GL_ONE, GL_ZERO);
      } else {
        error("The lighten blend mode is not supported on this platform.");
      }
      break;

    case BLEND_DARKEN:
      if (GLAD_GL_VERSION_2_0 || GLAD_GL_EXT_blend_minmax) {
        glBlendEquation(GL_MIN);
        glBlendFuncSeparate(srcRGB, GL_ZERO, GL_ONE, GL_ZERO);
      } else {
        error("The darken blend mode is not supported on this platform.");
      }
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

int lovrGraphicsIsScissorEnabled() {
  return state.isScissorEnabled;
}

void lovrGraphicsSetScissorEnabled(int isEnabled) {
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
  glfwGetFramebufferSize(state.window, &windowWidth, &windowHeight);
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

  if (shader != state.activeShader) {
    if (state.activeShader) {
      lovrRelease(&state.activeShader->ref);
    }

    state.activeShader = shader;
    lovrRetain(&state.activeShader->ref);
  }
}

void lovrGraphicsEnsureFont() {
  if (!state.activeFont && !state.defaultFont) {
    FontData* fontData = lovrFontDataCreate(NULL, 32);
    state.defaultFont = lovrFontCreate(fontData);
    lovrGraphicsSetFont(state.defaultFont);
  }
}

Font* lovrGraphicsGetFont() {
  lovrGraphicsEnsureFont();
  return state.activeFont;
}

void lovrGraphicsSetFont(Font* font) {
  if (state.activeFont) {
    lovrRelease(&state.activeFont->ref);
  }

  state.activeFont = font;

  if (font) {
    lovrRetain(&state.activeFont->ref);
  }
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

mat4 lovrGraphicsGetProjection() {
  return state.canvases[state.canvas]->projection;
}

void lovrGraphicsSetProjection(mat4 projection) {
  memcpy(state.canvases[state.canvas]->projection, projection, 16 * sizeof(float));
}

void lovrGraphicsSetPerspective(float near, float far, float fov) {
  int width, height;
  glfwGetWindowSize(state.window, &width, &height);
  mat4_perspective(state.canvases[state.canvas]->projection, near, far, fov, (float) width / height);
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
  if (GLAD_GL_VERSION_2_0) {
    state.pointSize = size;
    glPointSize(size);
  }
}

int lovrGraphicsIsCullingEnabled() {
  return state.isCullingEnabled;
}

void lovrGraphicsSetCullingEnabled(int isEnabled) {
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

int lovrGraphicsIsWireframe() {
  return state.isWireframe;
}

void lovrGraphicsSetWireframe(int wireframe) {
  if (GLAD_GL_VERSION_3_0 && state.isWireframe != wireframe) {
    state.isWireframe = wireframe;
    glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
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

float lovrGraphicsGetSystemLimit(GraphicsLimit limit) {
  switch (limit) {
    case LIMIT_POINT_SIZE: return state.maxPointSize;
    case LIMIT_TEXTURE_SIZE: return state.maxTextureSize;
    case LIMIT_TEXTURE_MSAA: return state.maxTextureMSAA;
    default: error("Unknown limit %d\n", limit);
  }
}

void lovrGraphicsPushCanvas() {
  if (++state.canvas >= MAX_CANVASES) {
    error("Canvas overflow");
  }

  memcpy(state.canvases[state.canvas], state.canvases[state.canvas - 1], sizeof(CanvasState));
}

void lovrGraphicsPopCanvas() {
  if (--state.canvas < 0) {
    error("Canvas underflow");
  }

  int* viewport = state.canvases[state.canvas]->viewport;
  lovrGraphicsSetViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
  lovrGraphicsBindFramebuffer(state.canvases[state.canvas]->framebuffer);
}

void lovrGraphicsSetViewport(int x, int y, int w, int h) {
  state.canvases[state.canvas]->viewport[0] = x;
  state.canvases[state.canvas]->viewport[1] = y;
  state.canvases[state.canvas]->viewport[2] = w;
  state.canvases[state.canvas]->viewport[3] = h;
  glViewport(x, y, w, h);
}

void lovrGraphicsBindFramebuffer(int framebuffer) {
  state.canvases[state.canvas]->framebuffer = framebuffer;
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
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

// Primitives

void lovrGraphicsSetShapeData(float* data, int length) {
  vec_clear(&state.shapeData);
  vec_pusharr(&state.shapeData, data, length);
}

void lovrGraphicsSetIndexData(unsigned int* data, int length) {
  vec_clear(&state.shapeIndices);
  vec_pusharr(&state.shapeIndices, data, length);
}

void lovrGraphicsDrawPrimitive(GLenum mode, int hasNormals, int hasTexCoords, int useIndices) {
  int stride = 3 + (hasNormals ? 3 : 0) + (hasTexCoords ? 2 : 0);
  int strideBytes = stride * sizeof(float);

  lovrGraphicsPrepare();
  if (GLAD_GL_VERSION_3_0) {
    glBindVertexArray(state.shapeArray);
  }
  glBindBuffer(GL_ARRAY_BUFFER, state.shapeBuffer);
  glBufferData(GL_ARRAY_BUFFER, state.shapeData.length * sizeof(float), state.shapeData.data, GL_STREAM_DRAW);
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
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state.shapeIndexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, state.shapeIndices.length * sizeof(unsigned int), state.shapeIndices.data, GL_STREAM_DRAW);
    glDrawElements(mode, state.shapeIndices.length, GL_UNSIGNED_INT, NULL);
  } else {
    glDrawArrays(mode, 0, state.shapeData.length / stride);
  }

  if (GLAD_GL_VERSION_3_0) {
    glBindVertexArray(0);
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

void lovrGraphicsPlane(DrawMode mode, Texture* texture, float x, float y, float z, float size, float nx, float ny, float nz) {

  // Normalize the normal vector
  float len = sqrt(nx * nx + ny * ny + nz + nz);
  if (len != 0) {
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

  float transform[16];
  mat4_setTransform(transform, x, y, z, size, theta, cx, cy, cz);

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

void lovrGraphicsCube(DrawMode mode, Texture* texture, mat4 transform) {
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
  lovrGraphicsEnsureFont();
  lovrFontPrint(state.activeFont, str, transform, wrap, halign, valign);
}
