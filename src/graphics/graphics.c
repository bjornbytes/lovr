#include "graphics/graphics.h"
#include "data/textureData.h"
#include "data/rasterizer.h"
#include "resources/shaders.h"
#include "event/event.h"
#include "filesystem/filesystem.h"
#include "math/math.h"
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
    EventData data = { .quit = { false, 0 } };
    Event event = { .type = type, .data = data };
    lovrEventPush(event);
  }
}

static void gammaCorrectColor(Color* color) {
  if (state.gammaCorrect) {
    color->r = lovrMathGammaToLinear(color->r);
    color->g = lovrMathGammaToLinear(color->g);
    color->b = lovrMathGammaToLinear(color->b);
  }
}

// Base

void lovrGraphicsInit() {
  //
}

void lovrGraphicsDestroy() {
  if (!state.initialized) return;
  lovrGraphicsSetShader(NULL);
  lovrGraphicsSetFont(NULL);
  for (int i = 0; i < DEFAULT_SHADER_COUNT; i++) {
    lovrRelease(state.defaultShaders[i]);
  }
  lovrRelease(state.defaultMaterial);
  lovrRelease(state.defaultFont);
  lovrRelease(state.defaultTexture);
  vec_deinit(&state.streamData);
  vec_deinit(&state.streamIndices);
  glDeleteVertexArrays(1, &state.streamVAO);
  glDeleteBuffers(1, &state.streamVBO);
  glDeleteBuffers(1, &state.streamIBO);
  glDeleteBuffers(1, &state.cameraUBO);
  memset(&state, 0, sizeof(GraphicsState));
}

void lovrGraphicsReset() {
  int w = lovrGraphicsGetWidth();
  int h = lovrGraphicsGetHeight();
  state.transform = 0;
  state.layer = 0;
  state.defaultShader = SHADER_DEFAULT;
  mat4_perspective(state.layers[state.layer].projections, .01f, 100.f, 67 * M_PI / 180., (float) w / h / 2.);
  mat4_perspective(state.layers[state.layer].projections + 16, .01f, 100.f, 67 * M_PI / 180., (float) w / h / 2.);
  state.layers[state.layer].viewport[0] = 0;
  state.layers[state.layer].viewport[1] = 0;
  state.layers[state.layer].viewport[2] = w;
  state.layers[state.layer].viewport[3] = h;
  lovrGraphicsSetBackgroundColor((Color) { 0, 0, 0, 1. });
  lovrGraphicsSetBlendMode(BLEND_ALPHA, BLEND_ALPHA_MULTIPLY);
  lovrGraphicsSetColor((Color) { 1., 1., 1., 1. });
  lovrGraphicsSetCullingEnabled(false);
  lovrGraphicsSetDefaultFilter((TextureFilter) { .mode = FILTER_TRILINEAR });
  lovrGraphicsSetDepthTest(COMPARE_LEQUAL, true);
  lovrGraphicsSetFont(NULL);
  lovrGraphicsSetLineWidth(1);
  lovrGraphicsSetPointSize(1);
  lovrGraphicsSetShader(NULL);
  lovrGraphicsSetStencilTest(COMPARE_NONE, 0);
  lovrGraphicsSetWinding(WINDING_COUNTERCLOCKWISE);
  lovrGraphicsSetWireframe(false);
  lovrGraphicsSetViewport(0, 0, w, h);
  lovrGraphicsOrigin();
}

void lovrGraphicsClear(bool clearColor, bool clearDepth, bool clearStencil, Color color, float depth, int stencil) {
  if (clearColor) {
    gammaCorrectColor(&color);
    float c[4] = { color.r, color.g, color.b, color.a };
    glClearBufferfv(GL_COLOR, 0, c);
    for (int i = 1; i < state.canvasCount; i++) {
      glClearBufferfv(GL_COLOR, i, c);
    }
  }

  if (clearDepth) {
    glClearBufferfv(GL_DEPTH, 0, &depth);
  }

  if (clearStencil) {
    glClearBufferiv(GL_STENCIL, 0, &stencil);
  }
}

void lovrGraphicsPresent() {
  glfwSwapBuffers(state.window);
  state.stats.drawCalls = 0;
  state.stats.shaderSwitches = 0;
}

void lovrGraphicsPrepare(Material* material, float* pose) {
  Shader* shader = lovrGraphicsGetActiveShader();

  if (!shader) {
    shader = state.defaultShaders[state.defaultShader] = lovrShaderCreateDefault(state.defaultShader);
  }

  mat4 model = state.transforms[state.transform][MATRIX_MODEL];
  lovrShaderSetMatrix(shader, "lovrModel", model, 16);

  float* views = state.layers[state.layer].views;
  float transforms[32];
  mat4_multiply(mat4_set(transforms + 0, views + 0), model);
  mat4_multiply(mat4_set(transforms + 16, views + 16), model);
  lovrShaderSetMatrix(shader, "lovrTransforms", transforms, 32);

  if (lovrShaderGetUniform(shader, "lovrNormalMatrix")) {
    if (mat4_invert(transforms)) {
      mat4_transpose(transforms);
    } else {
      mat4_identity(transforms);
    }

    float normalMatrix[9] = {
      transforms[0], transforms[1], transforms[2],
      transforms[4], transforms[5], transforms[6],
      transforms[8], transforms[9], transforms[10]
    };

    lovrShaderSetMatrix(shader, "lovrNormalMatrix", normalMatrix, 9);
  }

  // Color
  Color color = state.color;
  gammaCorrectColor(&color);
  float data[4] = { color.r, color.g, color.b, color.a };
  lovrShaderSetFloat(shader, "lovrColor", data, 4);

  // Point size
  lovrShaderSetFloat(shader, "lovrPointSize", &state.pointSize, 1);

  // Pose
  if (pose) {
    lovrShaderSetMatrix(shader, "lovrPose", pose, MAX_BONES * 16);
  } else {
    float identity[16];
    mat4_identity(identity);
    lovrShaderSetMatrix(shader, "lovrPose", identity, 16);
  }

  // Material
  if (!material) {
    material = lovrGraphicsGetDefaultMaterial();
  }

  for (int i = 0; i < MAX_MATERIAL_SCALARS; i++) {
    float value = lovrMaterialGetScalar(material, i);
    lovrShaderSetFloat(shader, lovrShaderScalarUniforms[i], &value, 1);
  }

  for (int i = 0; i < MAX_MATERIAL_COLORS; i++) {
    Color color = lovrMaterialGetColor(material, i);
    gammaCorrectColor(&color);
    float data[4] = { color.r, color.g, color.b, color.a };
    lovrShaderSetFloat(shader, lovrShaderColorUniforms[i], data, 4);
  }

  for (int i = 0; i < MAX_MATERIAL_TEXTURES; i++) {
    Texture* texture = lovrMaterialGetTexture(material, i);
    lovrShaderSetTexture(shader, lovrShaderTextureUniforms[i], &texture, 1);
  }

  lovrGraphicsUseProgram(shader->program);
  lovrShaderBind(shader);
}

void lovrGraphicsCreateWindow(int w, int h, bool fullscreen, int msaa, const char* title, const char* icon) {
  lovrAssert(!state.window, "Window is already created");

  if ((state.window = glfwGetCurrentContext()) == NULL) {
#ifdef EMSCRIPTEN
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_SAMPLES, msaa);
    glfwWindowHint(GLFW_SRGB_CAPABLE, state.gammaCorrect);
#else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_SAMPLES, msaa);
    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
    glfwWindowHint(GLFW_SRGB_CAPABLE, state.gammaCorrect);
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
  }

#ifndef EMSCRIPTEN
  gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);
  glfwSwapInterval(0);
  glEnable(GL_LINE_SMOOTH);
  glEnable(GL_PROGRAM_POINT_SIZE);
  glEnable(GL_CLIP_DISTANCE0);
  if (state.gammaCorrect) {
    glEnable(GL_FRAMEBUFFER_SRGB);
  } else {
    glDisable(GL_FRAMEBUFFER_SRGB);
  }
#endif
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  vec_init(&state.streamData);
  vec_init(&state.streamIndices);
  glGenVertexArrays(1, &state.streamVAO);
  glGenBuffers(1, &state.streamVBO);
  glGenBuffers(1, &state.streamIBO);
  glGenBuffers(1, &state.cameraUBO);
  glBindBuffer(GL_UNIFORM_BUFFER, state.cameraUBO);
  glBufferData(GL_UNIFORM_BUFFER, 4 * 16 * sizeof(float), NULL, GL_DYNAMIC_DRAW);
  glBindBufferBase(GL_UNIFORM_BUFFER, LOVR_SHADER_BLOCK_CAMERA, state.cameraUBO);
  lovrGraphicsReset();
  state.initialized = true;
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

GraphicsStats lovrGraphicsGetStats() {
  return state.stats;
}

// State

Color lovrGraphicsGetBackgroundColor() {
  return state.backgroundColor;
}

void lovrGraphicsSetBackgroundColor(Color color) {
  state.backgroundColor = color;
  gammaCorrectColor(&color);
  glClearColor(color.r, color.g, color.b, color.a);
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

void lovrGraphicsGetCanvas(Canvas** canvas, int* count) {
  *count = state.canvasCount;
  memcpy(canvas, state.canvas, state.canvasCount * sizeof(Canvas*));
}

void lovrGraphicsSetCanvas(Canvas** canvas, int count) {
  if (count == state.canvasCount && !memcmp(state.canvas, canvas, count * sizeof(Canvas*))) {
    return;
  }

  lovrAssert(count <= MAX_CANVASES, "Attempt to simultaneously render to %d canvases (the maximum is %d)", count, MAX_CANVASES);

  if (state.canvasCount > 0 && state.canvas[0]->msaa > 0) {
    int width = state.canvas[0]->texture.width;
    int height = state.canvas[0]->texture.height;
    glBindFramebuffer(GL_READ_FRAMEBUFFER, state.canvas[0]->framebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, state.canvas[0]->resolveFramebuffer);
    glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_LINEAR);
  }

  for (int i = 0; i < count; i++) {
    lovrRetain(&canvas[i]->texture);
  }

  for (int i = 0; i < state.canvasCount; i++) {
    lovrRelease(&state.canvas[i]->texture);
  }

  if (count == 0) {
    Layer layer = state.layers[state.layer];
    int* viewport = layer.viewport;
    lovrGraphicsBindFramebuffer(layer.canvas ? layer.canvas->texture.id : 0);
    lovrGraphicsSetViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
  } else {
    memcpy(state.canvas, canvas, count * sizeof(Canvas*));
    lovrGraphicsBindFramebuffer(canvas[0]->framebuffer);
    lovrGraphicsSetViewport(0, 0, canvas[0]->texture.width, canvas[0]->texture.height);

    GLenum buffers[MAX_CANVASES];
    for (int i = 0; i < count; i++) {
      buffers[i] = GL_COLOR_ATTACHMENT0 + i;
      glFramebufferTexture2D(GL_FRAMEBUFFER, buffers[i], GL_TEXTURE_2D, canvas[i]->texture.id, 0);
    }
    glDrawBuffers(count, buffers);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    lovrAssert(status != GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS, "All multicanvas canvases must have the same dimensions");
    lovrAssert(status == GL_FRAMEBUFFER_COMPLETE, "Unable to bind framebuffer");
  }

  state.canvasCount = count;
}

Color lovrGraphicsGetColor() {
  return state.color;
}

void lovrGraphicsSetColor(Color color) {
  state.color = color;
}

bool lovrGraphicsIsCullingEnabled() {
  return state.culling;
}

void lovrGraphicsSetCullingEnabled(bool culling) {
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

void lovrGraphicsGetDepthTest(CompareMode* mode, bool* write) {
  *mode = state.depthTest;
  *write = state.depthWrite;
}

void lovrGraphicsSetDepthTest(CompareMode depthTest, bool write) {
  if (state.depthTest != depthTest) {
    state.depthTest = depthTest;
    if (depthTest != COMPARE_NONE) {
      glDepthFunc(depthTest);
      glEnable(GL_DEPTH_TEST);
    } else {
      glDisable(GL_DEPTH_TEST);
    }
  }

  if (state.depthWrite != write) {
    state.depthWrite = write;
    glDepthMask(write);
  }
}

Font* lovrGraphicsGetFont() {
  if (!state.font) {
    if (!state.defaultFont) {
      Rasterizer* rasterizer = lovrRasterizerCreate(NULL, 32);
      state.defaultFont = lovrFontCreate(rasterizer);
    }

    lovrGraphicsSetFont(state.defaultFont);
  }

  return state.font;
}

void lovrGraphicsSetFont(Font* font) {
  lovrRetain(font);
  lovrRelease(state.font);
  state.font = font;
}

bool lovrGraphicsIsGammaCorrect() {
  return state.gammaCorrect;
}

void lovrGraphicsSetGammaCorrect(bool gammaCorrect) {
  state.gammaCorrect = gammaCorrect;
}

GraphicsLimits lovrGraphicsGetLimits() {
  if (!state.limits.initialized) {
#ifdef EMSCRIPTEN
    glGetFloatv(GL_ALIASED_POINT_SIZE_RANGE, state.limits.pointSizes);
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
  state.pointSize = size;
}

Shader* lovrGraphicsGetShader() {
  return state.shader;
}

void lovrGraphicsSetShader(Shader* shader) {
  if (shader != state.shader) {
    lovrRetain(shader);
    lovrRelease(state.shader);
    state.shader = shader;
  }
}

void lovrGraphicsGetStencilTest(CompareMode* mode, int* value) {
  *mode = state.stencilMode;
  *value = state.stencilValue;
}

void lovrGraphicsSetStencilTest(CompareMode mode, int value) {
  state.stencilMode = mode;
  state.stencilValue = value;

  if (state.stencilWriting) {
    return;
  }

  if (mode != COMPARE_NONE) {
    if (!state.stencilEnabled) {
      glEnable(GL_STENCIL_TEST);
      state.stencilEnabled = true;
    }

    GLenum glMode = mode;
    switch (mode) {
      case COMPARE_LESS: glMode = GL_GREATER; break;
      case COMPARE_LEQUAL: glMode = GL_GEQUAL; break;
      case COMPARE_GEQUAL: glMode = GL_LEQUAL; break;
      case COMPARE_GREATER: glMode = GL_LESS; break;
      default: break;
    }

    glStencilFunc(glMode, value, 0xff);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
  } else if (state.stencilEnabled) {
    glDisable(GL_STENCIL_TEST);
    state.stencilEnabled = false;
  }
}

Winding lovrGraphicsGetWinding() {
  return state.winding;
}

void lovrGraphicsSetWinding(Winding winding) {
  if (winding != state.winding) {
    state.winding = winding;
    glFrontFace(winding);
  }
}

bool lovrGraphicsIsWireframe() {
  return state.wireframe;
}

void lovrGraphicsSetWireframe(bool wireframe) {
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

static void lovrGraphicsSetStreamData(float* data, int length) {
  vec_clear(&state.streamData);
  vec_pusharr(&state.streamData, data, length);
}

static void lovrGraphicsSetIndexData(unsigned int* data, int length) {
  vec_clear(&state.streamIndices);
  vec_pusharr(&state.streamIndices, data, length);
}

static void lovrGraphicsDrawPrimitive(Material* material, GLenum mode, bool hasNormals, bool hasTexCoords, bool useIndices) {
  int stride = 3 + (hasNormals ? 3 : 0) + (hasTexCoords ? 2 : 0);
  int strideBytes = stride * sizeof(float);
  float* data = state.streamData.data;
  unsigned int* indices = state.streamIndices.data;

  lovrGraphicsPrepare(material, NULL);
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

  glDisableVertexAttribArray(LOVR_SHADER_BONES);
  glDisableVertexAttribArray(LOVR_SHADER_BONE_WEIGHTS);

  if (useIndices) {
    lovrGraphicsBindIndexBuffer(state.streamIBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, state.streamIndices.length * sizeof(unsigned int), indices, GL_STREAM_DRAW);
    lovrGraphicsDrawElements(mode, state.streamIndices.length, sizeof(uint32_t), 0, 1);
  } else {
    lovrGraphicsDrawArrays(mode, 0, state.streamData.length / stride, 1);
  }
}

void lovrGraphicsPoints(float* points, int count) {
  lovrGraphicsSetDefaultShader(SHADER_DEFAULT);
  lovrGraphicsSetStreamData(points, count);
  lovrGraphicsDrawPrimitive(NULL, MESH_POINTS, false, false, false);
}

void lovrGraphicsLine(float* points, int count) {
  lovrGraphicsSetDefaultShader(SHADER_DEFAULT);
  lovrGraphicsSetStreamData(points, count);
  lovrGraphicsDrawPrimitive(NULL, GL_LINE_STRIP, false, false, false);
}

void lovrGraphicsTriangle(DrawMode mode, Material* material, float* points) {
  lovrGraphicsSetDefaultShader(SHADER_DEFAULT);

  if (mode == DRAW_MODE_LINE) {
    lovrGraphicsSetStreamData(points, 9);
    lovrGraphicsDrawPrimitive(material, GL_LINE_LOOP, false, false, false);
  } else {
    float normal[3];
    vec3_cross(vec3_init(normal, &points[0]), &points[3]);

    float data[18] = {
      points[0], points[1], points[2], normal[0], normal[1], normal[2],
      points[3], points[4], points[5], normal[0], normal[1], normal[2],
      points[6], points[7], points[8], normal[0], normal[1], normal[2]
    };

    lovrGraphicsSetStreamData(data, 18);
    lovrGraphicsDrawPrimitive(material, GL_TRIANGLE_STRIP, true, false, false);
  }
}

void lovrGraphicsPlane(DrawMode mode, Material* material, mat4 transform) {
  lovrGraphicsPush();
  lovrGraphicsMatrixTransform(MATRIX_MODEL, transform);

  if (mode == DRAW_MODE_LINE) {
    float points[] = {
      -.5, .5, 0,
      .5, .5, 0,
      .5, -.5, 0,
      -.5, -.5, 0
    };

    lovrGraphicsSetDefaultShader(SHADER_DEFAULT);
    lovrGraphicsSetStreamData(points, 12);
    lovrGraphicsDrawPrimitive(material, GL_LINE_LOOP, false, false, false);
  } else if (mode == DRAW_MODE_FILL) {
    float data[] = {
      -.5, .5, 0,  0, 0, -1, 0, 1,
      -.5, -.5, 0, 0, 0, -1, 0, 0,
      .5, .5, 0,   0, 0, -1, 1, 1,
      .5, -.5, 0,  0, 0, -1, 1, 0
    };

    lovrGraphicsSetDefaultShader(SHADER_DEFAULT);
    lovrGraphicsSetStreamData(data, 32);
    lovrGraphicsDrawPrimitive(material, GL_TRIANGLE_STRIP, true, true, false);
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

  lovrGraphicsSetDefaultShader(SHADER_FULLSCREEN);
  Material* material = lovrGraphicsGetDefaultMaterial();
  lovrMaterialSetTexture(material, TEXTURE_DIFFUSE, texture);
  lovrGraphicsSetStreamData(data, 20);
  lovrGraphicsDrawPrimitive(material, GL_TRIANGLE_STRIP, false, true, false);
  lovrMaterialSetTexture(material, TEXTURE_DIFFUSE, NULL);
}

void lovrGraphicsBox(DrawMode mode, Material* material, mat4 transform) {
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

    lovrGraphicsSetDefaultShader(SHADER_DEFAULT);
    lovrGraphicsSetStreamData(points, 24);
    lovrGraphicsSetIndexData(indices, 24);
    lovrGraphicsDrawPrimitive(material, GL_LINES, false, false, true);
  } else {
    float data[] = {
      // Front
      -.5, -.5, -.5,  0, 0, -1, 0, 0,
      -.5, .5, -.5,   0, 0, -1, 0, 1,
      .5, -.5, -.5,   0, 0, -1, 1, 0,
      .5, .5, -.5,    0, 0, -1, 1, 1,

      // Right
      .5, .5, -.5,    1, 0, 0,  0, 1,
      .5, .5, .5,     1, 0, 0,  1, 1,
      .5, -.5, -.5,   1, 0, 0,  0, 0,
      .5, -.5, .5,    1, 0, 0,  1, 0,

      // Back
      .5, -.5, .5,    0, 0, 1,  0, 0,
      .5, .5, .5,     0, 0, 1,  0, 1,
      -.5, -.5, .5,   0, 0, 1,  1, 0,
      -.5, .5, .5,    0, 0, 1,  1, 1,

      // Left
      -.5, .5, .5,   -1, 0, 0,  0, 1,
      -.5, .5, -.5,  -1, 0, 0,  1, 1,
      -.5, -.5, .5,  -1, 0, 0,  0, 0,
      -.5, -.5, -.5, -1, 0, 0,  1, 0,

      // Bottom
      -.5, -.5, -.5,  0, -1, 0, 0, 0,
      .5, -.5, -.5,   0, -1, 0, 1, 0,
      -.5, -.5, .5,   0, -1, 0, 0, 1,
      .5, -.5, .5,    0, -1, 0, 1, 1,

      // Adjust
      .5, -.5, .5,    0, 1, 0,  0, 1,
      -.5, .5, -.5,   0, 1, 0,  0, 1,

      // Top
      -.5, .5, -.5,   0, 1, 0,  0, 1,
      -.5, .5, .5,    0, 1, 0,  0, 0,
      .5, .5, -.5,    0, 1, 0,  1, 1,
      .5, .5, .5,     0, 1, 0,  1, 0
    };

    lovrGraphicsSetDefaultShader(SHADER_DEFAULT);
    lovrGraphicsSetStreamData(data, 208);
    lovrGraphicsDrawPrimitive(material, GL_TRIANGLE_STRIP, true, true, false);
  }

  lovrGraphicsPop();
}

void lovrGraphicsArc(DrawMode mode, ArcMode arcMode, Material* material, mat4 transform, float theta1, float theta2, int segments) {
  if (fabsf(theta1 - theta2) >= 2 * M_PI) {
    theta1 = 0;
    theta2 = 2 * M_PI;
  }

  lovrGraphicsPush();
  lovrGraphicsMatrixTransform(MATRIX_MODEL, transform);

  vec_clear(&state.streamData);

  if (arcMode == ARC_MODE_PIE && fabsf(theta1 - theta2) < 2 * M_PI) {
    vec_push(&state.streamData, 0);
    vec_push(&state.streamData, 0);
    vec_push(&state.streamData, 0);

    if (mode == DRAW_MODE_FILL) {
      vec_push(&state.streamData, 0);
      vec_push(&state.streamData, 0);
      vec_push(&state.streamData, 1);

      vec_push(&state.streamData, .5);
      vec_push(&state.streamData, .5);
    }
  }

  float theta = theta1;
  float angleShift = (theta2 - theta1) / (float) segments;

  if (mode == DRAW_MODE_LINE) {
    for (int i = 0; i <= segments; i++) {
      float x = cos(theta) * .5;
      float y = sin(theta) * .5;
      vec_push(&state.streamData, x);
      vec_push(&state.streamData, y);
      vec_push(&state.streamData, 0);
      theta += angleShift;
    }

    lovrGraphicsSetDefaultShader(SHADER_DEFAULT);
    lovrGraphicsDrawPrimitive(material, arcMode == ARC_MODE_OPEN ? GL_LINE_STRIP : GL_LINE_LOOP, false, false, false);
  } else if (mode == DRAW_MODE_FILL) {
    for (int i = 0; i <= segments; i++) {
      float x = cos(theta) * .5;
      float y = sin(theta) * .5;
      vec_push(&state.streamData, x);
      vec_push(&state.streamData, y);
      vec_push(&state.streamData, 0);

      vec_push(&state.streamData, 0);
      vec_push(&state.streamData, 0);
      vec_push(&state.streamData, 1);

      vec_push(&state.streamData, x + .5);
      vec_push(&state.streamData, 1 - (y + .5));
      theta += angleShift;
    }

    lovrGraphicsSetDefaultShader(SHADER_DEFAULT);
    lovrGraphicsDrawPrimitive(material, GL_TRIANGLE_FAN, true, true, false);
  }

  lovrGraphicsPop();
}

void lovrGraphicsCircle(DrawMode mode, Material* material, mat4 transform, int segments) {
  lovrGraphicsArc(mode, ARC_MODE_OPEN, material, transform, 0, 2 * M_PI, segments);
}

void lovrGraphicsCylinder(Material* material, float x1, float y1, float z1, float x2, float y2, float z2, float r1, float r2, bool capped, int segments) {
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

  lovrGraphicsSetDefaultShader(SHADER_DEFAULT);
  lovrGraphicsDrawPrimitive(material, GL_TRIANGLES, true, false, true);
#undef PUSH_CYLINDER_VERTEX
#undef PUSH_CYLINDER_TRIANGLE
}

void lovrGraphicsSphere(Material* material, mat4 transform, int segments) {
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

      vec_push(&state.streamData, x);
      vec_push(&state.streamData, y);
      vec_push(&state.streamData, z);

      vec_push(&state.streamData, u);
      vec_push(&state.streamData, 1 - v);
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

  if (transform) {
    lovrGraphicsPush();
    lovrGraphicsMatrixTransform(MATRIX_MODEL, transform);
  }

  lovrGraphicsSetDefaultShader(SHADER_DEFAULT);
  lovrGraphicsDrawPrimitive(material, GL_TRIANGLES, true, true, true);

  if (transform) {
    lovrGraphicsPop();
  }
}

void lovrGraphicsSkybox(Texture* texture, float angle, float ax, float ay, float az) {
  Winding winding = state.winding;

  lovrGraphicsPush();
  lovrGraphicsOrigin();
  lovrGraphicsRotate(MATRIX_MODEL, angle, ax, ay, az);
  lovrGraphicsSetWinding(WINDING_COUNTERCLOCKWISE);

  if (texture->type == TEXTURE_CUBE) {
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
      -1.f, 1.f, 1.f,
      1.f, -1.f, 1.f,
      1.f, 1.f, 1.f,

      // Right
      1.f, 1.f, 1.f,
      1.f, 1.f, -1.f,
      1.f, -1.f, 1.f,
      1.f, -1.f, -1.f,

      // Bottom
      1.f, -1.f, -1.f,
      -1.f, -1.f, -1.f,
      1.f, -1.f, 1.f,
      -1.f, -1.f, 1.f,

      // Adjust
      -1.f, -1.f, 1.f,
      -1.f, 1.f, -1.f,

      // Top
      -1.f, 1.f, -1.f,
      1.f, 1.f, -1.f,
      -1.f, 1.f, 1.f,
      1.f, 1.f, 1.f
    };

    lovrGraphicsSetStreamData(cube, 78);
    lovrGraphicsSetDefaultShader(SHADER_SKYBOX);
    Material* material = lovrGraphicsGetDefaultMaterial();
    lovrMaterialSetTexture(material, TEXTURE_ENVIRONMENT_MAP, texture);
    lovrGraphicsDrawPrimitive(material, GL_TRIANGLE_STRIP, false, false, false);
    lovrMaterialSetTexture(material, TEXTURE_ENVIRONMENT_MAP, NULL);
  } else if (texture->type == TEXTURE_2D) {
    CompareMode mode;
    bool write;
    lovrGraphicsGetDepthTest(&mode, &write);
    lovrGraphicsSetDepthTest(COMPARE_LEQUAL, false);
    Material* material = lovrGraphicsGetDefaultMaterial();
    lovrMaterialSetTexture(material, TEXTURE_DIFFUSE, texture);
    lovrGraphicsSphere(material, NULL, 30);
    lovrMaterialSetTexture(material, TEXTURE_DIFFUSE, NULL);
    lovrGraphicsSetDepthTest(mode, write);
  } else {
    lovrThrow("lovr.graphics.skybox only accepts 2d and cube Textures");
  }

  lovrGraphicsSetWinding(winding);
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
  lovrGraphicsSetDefaultShader(SHADER_FONT);
  Material* material = lovrGraphicsGetDefaultMaterial();
  lovrMaterialSetTexture(material, TEXTURE_DIFFUSE, font->texture);
  CompareMode mode;
  bool write;
  lovrGraphicsGetDepthTest(&mode, &write);
  lovrGraphicsSetDepthTest(mode, false);
  lovrGraphicsDrawPrimitive(NULL, GL_TRIANGLES, false, true, false);
  lovrGraphicsSetDepthTest(mode, write);
  lovrMaterialSetTexture(material, TEXTURE_DIFFUSE, NULL);
  lovrGraphicsPop();
}

void lovrGraphicsStencil(StencilAction action, int replaceValue, StencilCallback callback, void* userdata) {
  CompareMode mode;
  bool write;
  lovrGraphicsGetDepthTest(&mode, &write);
  lovrGraphicsSetDepthTest(mode, false);
  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

  if (!state.stencilEnabled) {
    glEnable(GL_STENCIL_TEST);
    state.stencilEnabled = true;
  }

  glStencilFunc(GL_ALWAYS, replaceValue, 0xff);
  glStencilOp(GL_KEEP, GL_KEEP, action);

  state.stencilWriting = true;
  callback(userdata);
  state.stencilWriting = false;

  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  lovrGraphicsSetDepthTest(mode, write);
  lovrGraphicsSetStencilTest(state.stencilMode, state.stencilValue);
}

// Internal State
void lovrGraphicsPushLayer(Layer layer) {
  if (++state.layer >= MAX_LAYERS) {
    lovrThrow("Layer overflow");
  }

  memcpy(&state.layers[state.layer], &layer, sizeof(Layer));

  glBindBuffer(GL_UNIFORM_BUFFER, state.cameraUBO);
  glBufferSubData(GL_UNIFORM_BUFFER, 0, 4 * 16 * sizeof(float), &layer);

  if (state.canvasCount == 0) {
    int* viewport = layer.viewport;
    lovrGraphicsBindFramebuffer(layer.canvas ? layer.canvas->texture.id : 0);
    lovrGraphicsSetViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
  }
}

void lovrGraphicsPopLayer() {
  if (--state.layer < 0) {
    lovrThrow("Layer underflow");
  }

  if (state.canvasCount == 0) {
    Layer layer = state.layers[state.layer];
    int* viewport = layer.viewport;
    lovrGraphicsBindFramebuffer(layer.canvas ? layer.canvas->texture.id : 0);
    lovrGraphicsSetViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
  }
}

void lovrGraphicsSetViewport(int x, int y, int w, int h) {
  glViewport(x, y, w, h);
}

void lovrGraphicsBindFramebuffer(int framebuffer) {
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
}

Texture* lovrGraphicsGetTexture(int slot) {
  return state.textures[slot];
}

void lovrGraphicsBindTexture(Texture* texture, TextureType type, int slot) {
  if (!texture) {
    if (!state.defaultTexture) {
      TextureData* textureData = lovrTextureDataGetBlank(1, 1, 0xff, FORMAT_RGBA);
      state.defaultTexture = lovrTextureCreate(TEXTURE_2D, &textureData, 1, true, false);
    }

    texture = state.defaultTexture;
  }

  if (texture != state.textures[slot]) {
    lovrRetain(texture);
    lovrRelease(state.textures[slot]);
    state.textures[slot] = texture;
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(type, texture->id);
  }
}

Material* lovrGraphicsGetDefaultMaterial() {
  if (!state.defaultMaterial) {
    state.defaultMaterial = lovrMaterialCreate(true);
  }

  return state.defaultMaterial;
}

void lovrGraphicsSetDefaultShader(DefaultShader shader) {
  state.defaultShader = shader;
}

Shader* lovrGraphicsGetActiveShader() {
  return state.shader ? state.shader : state.defaultShaders[state.defaultShader];
}

void lovrGraphicsUseProgram(uint32_t program) {
  if (state.program != program) {
    state.program = program;
    glUseProgram(program);
    state.stats.shaderSwitches++;
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

void lovrGraphicsDrawArrays(GLenum mode, size_t start, size_t count, int instances) {
  glDrawArraysInstanced(mode, start, count, instances * 2);
  state.stats.drawCalls++;
}

void lovrGraphicsDrawElements(GLenum mode, size_t count, size_t indexSize, size_t offset, int instances) {
  GLenum indexType = indexSize == sizeof(uint16_t) ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
  glDrawElementsInstanced(mode, count, indexType, (GLvoid*) offset, instances * 2);
  state.stats.drawCalls++;
}
