#include "graphics/graphics.h"
#include "data/rasterizer.h"
#include "event/event.h"
#include "filesystem/filesystem.h"
#include "math/math.h"
#include "util.h"
#include "lib/math.h"
#include "lib/stb/stb_image.h"
#define _USE_MATH_DEFINES
#include <stdlib.h>
#include <string.h>
#include <math.h>

static GraphicsState state;

static void gammaCorrectColor(Color* color) {
  if (state.gammaCorrect) {
    color->r = lovrMathGammaToLinear(color->r);
    color->g = lovrMathGammaToLinear(color->g);
    color->b = lovrMathGammaToLinear(color->b);
  }
}

static void onCloseWindow() {
  lovrEventPush((Event) { .type = EVENT_QUIT, .data.quit = { false, 0 } });
}

static void onResizeWindow(int width, int height) {
  state.width = width;
  state.height = height;
}

static bool batchable(DrawRequest* a) {
  DrawRequest* b = &state.batch;
  if (a->instances > 1) return false;
  if (a->mesh != b->mesh) return false;
  if (!a->mesh && a->mode != b->mode) return false;
  if (!a->mesh && !!a->index.count != !!b->index.count) return false;
  if (a->shader != b->shader) return false;
  if (a->material != b->material) return false;
  if (!a->material && a->diffuseTexture != b->diffuseTexture) return false;
  if (!a->material && a->environmentMap != b->environmentMap) return false;
  if (a->mono != b->mono) return false;
  if (!!a->pose != !!b->pose || (a->pose && memcmp(a->pose, b->pose, MAX_BONES * 16 * sizeof(float)))) return false;
  if (a->material && lovrMaterialIsDirty(a->material)) return false;
  return true;
}

// Base

bool lovrGraphicsInit(bool gammaCorrect) {
  state.gammaCorrect = gammaCorrect;
  return false;
}

void lovrGraphicsDestroy() {
  if (!state.initialized) return;
  while (state.pipelineIndex > 0) {
    lovrGraphicsPopPipeline();
  }
  lovrGraphicsSetShader(NULL);
  lovrGraphicsSetFont(NULL);
  lovrGraphicsSetCanvas(NULL);
  for (int i = 0; i < MAX_DEFAULT_SHADERS; i++) {
    lovrRelease(state.defaultShaders[i]);
  }
  lovrRelease(state.defaultMaterial);
  lovrRelease(state.defaultFont);
  lovrRelease(state.defaultMesh);
  lovrRelease(state.vertexMap);
  lovrRelease(state.identityBuffer);
  lovrRelease(state.block);
  lovrGpuDestroy();
  memset(&state, 0, sizeof(GraphicsState));
}

void lovrGraphicsPresent() {
  lovrGraphicsFlush();
  lovrPlatformSwapBuffers();
  lovrGpuPresent();
}

void lovrGraphicsSetWindow(WindowFlags* flags) {
  lovrAssert(!state.initialized, "Window is already created");

  if (flags) {
    flags->srgb = state.gammaCorrect;
#ifdef EMSCRIPTEN
    flags->vsync = 1;
#else
    flags->vsync = 0;
#endif
  }

  lovrAssert(lovrPlatformSetWindow(flags), "Could not create window");
  lovrPlatformOnWindowClose(onCloseWindow);
  lovrPlatformOnWindowResize(onResizeWindow);
  lovrPlatformGetFramebufferSize(&state.width, &state.height);
  lovrGpuInit(state.gammaCorrect, lovrGetProcAddress);

  VertexFormat format;
  vertexFormatInit(&format);
  vertexFormatAppend(&format, "lovrPosition", ATTR_FLOAT, 3);
  vertexFormatAppend(&format, "lovrNormal", ATTR_FLOAT, 3);
  vertexFormatAppend(&format, "lovrTexCoord", ATTR_FLOAT, 2);
  state.defaultMesh = lovrMeshCreate(MAX_VERTICES, format, DRAW_TRIANGLES, USAGE_STREAM, false);

  state.vertexMap = lovrBufferCreate(MAX_VERTICES * sizeof(uint8_t), NULL, USAGE_STREAM, false);
  lovrMeshAttachAttribute(state.defaultMesh, "lovrDrawID", &(MeshAttribute) {
    .buffer = state.vertexMap,
    .type = ATTR_BYTE,
    .components = 1,
    .integer = true,
    .enabled = true
  });

  vec_uniform_t uniforms;
  vec_init(&uniforms);
  vec_push(&uniforms, ((Uniform) { .name = "lovrModels", .type = UNIFORM_MATRIX, .components = 4, .count = MAX_BATCH_SIZE }));
  vec_push(&uniforms, ((Uniform) { .name = "lovrColors", .type = UNIFORM_FLOAT, .components = 4, .count = MAX_BATCH_SIZE }));
  state.block = lovrShaderBlockCreate(&uniforms, BLOCK_UNIFORM, USAGE_STREAM);
  vec_deinit(&uniforms);

  uint8_t identity[MAX_BATCH_SIZE];
  for (int i = 0; i < MAX_BATCH_SIZE; i++) { identity[i] = i; }
  state.identityBuffer = lovrBufferCreate(sizeof(identity), identity, USAGE_STATIC, false);

  lovrGraphicsReset();
  state.initialized = true;
}

int lovrGraphicsGetWidth() {
  return state.width;
}

int lovrGraphicsGetHeight() {
  return state.height;
}

void lovrGraphicsSetCamera(Camera* camera, bool clear) {
  lovrGraphicsFlush();

  if (state.camera.canvas && (!camera || camera->canvas != state.camera.canvas)) {
    lovrCanvasResolve(state.camera.canvas);
  }

  if (!camera) {
    state.camera.canvas = NULL;
    state.camera.stereo = false;
    for (int i = 0; i < 2; i++) {
      mat4_identity(state.camera.viewMatrix[i]);
      mat4_perspective(state.camera.projection[i], .01f, 100.f, 67 * M_PI / 180., (float) state.width / state.height);
    }
  } else {
    state.camera = *camera;
  }

  if (clear) {
    lovrGpuClear(state.camera.canvas, &state.backgroundColor, &(float) { 1. }, &(int) { 0 });
  }
}

Buffer* lovrGraphicsGetIdentityBuffer() {
  return state.identityBuffer;
}

// State

void lovrGraphicsReset() {
  while (state.pipelineIndex > 0) {
    lovrGraphicsPopPipeline();
  }
  state.pipeline = &state.pipelines[state.pipelineIndex];
  state.transform = 0;
  lovrGraphicsSetCamera(NULL, false);
  lovrGraphicsSetBackgroundColor((Color) { 0, 0, 0, 1 });
  lovrGraphicsSetBlendMode(BLEND_ALPHA, BLEND_ALPHA_MULTIPLY);
  lovrGraphicsSetCanvas(NULL);
  lovrGraphicsSetColor((Color) { 1, 1, 1, 1 });
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
  lovrGraphicsOrigin();
}

void lovrGraphicsPushPipeline() {
  lovrAssert(++state.pipelineIndex < MAX_PIPELINES, "Unbalanced pipeline stack (more pushes than pops?)");
  memcpy(&state.pipelines[state.pipelineIndex], &state.pipelines[state.pipelineIndex - 1], sizeof(Pipeline));
  state.pipeline = &state.pipelines[state.pipelineIndex];
}

void lovrGraphicsPopPipeline() {
  lovrAssert(--state.pipelineIndex >= 0, "Unbalanced pipeline stack (more pops than pushes?)");
  state.pipeline = &state.pipelines[state.pipelineIndex];
}

bool lovrGraphicsGetAlphaSampling() {
  return state.pipeline->alphaSampling;
}

void lovrGraphicsSetAlphaSampling(bool sample) {
  state.pipeline->alphaSampling = sample;
}

Color lovrGraphicsGetBackgroundColor() {
  return state.backgroundColor;
}

void lovrGraphicsSetBackgroundColor(Color color) {
  state.backgroundColor = color;
}

void lovrGraphicsGetBlendMode(BlendMode* mode, BlendAlphaMode* alphaMode) {
  *mode = state.pipeline->blendMode;
  *alphaMode = state.pipeline->blendAlphaMode;
}

void lovrGraphicsSetBlendMode(BlendMode mode, BlendAlphaMode alphaMode) {
  state.pipeline->blendMode = mode;
  state.pipeline->blendAlphaMode = alphaMode;
}

Canvas* lovrGraphicsGetCanvas() {
  return state.canvas;
}

void lovrGraphicsSetCanvas(Canvas* canvas) {
  if (state.canvas && canvas != state.canvas) {
    lovrCanvasResolve(state.canvas);
  }

  lovrRetain(canvas);
  lovrRelease(state.canvas);
  state.canvas = canvas;
}

Color lovrGraphicsGetColor() {
  return state.color;
}

void lovrGraphicsSetColor(Color color) {
  state.color = color;
}

bool lovrGraphicsIsCullingEnabled() {
  return state.pipeline->culling;
}

void lovrGraphicsSetCullingEnabled(bool culling) {
  state.pipeline->culling = culling;
}

TextureFilter lovrGraphicsGetDefaultFilter() {
  return state.defaultFilter;
}

void lovrGraphicsSetDefaultFilter(TextureFilter filter) {
  state.defaultFilter = filter;
}

void lovrGraphicsGetDepthTest(CompareMode* mode, bool* write) {
  *mode = state.pipeline->depthTest;
  *write = state.pipeline->depthWrite;
}

void lovrGraphicsSetDepthTest(CompareMode mode, bool write) {
  state.pipeline->depthTest = mode;
  state.pipeline->depthWrite = write;
}

Font* lovrGraphicsGetFont() {
  if (!state.font) {
    if (!state.defaultFont) {
      Rasterizer* rasterizer = lovrRasterizerCreate(NULL, 32);
      state.defaultFont = lovrFontCreate(rasterizer);
      lovrRelease(rasterizer);
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

float lovrGraphicsGetLineWidth() {
  return state.pipeline->lineWidth;
}

void lovrGraphicsSetLineWidth(uint8_t width) {
  lovrAssert(width > 0 && width <= 255, "Line width must be between 0 and 255");
  state.pipeline->lineWidth = width;
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
  lovrAssert(!shader || lovrShaderGetType(shader) == SHADER_GRAPHICS, "Compute shaders can not be set as the active shader");
  lovrRetain(shader);
  lovrRelease(state.shader);
  state.shader = shader;
}

void lovrGraphicsGetStencilTest(CompareMode* mode, int* value) {
  *mode = state.pipeline->stencilMode;
  *value = state.pipeline->stencilValue;
}

void lovrGraphicsSetStencilTest(CompareMode mode, int value) {
  state.pipeline->stencilMode = mode;
  state.pipeline->stencilValue = value;
}

Winding lovrGraphicsGetWinding() {
  return state.pipeline->winding;
}

void lovrGraphicsSetWinding(Winding winding) {
  state.pipeline->winding = winding;
}

bool lovrGraphicsIsWireframe() {
  return state.pipeline->wireframe;
}

void lovrGraphicsSetWireframe(bool wireframe) {
#ifndef EMSCRIPTEN
  state.pipeline->wireframe = wireframe;
#endif
}

// Transforms

void lovrGraphicsPush() {
  lovrAssert(++state.transform < MAX_TRANSFORMS, "Unbalanced matrix stack (more pushes than pops?)");
  mat4_init(state.transforms[state.transform], state.transforms[state.transform - 1]);
}

void lovrGraphicsPop() {
  lovrAssert(--state.transform >= 0, "Unbalanced matrix stack (more pops than pushes?)");
}

void lovrGraphicsOrigin() {
  mat4_identity(state.transforms[state.transform]);
}

void lovrGraphicsTranslate(vec3 translation) {
  mat4_translate(state.transforms[state.transform], translation[0], translation[1], translation[2]);
}

void lovrGraphicsRotate(quat rotation) {
  mat4_rotateQuat(state.transforms[state.transform], rotation);
}

void lovrGraphicsScale(vec3 scale) {
  mat4_scale(state.transforms[state.transform], scale[0], scale[1], scale[2]);
}

void lovrGraphicsMatrixTransform(mat4 transform) {
  mat4_multiply(state.transforms[state.transform], transform);
}

void lovrGraphicsSetProjection(mat4 projection) {
  mat4_set(state.camera.projection[0], projection);
  mat4_set(state.camera.projection[1], projection);
}

// Rendering

float* lovrGraphicsGetVertexPointer(uint32_t count) {
  lovrAssert(count <= MAX_VERTICES, "Hey now!  Up to %d vertices are allowed per primitive", MAX_VERTICES);

  if (state.vertexCursor + count > MAX_VERTICES) {
    lovrGraphicsFlush();
    state.batchVertex = state.vertexCursor = 0;
  }

  return lovrMeshMapVertices(state.defaultMesh, state.vertexCursor * 8 * sizeof(float));
}

uint16_t* lovrGraphicsGetIndexPointer(uint32_t count) {
  lovrAssert(count <= MAX_INDICES, "Whoa there!  Up to %d indices are allowed per primitive", MAX_INDICES);

  if (state.indexCursor + count > MAX_INDICES) {
    lovrGraphicsFlush();
    state.batchIndex = state.indexCursor = 0;
  }

  return lovrMeshMapIndices(state.defaultMesh, count ? MAX_INDICES : 0, sizeof(uint16_t), state.indexCursor * sizeof(uint16_t));
}

void lovrGraphicsClear(Color* color, float* depth, int* stencil) {
  if (color) gammaCorrectColor(color);
  lovrGpuClear(state.canvas ? state.canvas : state.camera.canvas, color, depth, stencil);
}

void lovrGraphicsDiscard(bool color, bool depth, bool stencil) {
  lovrGpuDiscard(state.canvas ? state.canvas : state.camera.canvas, color, depth, stencil);
}

void lovrGraphicsFlush() {
  if (state.batchSize == 0) {
    return;
  }

  // Resolve objects
  DrawRequest* draw = &state.batch;
  Mesh* mesh = draw->mesh ? draw->mesh : state.defaultMesh;
  Canvas* canvas = state.canvas ? state.canvas : state.camera.canvas;
  Material* material = draw->material ? draw->material : (state.defaultMaterial ? state.defaultMaterial : (state.defaultMaterial = lovrMaterialCreate()));
  Shader* shader = state.shader ? state.shader : (state.defaultShaders[draw->shader] ? state.defaultShaders[draw->shader] : (state.defaultShaders[draw->shader] = lovrShaderCreateDefault(draw->shader)));

  if (!draw->material) {
    lovrMaterialSetTexture(material, TEXTURE_DIFFUSE, draw->diffuseTexture);
    lovrMaterialSetTexture(material, TEXTURE_ENVIRONMENT_MAP, draw->environmentMap);
  }

  // Camera
  lovrShaderSetMatrices(shader, "lovrViews", state.camera.viewMatrix[0], 0, 32);
  lovrShaderSetMatrices(shader, "lovrProjections", state.camera.projection[0], 0, 32);

  // Pose
  if (draw->pose) {
    lovrShaderSetMatrices(shader, "lovrPose", draw->pose, 0, MAX_BONES * 16);
  } else {
    lovrShaderSetMatrices(shader, "lovrPose", (float[16]) MAT4_IDENTITY, 0, 16);
  }

  // Point size
  lovrShaderSetFloats(shader, "lovrPointSize", &state.pointSize, 0, 1);

  // Buffers and Mesh
  int vertexCount = state.vertexCursor - state.batchVertex;
  int indexCount = state.indexCursor - state.batchIndex;
  lovrBufferFlush(lovrShaderBlockGetBuffer(state.block), 0, MAX_BATCH_SIZE * (16 * sizeof(float) + 4 * sizeof(float)));
  lovrBufferFlush(state.vertexMap, state.batchVertex, vertexCount);

  if (!draw->mesh) {
    if (indexCount > 0) {
      lovrMeshSetDrawRange(state.defaultMesh, state.batchIndex, indexCount);
    } else {
      lovrMeshSetDrawRange(state.defaultMesh, state.batchVertex, vertexCount);
    }

    lovrMeshSetDrawMode(state.defaultMesh, draw->mode);
    lovrMeshFlushVertices(state.defaultMesh, state.batchVertex * 8 * sizeof(float), vertexCount * 8 * sizeof(float));
    lovrMeshFlushIndices(state.defaultMesh);
  }

  lovrMaterialBind(material, shader);
  lovrShaderSetBlock(shader, "lovrDrawData", state.block, ACCESS_READ);

  lovrGpuDraw(&(DrawCommand) {
    .mesh = mesh,
    .shader = shader,
    .canvas = canvas,
    .pipeline = *state.pipeline,
    .instances = draw->instances,
    .width = canvas ? lovrCanvasGetWidth(canvas) : state.width,
    .height = canvas ? lovrCanvasGetHeight(canvas) : state.height,
    .stereo = !draw->mono && (canvas ? lovrCanvasIsStereo(canvas) : state.camera.stereo)
  }, 1);

  state.batchSize = 0;
  state.batchVertex = state.vertexCursor;
  state.batchIndex = state.indexCursor;
}

void lovrGraphicsDraw(DrawRequest* draw) {
  if (state.batchSize > 0 && !batchable(draw)) {
    lovrGraphicsFlush();
  }

  if (state.batchSize == 0) {
    memcpy(&state.batch, draw, sizeof(DrawRequest));
  } else if (draw->mesh && draw->instances <= 1) {
    state.batch.instances++;
  }

  // Geometry
  if (!draw->mesh) {
    if (draw->vertex.data) {
      void* vertices = lovrGraphicsGetVertexPointer(draw->vertex.count);
      memcpy(vertices, draw->vertex.data, draw->vertex.count * 8 * sizeof(float));
    }

    if (draw->index.count) {
      // TODO conditionally use BaseVertex when it is supported to avoid this
      if (draw->index.data) {
        for (uint32_t i = 0; i < draw->index.count; i++) {
          draw->index.data[i] += state.vertexCursor;
        }
      }

      if (draw->index.data) {
        uint16_t* indices = lovrGraphicsGetIndexPointer(draw->index.count);
        memcpy(indices, draw->index.data, draw->index.count * sizeof(uint16_t));
      }

      state.indexCursor += draw->index.count;
    } else {
      lovrMeshMapIndices(state.defaultMesh, 0, 0, 0);
    }

    void* vertexMap = lovrBufferMap(state.vertexMap, state.vertexCursor * sizeof(uint8_t));
    memset(vertexMap, state.batchSize, draw->vertex.count * sizeof(uint8_t));
    state.vertexCursor += draw->vertex.count;
  }

  // Transform and color
  struct { float model[MAX_BATCH_SIZE][16]; float color[MAX_BATCH_SIZE][4]; } *drawData;
  drawData = lovrBufferMap(lovrShaderBlockGetBuffer(state.block), 0);

  memcpy(drawData->model[state.batchSize], state.transforms[state.transform], 16 * sizeof(float));
  if (draw->transform) { mat4_multiply(drawData->model[state.batchSize], draw->transform); }

  memcpy(drawData->color[state.batchSize], (float*) &state.color, 4 * sizeof(float));

  if (++state.batchSize >= MAX_BATCH_SIZE) {
    lovrGraphicsFlush();
  }
}

void lovrGraphicsPoints(uint32_t count) {
  lovrGraphicsDraw(&(DrawRequest) {
    .mode = DRAW_POINTS,
    .vertex.count = count
  });
}

void lovrGraphicsLine(uint32_t count) {
  lovrGraphicsDraw(&(DrawRequest) {
    .mode = DRAW_LINE_STRIP,
    .vertex.count = count
  });
}

void lovrGraphicsTriangle(DrawStyle style, Material* material, float points[9]) {
  if (style == STYLE_LINE) {
    lovrGraphicsDraw(&(DrawRequest) {
      .material = material,
      .mode = DRAW_LINE_LOOP,
      .vertex.count = 3,
      .vertex.data = (float[]) {
        points[0], points[1], points[2], 0, 0, 0, 0, 0,
        points[3], points[4], points[5], 0, 0, 0, 0, 0,
        points[6], points[7], points[8], 0, 0, 0, 0, 0
      }
    });
  } else {
    float normal[3];
    vec3_cross(vec3_init(normal, &points[0]), &points[3]);
    lovrGraphicsDraw(&(DrawRequest) {
      .material = material,
      .mode = DRAW_TRIANGLES,
      .vertex.count = 3,
      .vertex.data = (float[]) {
        points[0], points[1], points[2], normal[0], normal[1], normal[2], 0, 0,
        points[3], points[4], points[5], normal[0], normal[1], normal[2], 0, 0,
        points[6], points[7], points[8], normal[0], normal[1], normal[2], 0, 0
      }
    });
  }
}

void lovrGraphicsPlane(DrawStyle style, Material* material, mat4 transform) {
  if (style == STYLE_LINE) {
    lovrGraphicsDraw(&(DrawRequest) {
      .transform = transform,
      .material = material,
      .mode = DRAW_LINE_LOOP,
      .vertex.count = 4,
      .vertex.data = (float[]) {
        -.5, .5, 0,  0, 0, 0, 0, 0,
        .5, .5, 0,   0, 0, 0, 0, 0,
        .5, -.5, 0,  0, 0, 0, 0, 0,
        -.5, -.5, 0, 0, 0, 0, 0, 0
      }
    });
  } else {
    lovrGraphicsDraw(&(DrawRequest) {
      .transform = transform,
      .material = material,
      .mode = DRAW_TRIANGLES,
      .vertex.count = 4,
      .vertex.data = (float[]) {
        -.5, .5, 0,  0, 0, -1, 0, 1,
        -.5, -.5, 0, 0, 0, -1, 0, 0,
        .5, .5, 0,   0, 0, -1, 1, 1,
        .5, -.5, 0,  0, 0, -1, 1, 0
      },
      .index.count = 6,
      .index.data = (uint16_t[]) { 0, 1, 2, 2, 1, 3 }
    });
  }
}

void lovrGraphicsBox(DrawStyle style, Material* material, mat4 transform) {
  if (style == STYLE_LINE) {
    lovrGraphicsDraw(&(DrawRequest) {
      .transform = transform,
      .material = material,
      .mode = DRAW_LINES,
      .vertex.count = 8,
      .vertex.data = (float[]) {
        // Front
        -.5, .5, -.5,  0, 0, 0, 0, 0,
        .5, .5, -.5,   0, 0, 0, 0, 0,
        .5, -.5, -.5,  0, 0, 0, 0, 0,
        -.5, -.5, -.5, 0, 0, 0, 0, 0,
        // Back
        -.5, .5, .5,   0, 0, 0, 0, 0,
        .5, .5, .5,    0, 0, 0, 0, 0,
        .5, -.5, .5,   0, 0, 0, 0, 0,
        -.5, -.5, .5,  0, 0, 0, 0, 0
      },
      .index.count = 24,
      .index.data = (uint16_t[]) {
        0, 1, 1, 2, 2, 3, 3, 0, // Front
        4, 5, 5, 6, 6, 7, 7, 4, // Back
        0, 4, 1, 5, 2, 6, 3, 7  // Connections
      }
    });
  } else {
    lovrGraphicsDraw(&(DrawRequest) {
      .transform = transform,
      .material = material,
      .mode = DRAW_TRIANGLES,
      .vertex.count = 24,
      .vertex.data = (float[]) {
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
        // Top
        -.5, .5, -.5,   0, 1, 0,  0, 1,
        -.5, .5, .5,    0, 1, 0,  0, 0,
        .5, .5, -.5,    0, 1, 0,  1, 1,
        .5, .5, .5,     0, 1, 0,  1, 0
      },
      .index.count = 36,
      .index.data = (uint16_t[]) {
        0,  1,   2,  2,  1,  3,
        4,  5,   6,  6,  5,  7,
        8,  9,  10, 10,  9, 11,
        12, 13, 14, 14, 13, 15,
        16, 17, 18, 18, 17, 19,
        20, 21, 22, 22, 21, 23
      }
    });
  }
}

void lovrGraphicsArc(DrawStyle style, ArcMode arcMode, Material* material, mat4 transform, float theta1, float theta2, int segments) {
  if (fabsf(theta1 - theta2) >= 2 * M_PI) {
    theta1 = 0;
    theta2 = 2 * M_PI;
  }

  bool hasCenterPoint = arcMode == ARC_MODE_PIE && fabsf(theta1 - theta2) < 2 * M_PI;
  uint32_t count = segments + 1 + hasCenterPoint;
  float* vertices = lovrGraphicsGetVertexPointer(count);

  if (hasCenterPoint) {
    memcpy(vertices, ((float[]) { 0, 0, 0, 0, 0, 1, .5, .5 }), 8 * sizeof(float));
    vertices += 8;
  }

  float theta = theta1;
  float angleShift = (theta2 - theta1) / (float) segments;

  for (int i = 0; i <= segments; i++) {
    float x = cos(theta) * .5;
    float y = sin(theta) * .5;
    memcpy(vertices, ((float[]) { x, y, 0, 0, 0, 1, x + .5, 1 - (y + .5) }), 8 * sizeof(float));
    vertices += 8;
    theta += angleShift;
  }

  lovrGraphicsDraw(&(DrawRequest) {
    .transform = transform,
    .material = material,
    .mode = style == STYLE_LINE ? (arcMode == ARC_MODE_OPEN ? DRAW_LINE_STRIP : DRAW_LINE_LOOP) : DRAW_TRIANGLE_FAN,
    .vertex.count = count
  });
}

void lovrGraphicsCircle(DrawStyle style, Material* material, mat4 transform, int segments) {
  lovrGraphicsArc(style, ARC_MODE_OPEN, material, transform, 0, 2 * M_PI, segments);
}

void lovrGraphicsCylinder(Material* material, float x1, float y1, float z1, float x2, float y2, float z2, float r1, float r2, bool capped, int segments) {
  float axis[3] = { x1 - x2, y1 - y2, z1 - z2 };
  float n[3] = { x1 - x2, y1 - y2, z1 - z2 };
  float p[3];
  float q[3];

  uint32_t vertexCount = ((capped && r1) * (segments + 2) + (capped && r2) * (segments + 2) + 2 * (segments + 1));
  uint32_t indexCount = 3 * segments * ((capped && r1) + (capped && r2) + 2);

  float* vertices = lovrGraphicsGetVertexPointer(vertexCount);
  uint16_t* indices = lovrGraphicsGetIndexPointer(indexCount);
  float* baseVertex = vertices;

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
  *vertices++ = x; \
  *vertices++ = y; \
  *vertices++ = z; \
  *vertices++ = nx; \
  *vertices++ = ny; \
  *vertices++ = nz; \
  *vertices++ = 0; \
  *vertices++ = 0;
#define PUSH_CYLINDER_TRIANGLE(i1, i2, i3) \
  *indices++ = i1; \
  *indices++ = i2; \
  *indices++ = i3; \

  // Ring
  for (int i = 0; i <= segments; i++) {
    float theta = i * (2 * M_PI) / segments;
    n[0] = cos(theta) * p[0] + sin(theta) * q[0];
    n[1] = cos(theta) * p[1] + sin(theta) * q[1];
    n[2] = cos(theta) * p[2] + sin(theta) * q[2];
    PUSH_CYLINDER_VERTEX(x1 + r1 * n[0], y1 + r1 * n[1], z1 + r1 * n[2], n[0], n[1], n[2]);
    PUSH_CYLINDER_VERTEX(x2 + r2 * n[0], y2 + r2 * n[1], z2 + r2 * n[2], n[0], n[1], n[2]);
  }

  // Top
  int topOffset = (segments + 1) * 2 + state.vertexCursor;
  if (capped && r1 != 0) {
    PUSH_CYLINDER_VERTEX(x1, y1, z1, axis[0], axis[1], axis[2]);
    for (int i = 0; i <= segments; i++) {
      int j = i * 2 * 8;
      PUSH_CYLINDER_VERTEX(baseVertex[j + 0], baseVertex[j + 1], baseVertex[j + 2], axis[0], axis[1], axis[2]);
    }
  }

  // Bottom
  int bottomOffset = (segments + 1) * 2 + (1 + segments + 1) * (capped && r1 != 0) + state.vertexCursor;
  if (capped && r2 != 0) {
    PUSH_CYLINDER_VERTEX(x2, y2, z1, -axis[0], -axis[1], -axis[2]);
    for (int i = 0; i <= segments; i++) {
      int j = i * 2 * 8 + 8;
      PUSH_CYLINDER_VERTEX(baseVertex[j + 0], baseVertex[j + 1], baseVertex[j + 2], -axis[0], -axis[1], -axis[2]);
    }
  }

  // Indices
  for (int i = 0; i < segments; i++) {
    int j = 2 * i + state.vertexCursor;
    PUSH_CYLINDER_TRIANGLE(j, j + 1, j + 2);
    PUSH_CYLINDER_TRIANGLE(j + 1, j + 3, j + 2);

    if (capped && r1 != 0) {
      PUSH_CYLINDER_TRIANGLE(topOffset, topOffset + i + 1, topOffset + i + 2);
    }

    if (capped && r2 != 0) {
      PUSH_CYLINDER_TRIANGLE(bottomOffset, bottomOffset + i + 1, bottomOffset + i + 2);
    }
  }
#undef PUSH_CYLINDER_VERTEX
#undef PUSH_CYLINDER_TRIANGLE

  lovrGraphicsDraw(&(DrawRequest) {
    .material = material,
    .mode = DRAW_TRIANGLES,
    .vertex.count = vertexCount,
    .index.count = indexCount
  });
}

void lovrGraphicsSphere(Material* material, mat4 transform, int segments) {
  uint32_t vertexCount = (segments + 1) * (segments + 1);
  uint32_t indexCount = segments * segments * 6;
  float* vertices = lovrGraphicsGetVertexPointer(vertexCount);
  uint16_t* indices = lovrGraphicsGetIndexPointer(indexCount);

  for (int i = 0; i <= segments; i++) {
    float v = i / (float) segments;
    for (int j = 0; j <= segments; j++) {
      float u = j / (float) segments;

      float x = sin(u * 2 * M_PI) * sin(v * M_PI);
      float y = cos(v * M_PI);
      float z = -cos(u * 2 * M_PI) * sin(v * M_PI);
      memcpy(vertices, ((float[]) { x, y, z, x, y, z, u, 1 - v }), 8 * sizeof(float));
      vertices += 8;
    }
  }

  for (int i = 0; i < segments; i++) {
    uint16_t offset0 = i * (segments + 1);
    uint16_t offset1 = (i + 1) * (segments + 1);
    for (int j = 0; j < segments; j++) {
      uint16_t i0 = offset0 + j + state.vertexCursor;
      uint16_t i1 = offset1 + j + state.vertexCursor;
      memcpy(indices, ((uint16_t[]) { i0, i1, i0 + 1, i1, i1 + 1, i0 + 1 }), 6 * sizeof(uint16_t));
      indices += 6;
    }
  }

  lovrGraphicsDraw(&(DrawRequest) {
    .transform = transform,
    .material = material,
    .mode = DRAW_TRIANGLES,
    .vertex.count = vertexCount,
    .index.count = indexCount
  });
}

void lovrGraphicsSkybox(Texture* texture, float angle, float ax, float ay, float az) {
  TextureType type = lovrTextureGetType(texture);
  lovrAssert(type == TEXTURE_CUBE || type == TEXTURE_2D, "Only 2D and cube textures can be used as skyboxes");
  lovrGraphicsPushPipeline();
  lovrGraphicsSetWinding(WINDING_COUNTERCLOCKWISE);
  lovrGraphicsDraw(&(DrawRequest) {
    .shader = type == TEXTURE_CUBE ? SHADER_CUBE : SHADER_PANO,
    .diffuseTexture = type == TEXTURE_2D ? texture : NULL,
    .environmentMap = type == TEXTURE_CUBE ? texture : NULL,
    .mode = DRAW_TRIANGLE_STRIP,
    .vertex.count = 4,
    .vertex.data = (float[]) {
      -1, 1, 1,  0, 0, 0, 0, 0,
      -1, -1, 1, 0, 0, 0, 0, 0,
      1, 1, 1,   0, 0, 0, 0, 0,
      1, -1, 1,  0, 0, 0, 0, 0
    }
  });
  lovrGraphicsPopPipeline();
}

void lovrGraphicsPrint(const char* str, mat4 transform, float wrap, HorizontalAlign halign, VerticalAlign valign) {
  Font* font = lovrGraphicsGetFont();
  float scale = 1 / font->pixelDensity;
  float offsety;
  uint32_t vertexCount;
  uint32_t maxVertices = strlen(str) * 6;
  float* vertices = lovrGraphicsGetVertexPointer(maxVertices);
  lovrFontRender(font, str, wrap, halign, valign, vertices, &offsety, &vertexCount);

  lovrGraphicsPush();
  lovrGraphicsMatrixTransform(transform);
  lovrGraphicsScale((float[3]) { scale, scale, scale });
  lovrGraphicsTranslate((float[3]) { 0, offsety, 0 });
  lovrGraphicsPushPipeline();
  lovrGraphicsSetAlphaSampling(true);
  lovrGraphicsDraw(&(DrawRequest) {
    .shader = SHADER_FONT,
    .diffuseTexture = font->texture,
    .mode = DRAW_TRIANGLES,
    .vertex.count = vertexCount
  });
  lovrGraphicsPopPipeline();
  lovrGraphicsPop();
}

void lovrGraphicsFill(Texture* texture, float u, float v, float w, float h) {
  lovrGraphicsPushPipeline();
  lovrGraphicsSetDepthTest(COMPARE_NONE, false);
  lovrGraphicsDraw(&(DrawRequest) {
    .mono = true,
    .shader = SHADER_FILL,
    .diffuseTexture = texture,
    .mode = DRAW_TRIANGLE_STRIP,
    .vertex.count = 4,
    .vertex.data = (float[]) {
      -1, 1, 0,  0, 0, 0, u, v + h,
      -1, -1, 0, 0, 0, 0, u, v,
      1, 1, 0,   0, 0, 0, u + w, v + h,
      1, -1, 0,  0, 0, 0, u + w, v
    }
  });
  lovrGraphicsPopPipeline();
}
