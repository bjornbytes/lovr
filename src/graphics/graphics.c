#include "graphics/graphics.h"
#include "graphics/buffer.h"
#include "graphics/canvas.h"
#include "graphics/material.h"
#include "graphics/mesh.h"
#include "graphics/texture.h"
#include "data/rasterizer.h"
#include "event/event.h"
#include "math/math.h"
#include "lib/maf.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

static GraphicsState state;

static void gammaCorrectColor(Color* color) {
  color->r = lovrMathGammaToLinear(color->r);
  color->g = lovrMathGammaToLinear(color->g);
  color->b = lovrMathGammaToLinear(color->b);
}

static void onCloseWindow(void) {
  lovrEventPush((Event) { .type = EVENT_QUIT, .data.quit = { false, 0 } });
}

static void onResizeWindow(int width, int height) {
  state.width = width;
  state.height = height;
}

static const uint32_t BUFFER_COUNTS[] = {
  [STREAM_VERTEX] = (1 << 16) - 1,
  [STREAM_INDEX] = 1 << 16,
  [STREAM_DRAW_ID] = (1 << 16) - 1,
#ifdef LOVR_WEBGL // Temporarily work around bug where big UBOs don't work
  [STREAM_TRANSFORM] = MAX_DRAWS,
  [STREAM_COLOR] = MAX_DRAWS
#else
  [STREAM_TRANSFORM] = MAX_DRAWS * MAX_BATCHES * 2,
  [STREAM_COLOR] = MAX_DRAWS * MAX_BATCHES * 2
#endif
};

static const size_t BUFFER_STRIDES[] = {
  [STREAM_VERTEX] = 8 * sizeof(float),
  [STREAM_INDEX] = sizeof(uint16_t),
  [STREAM_DRAW_ID] = sizeof(uint8_t),
  [STREAM_TRANSFORM] = 16 * sizeof(float),
  [STREAM_COLOR] = 4 * sizeof(float)
};

static const BufferType BUFFER_TYPES[] = {
  [STREAM_VERTEX] = BUFFER_VERTEX,
  [STREAM_INDEX] = BUFFER_INDEX,
  [STREAM_DRAW_ID] = BUFFER_GENERIC,
  [STREAM_TRANSFORM] = BUFFER_UNIFORM,
  [STREAM_COLOR] = BUFFER_UNIFORM
};

static void lovrGraphicsInitBuffers() {
  for (int i = 0; i < MAX_BUFFER_ROLES; i++) {
    state.buffers[i] = lovrBufferCreate(BUFFER_COUNTS[i] * BUFFER_STRIDES[i], NULL, BUFFER_TYPES[i], USAGE_STREAM, false);
  }

  // The identity buffer is used for autoinstanced meshes and instanced primitives and maps the
  // instance ID to a vertex attribute.  Its contents never change, so they are initialized here.
  state.identityBuffer = lovrBufferCreate(MAX_DRAWS, NULL, BUFFER_VERTEX, USAGE_STATIC, false);
  uint8_t* id = lovrBufferMap(state.identityBuffer, 0);
  for (int i = 0; i < MAX_DRAWS; i++) id[i] = i;
  lovrBufferFlush(state.identityBuffer, 0, MAX_DRAWS);
  lovrBufferUnmap(state.identityBuffer);

  Buffer* vertexBuffer = state.buffers[STREAM_VERTEX];
  size_t stride = BUFFER_STRIDES[STREAM_VERTEX];

  MeshAttribute position = { .buffer = vertexBuffer, .offset = 0, .stride = stride, .type = F32, .components = 3 };
  MeshAttribute normal = { .buffer = vertexBuffer, .offset = 12, .stride = stride, .type = F32, .components = 3 };
  MeshAttribute texCoord = { .buffer = vertexBuffer, .offset = 24, .stride = stride, .type = F32, .components = 2 };
  MeshAttribute drawId = { .buffer = state.buffers[STREAM_DRAW_ID], .type = U8, .components = 1, .integer = true };
  MeshAttribute identity = { .buffer = state.identityBuffer, .type = U8, .components = 1, .divisor = 1, .integer = true };

  state.mesh = lovrMeshCreate(DRAW_TRIANGLES, NULL, 0);
  lovrMeshAttachAttribute(state.mesh, "lovrPosition", &position);
  lovrMeshAttachAttribute(state.mesh, "lovrNormal", &normal);
  lovrMeshAttachAttribute(state.mesh, "lovrTexCoord", &texCoord);
  lovrMeshAttachAttribute(state.mesh, "lovrDrawID", &drawId);

  state.instancedMesh = lovrMeshCreate(DRAW_TRIANGLES, NULL, 0);
  lovrMeshAttachAttribute(state.instancedMesh, "lovrPosition", &position);
  lovrMeshAttachAttribute(state.instancedMesh, "lovrNormal", &normal);
  lovrMeshAttachAttribute(state.instancedMesh, "lovrTexCoord", &texCoord);
  lovrMeshAttachAttribute(state.instancedMesh, "lovrDrawID", &identity);
}

static void* lovrGraphicsMapBuffer(BufferRole role, uint32_t count) {
  Buffer* buffer = state.buffers[role];
  uint32_t limit = BUFFER_COUNTS[role];
  lovrAssert(count <= limit, "Whoa there!  Tried to get %d elements from a buffer that only has %d elements.", count, limit);

  if (state.cursors[role] + count > limit) {
    lovrGraphicsFlush();
    state.cursors[role] = 0;

    // Locks are placed as late as possible, causing the last lock to never get placed.  Whenever we
    // wrap around a buffer, we gotta place that last missing lock.
    state.locks[role][MAX_LOCKS - 1] = lovrGpuLock();
  }

  // Wait on any pending locks for the mapped region(s)
  int firstLock = state.cursors[role] / (BUFFER_COUNTS[role] / MAX_LOCKS);
  int lastLock = MIN(state.cursors[role] + count, BUFFER_COUNTS[role] - 1) / (BUFFER_COUNTS[role] / MAX_LOCKS);
  for (int i = firstLock; i <= lastLock; i++) {
    if (state.locks[role][i]) {
      lovrGpuUnlock(state.locks[role][i]);
      lovrGpuDestroyLock(state.locks[role][i]);
      state.locks[role][i] = NULL;
    }
  }

  return lovrBufferMap(buffer, state.cursors[role] * BUFFER_STRIDES[role]);
}

static bool areBatchParamsEqual(BatchType typeA, BatchType typeB, BatchParams* a, BatchParams* b) {
  if (typeA != typeB) return false;

  switch (typeA) {
    case BATCH_TRIANGLES:
      return a->triangles.style == b->triangles.style;
    case BATCH_BOX:
      return a->box.style == b->box.style;
    case BATCH_ARC:
      return
        a->arc.style == b->arc.style && a->arc.mode == b->arc.mode &&
        a->arc.r1 == b->arc.r1 && a->arc.r2 == b->arc.r2 && a->arc.segments == b->arc.segments;
    case BATCH_CYLINDER:
      return
        a->cylinder.r1 == b->cylinder.r1 && a->cylinder.r2 == b->cylinder.r2 &&
        a->cylinder.capped == b->cylinder.capped && a->cylinder.segments == b->cylinder.segments;
    case BATCH_SPHERE:
      return a->sphere.segments == b->sphere.segments;
    case BATCH_MESH:
      return
        a->mesh.object == b->mesh.object && a->mesh.mode == b->mesh.mode &&
        a->mesh.rangeStart == b->mesh.rangeStart && a->mesh.rangeCount == b->mesh.rangeCount;
    default:
      return true;
  }
}

// Base

bool lovrGraphicsInit() {
  return false; // Graphics is only initialized when the window is created, because OpenGL
}

void lovrGraphicsDestroy() {
  if (!state.initialized) return;
  lovrGraphicsSetShader(NULL);
  lovrGraphicsSetFont(NULL);
  lovrGraphicsSetCanvas(NULL);
  for (int i = 0; i < MAX_DEFAULT_SHADERS; i++) {
    lovrRelease(Shader, state.defaultShaders[i]);
  }
  for (int i = 0; i < MAX_BUFFER_ROLES; i++) {
    lovrRelease(Buffer, state.buffers[i]);
    for (int j = 0; j < MAX_LOCKS; j++) {
      lovrGpuDestroyLock(state.locks[i][j]);
    }
  }
  lovrRelease(Mesh, state.mesh);
  lovrRelease(Mesh, state.instancedMesh);
  lovrRelease(Buffer, state.identityBuffer);
  lovrRelease(Material, state.defaultMaterial);
  lovrRelease(Font, state.defaultFont);
  lovrGpuDestroy();
  memset(&state, 0, sizeof(GraphicsState));
}

void lovrGraphicsPresent() {
  lovrGraphicsFlush();
  lovrPlatformSwapBuffers();
  lovrGpuPresent();
}

void lovrGraphicsCreateWindow(WindowFlags* flags) {
  lovrAssert(!state.initialized, "Window is already created");
#ifdef EMSCRIPTEN
  flags->vsync = 1;
#else
  flags->vsync = 0;
#endif
  lovrAssert(lovrPlatformCreateWindow(flags), "Could not create window");
  lovrPlatformOnWindowClose(onCloseWindow);
  lovrPlatformOnWindowResize(onResizeWindow);
  lovrPlatformGetFramebufferSize(&state.width, &state.height);
  lovrGpuInit(lovrGetProcAddress);
  lovrGraphicsInitBuffers();
  lovrGraphicsReset();
  state.initialized = true;
}

int lovrGraphicsGetWidth() {
  return state.width;
}

int lovrGraphicsGetHeight() {
  return state.height;
}

float lovrGraphicsGetPixelDensity() {
  int width, framebufferWidth;
  lovrPlatformGetWindowSize(&width, NULL);
  lovrPlatformGetFramebufferSize(&framebufferWidth, NULL);
  if (width == 0 || framebufferWidth == 0) {
    return 0.f;
  } else {
    return (float) framebufferWidth / (float) width;
  }
}

void lovrGraphicsSetCamera(Camera* camera, bool clear) {
  lovrGraphicsFlush();

  if (state.camera.canvas && (!camera || camera->canvas != state.camera.canvas)) {
    lovrCanvasResolve(state.camera.canvas);
  }

  if (!camera) {
    memset(&state.camera, 0, sizeof(Camera));
    mat4_identity(state.camera.viewMatrix[0]);
    mat4_identity(state.camera.viewMatrix[1]);
    mat4_perspective(state.camera.projection[0], .01f, 100.f, 67.f * (float) M_PI / 180.f, (float) state.width / state.height);
    mat4_perspective(state.camera.projection[1], .01f, 100.f, 67.f * (float) M_PI / 180.f, (float) state.width / state.height);
  } else {
    state.camera = *camera;
  }

  if (clear) {
    Color background = state.backgroundColor;
    gammaCorrectColor(&background);
    lovrGpuClear(state.camera.canvas, &background, &(float) { 1. }, &(int) { 0 });
  }
}

Buffer* lovrGraphicsGetIdentityBuffer() {
  return state.identityBuffer;
}

// State

void lovrGraphicsReset() {
  state.transform = 0;
  lovrGraphicsSetCamera(NULL, false);
  lovrGraphicsSetAlphaSampling(false);
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

bool lovrGraphicsGetAlphaSampling() {
  return state.pipeline.alphaSampling;
}

void lovrGraphicsSetAlphaSampling(bool sample) {
  state.pipeline.alphaSampling = sample;
}

Color lovrGraphicsGetBackgroundColor() {
  return state.backgroundColor;
}

void lovrGraphicsSetBackgroundColor(Color color) {
  state.backgroundColor = color;
}

void lovrGraphicsGetBlendMode(BlendMode* mode, BlendAlphaMode* alphaMode) {
  *mode = state.pipeline.blendMode;
  *alphaMode = state.pipeline.blendAlphaMode;
}

void lovrGraphicsSetBlendMode(BlendMode mode, BlendAlphaMode alphaMode) {
  state.pipeline.blendMode = mode;
  state.pipeline.blendAlphaMode = alphaMode;
}

Canvas* lovrGraphicsGetCanvas() {
  return state.canvas;
}

void lovrGraphicsSetCanvas(Canvas* canvas) {
  if (state.canvas && canvas != state.canvas) {
    lovrCanvasResolve(state.canvas);
  }

  lovrRetain(canvas);
  lovrRelease(Canvas, state.canvas);
  state.canvas = canvas;
}

Color lovrGraphicsGetColor() {
  return state.color;
}

void lovrGraphicsSetColor(Color color) {
  state.color = color;
}

bool lovrGraphicsIsCullingEnabled() {
  return state.pipeline.culling;
}

void lovrGraphicsSetCullingEnabled(bool culling) {
  state.pipeline.culling = culling;
}

TextureFilter lovrGraphicsGetDefaultFilter() {
  return state.defaultFilter;
}

void lovrGraphicsSetDefaultFilter(TextureFilter filter) {
  state.defaultFilter = filter;
}

void lovrGraphicsGetDepthTest(CompareMode* mode, bool* write) {
  *mode = state.pipeline.depthTest;
  *write = state.pipeline.depthWrite;
}

void lovrGraphicsSetDepthTest(CompareMode mode, bool write) {
  state.pipeline.depthTest = mode;
  state.pipeline.depthWrite = write;
}

Font* lovrGraphicsGetFont() {
  if (!state.font) {
    if (!state.defaultFont) {
      Rasterizer* rasterizer = lovrRasterizerCreate(NULL, 32);
      state.defaultFont = lovrFontCreate(rasterizer);
      lovrRelease(Rasterizer, rasterizer);
    }

    lovrGraphicsSetFont(state.defaultFont);
  }

  return state.font;
}

void lovrGraphicsSetFont(Font* font) {
  lovrRetain(font);
  lovrRelease(Font, state.font);
  state.font = font;
}

float lovrGraphicsGetLineWidth() {
  return state.pipeline.lineWidth;
}

void lovrGraphicsSetLineWidth(uint8_t width) {
  state.pipeline.lineWidth = width;
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
  lovrRelease(Shader, state.shader);
  state.shader = shader;
}

void lovrGraphicsGetStencilTest(CompareMode* mode, int* value) {
  *mode = state.pipeline.stencilMode;
  *value = state.pipeline.stencilValue;
}

void lovrGraphicsSetStencilTest(CompareMode mode, int value) {
  state.pipeline.stencilMode = mode;
  state.pipeline.stencilValue = value;
}

Winding lovrGraphicsGetWinding() {
  return state.pipeline.winding;
}

void lovrGraphicsSetWinding(Winding winding) {
  state.pipeline.winding = winding;
}

bool lovrGraphicsIsWireframe() {
  return state.pipeline.wireframe;
}

void lovrGraphicsSetWireframe(bool wireframe) {
#ifdef LOVR_GL
  state.pipeline.wireframe = wireframe;
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

void lovrGraphicsClear(Color* color, float* depth, int* stencil) {
  if (color) gammaCorrectColor(color);
  if (color || depth || stencil) lovrGraphicsFlush();
  lovrGpuClear(state.canvas ? state.canvas : state.camera.canvas, color, depth, stencil);
}

void lovrGraphicsDiscard(bool color, bool depth, bool stencil) {
  if (color || depth || stencil) lovrGraphicsFlush();
  lovrGpuDiscard(state.canvas ? state.canvas : state.camera.canvas, color, depth, stencil);
}

void lovrGraphicsBatch(BatchRequest* req) {

  // Resolve objects
  Canvas* canvas = state.canvas ? state.canvas : state.camera.canvas;
  Shader* shader = state.shader ? state.shader : (state.defaultShaders[req->shader] ? state.defaultShaders[req->shader] : (state.defaultShaders[req->shader] = lovrShaderCreateDefault(req->shader)));
  Pipeline* pipeline = req->pipeline ? req->pipeline : &state.pipeline;
  Material* material = req->material ? req->material : (state.defaultMaterial ? state.defaultMaterial : (state.defaultMaterial = lovrMaterialCreate()));

  if (!req->material) {
    lovrMaterialSetTexture(material, TEXTURE_DIFFUSE, req->diffuseTexture);
    lovrMaterialSetTexture(material, TEXTURE_ENVIRONMENT_MAP, req->environmentMap);
  }

  if (lovrShaderHasUniform(shader, "lovrPose")) {
    if (req->type == BATCH_MESH && req->params.mesh.pose) {
      lovrShaderSetMatrices(shader, "lovrPose", req->params.mesh.pose, 0, MAX_BONES * 16);
    } else {
      lovrShaderSetMatrices(shader, "lovrPose", (float[]) MAT4_IDENTITY, 0, 16);
    }
  }

  // Try to find an existing batch to use
  Batch* batch = NULL;
  if (req->type != BATCH_MESH || req->params.mesh.instances == 1) {
    for (int i = state.batchCount - 1; i >= 0; i--) {
      Batch* b = &state.batches[i];

      if (b->count >= MAX_DRAWS) { goto next; }
      if (!areBatchParamsEqual(req->type, b->type, &req->params, &b->params)) { goto next; }
      if (b->canvas == canvas && b->shader == shader && !memcmp(&b->pipeline, pipeline, sizeof(Pipeline)) && b->material == material) {
        batch = b;
        break;
      }

      // Draws can't be reordered when blending is on, depth test is off, or either of the batches
      // are streaming their vertices (since buffers are append-only)
next:
      if (b->pipeline.blendMode != BLEND_NONE || pipeline->blendMode != BLEND_NONE) { break; }
      if (b->pipeline.depthTest == COMPARE_NONE || pipeline->depthTest == COMPARE_NONE) { break; }
      if (!req->instanced) { break; }
    }
  }

  if (req->vertexCount > 0 && (!req->instanced || !batch)) {
    *(req->vertices) = lovrGraphicsMapBuffer(STREAM_VERTEX, req->vertexCount);
    uint8_t* ids = lovrGraphicsMapBuffer(STREAM_DRAW_ID, req->vertexCount);
    memset(ids, batch ? batch->count : 0, req->vertexCount * sizeof(uint8_t));

    if (req->indexCount > 0) {
      *(req->indices) = lovrGraphicsMapBuffer(STREAM_INDEX, req->indexCount);
      *(req->baseVertex) = state.cursors[STREAM_VERTEX];
    }

    // The buffer mapping here could have triggered a flush, so if we were hoping to batch with
    // something but the batch count is zero now, we just start a new batch.  Maybe there's a better
    // way to detect this.
    if (batch && state.batchCount == 0) {
      batch = NULL;
    }
  }

  // Start a new batch
  if (!batch) {
    if (state.batchCount >= MAX_BATCHES) {
      lovrGraphicsFlush();
    }

    float* transforms = lovrGraphicsMapBuffer(STREAM_TRANSFORM, MAX_DRAWS);
    Color* colors = lovrGraphicsMapBuffer(STREAM_COLOR, MAX_DRAWS);

    batch = &state.batches[state.batchCount++];
    *batch = (Batch) {
      .type = req->type,
      .params = req->params,
      .drawMode = req->drawMode,
      .canvas = canvas,
      .shader = shader,
      .pipeline = *pipeline,
      .material = material,
      .transforms = transforms,
      .colors = colors,
      .instanced = req->instanced
    };

    for (int i = 0; i < MAX_BUFFER_ROLES; i++) {
      batch->cursors[i].start = state.cursors[i];
    }

    batch->cursors[STREAM_TRANSFORM].count = MAX_DRAWS;
    batch->cursors[STREAM_COLOR].count = MAX_DRAWS;
    state.cursors[STREAM_TRANSFORM] += MAX_DRAWS;
    state.cursors[STREAM_COLOR] += MAX_DRAWS;
  }

  // Transform
  if (req->transform) {
    float transform[16];
    mat4_multiply(mat4_init(transform, state.transforms[state.transform]), req->transform);
    memcpy(&batch->transforms[16 * batch->count], transform, 16 * sizeof(float));
  } else {
    memcpy(&batch->transforms[16 * batch->count], state.transforms[state.transform], 16 * sizeof(float));
  }

  // Color
  Color color = state.color;
  gammaCorrectColor(&color);
  batch->colors[batch->count] = color;

  if (!req->instanced || batch->count == 0) {
    batch->cursors[STREAM_VERTEX].count += req->vertexCount;
    batch->cursors[STREAM_INDEX].count += req->indexCount;
    batch->cursors[STREAM_DRAW_ID].count += req->vertexCount;

    state.cursors[STREAM_VERTEX] += req->vertexCount;
    state.cursors[STREAM_INDEX] += req->indexCount;
    state.cursors[STREAM_DRAW_ID] += req->vertexCount;
  }

  batch->count++;
}

void lovrGraphicsFlush() {
  if (state.batchCount == 0) {
    return;
  }

  // Prevent infinite flushing >_>
  int batchCount = state.batchCount;
  state.batchCount = 0;

  // Flush buffers
  Batch* firstBatch = &state.batches[0];
  Batch* lastBatch = &state.batches[batchCount - 1];
  for (int i = 0; i < MAX_BUFFER_ROLES; i++) {
    size_t offset = firstBatch->cursors[i].start * BUFFER_STRIDES[i];
    size_t size = (lastBatch->cursors[i].start + lastBatch->cursors[i].count - firstBatch->cursors[i].start) * BUFFER_STRIDES[i];
    lovrBufferFlush(state.buffers[i], offset, size);
    lovrBufferUnmap(state.buffers[i]);
  }


  for (int b = 0; b < batchCount; b++) {
    Batch* batch = &state.batches[b];
    BatchParams* params = &batch->params;
    Mesh* mesh = batch->type == BATCH_MESH ? params->mesh.object : (batch->instanced ? state.instancedMesh : state.mesh);
    int instances = batch->instanced ? batch->count : 1;

    // Bind UBOs
    lovrShaderSetBlock(batch->shader, "lovrModelBlock", state.buffers[STREAM_TRANSFORM], batch->cursors[STREAM_TRANSFORM].start * BUFFER_STRIDES[STREAM_TRANSFORM], MAX_DRAWS * BUFFER_STRIDES[STREAM_TRANSFORM], ACCESS_READ);
    lovrShaderSetBlock(batch->shader, "lovrColorBlock", state.buffers[STREAM_COLOR], batch->cursors[STREAM_COLOR].start * BUFFER_STRIDES[STREAM_COLOR], MAX_DRAWS * BUFFER_STRIDES[STREAM_COLOR], ACCESS_READ);

    // Uniforms
    lovrMaterialBind(batch->material, batch->shader);
    lovrShaderSetMatrices(batch->shader, "lovrViews", state.camera.viewMatrix[0], 0, 32);
    lovrShaderSetMatrices(batch->shader, "lovrProjections", state.camera.projection[0], 0, 32);

    if (batch->drawMode == DRAW_POINTS) {
      lovrShaderSetFloats(batch->shader, "lovrPointSize", &state.pointSize, 0, 1);
    }

    uint32_t rangeStart, rangeCount;
    if (batch->type == BATCH_MESH) {
      rangeStart = params->mesh.rangeStart;
      rangeCount = params->mesh.rangeCount;
      if (params->mesh.instances > 1) {
        lovrMeshSetAttributeEnabled(mesh, "lovrDrawID", false);
        instances = params->mesh.instances;
      } else {
        lovrMeshSetAttributeEnabled(mesh, "lovrDrawID", true);
        instances = batch->count;
      }
    } else {
      bool indexed = batch->cursors[STREAM_INDEX].count > 0;
      rangeStart = batch->cursors[indexed ? STREAM_INDEX : STREAM_VERTEX].start;
      rangeCount = batch->cursors[indexed ? STREAM_INDEX : STREAM_VERTEX].count;
      if (indexed) {
        lovrMeshSetIndexBuffer(mesh, state.buffers[STREAM_INDEX], BUFFER_COUNTS[STREAM_INDEX], sizeof(uint16_t), 0);
      } else {
        lovrMeshSetIndexBuffer(mesh, NULL, 0, 0, 0);
      }
    }

    // Draw!
    lovrGpuDraw(&(DrawCommand) {
      .mesh = mesh,
      .shader = batch->shader,
      .canvas = batch->canvas,
      .pipeline = batch->pipeline,
      .drawMode = batch->drawMode,
      .instances = instances,
      .rangeStart = rangeStart,
      .rangeCount = rangeCount,
      .width = batch->canvas ? lovrCanvasGetWidth(batch->canvas) : state.width,
      .height = batch->canvas ? lovrCanvasGetHeight(batch->canvas) : state.height,
      .stereo = batch->type != BATCH_FILL && (batch->canvas ? lovrCanvasIsStereo(batch->canvas) : state.camera.stereo)
    });

    // Pop lock and drop it
    for (int i = 0; i < MAX_BUFFER_ROLES; i++) {
      if (batch->cursors[i].count > 0) {
        size_t lockSize = BUFFER_COUNTS[i] / MAX_LOCKS;
        size_t start = batch->cursors[i].start;
        size_t count = batch->cursors[i].count + 1;
        size_t firstLock = start / lockSize;
        size_t lastLock = MIN(start + count, BUFFER_COUNTS[i] - 1) / lockSize;
        for (size_t j = firstLock; j < lastLock; j++) {
          state.locks[i][j] = lovrGpuLock();
        }
      }
    }
  }
}

void lovrGraphicsFlushCanvas(Canvas* canvas) {
  for (int i = state.batchCount - 1; i >= 0; i--) {
    if (state.batches[i].canvas == canvas) {
      lovrGraphicsFlush();
      return;
    }
  }
}

void lovrGraphicsFlushShader(Shader* shader) {
  for (int i = state.batchCount - 1; i >= 0; i--) {
    if (state.batches[i].shader == shader) {
      lovrGraphicsFlush();
      return;
    }
  }
}

void lovrGraphicsFlushMaterial(Material* material) {
  for (int i = state.batchCount - 1; i >= 0; i--) {
    if (state.batches[i].material == material) {
      lovrGraphicsFlush();
      return;
    }
  }
}

void lovrGraphicsFlushMesh(Mesh* mesh) {
  for (int i = state.batchCount - 1; i >= 0; i--) {
    if (state.batches[i].type == BATCH_MESH && state.batches[i].params.mesh.object == mesh) {
      lovrGraphicsFlush();
      return;
    }
  }
}

void lovrGraphicsPoints(uint32_t count, float** vertices) {
  lovrGraphicsBatch(&(BatchRequest) {
    .type = BATCH_POINTS,
    .drawMode = DRAW_POINTS,
    .vertexCount = count,
    .vertices = vertices
  });
}

void lovrGraphicsLine(uint32_t count, float** vertices) {
  uint32_t indexCount = count + 1;
  uint16_t* indices;
  uint16_t baseVertex;

  lovrGraphicsBatch(&(BatchRequest) {
    .type = BATCH_LINES,
    .drawMode = DRAW_LINE_STRIP,
    .vertexCount = count,
    .vertices = vertices,
    .indexCount = indexCount,
    .indices = &indices,
    .baseVertex = &baseVertex
  });

  indices[0] = 0xffff;
  for (uint32_t i = 1; i < indexCount; i++) {
    indices[i] = baseVertex + i - 1;
  }
}

void lovrGraphicsTriangle(DrawStyle style, Material* material, uint32_t count, float** vertices) {
  uint32_t indexCount = style == STYLE_LINE ? (4 * count / 3) : 0;
  uint16_t* indices;
  uint16_t baseVertex;

  lovrGraphicsBatch(&(BatchRequest) {
    .type = BATCH_TRIANGLES,
    .params.triangles.style = style,
    .drawMode = style == STYLE_LINE ? DRAW_LINE_LOOP : DRAW_TRIANGLES,
    .material = material,
    .vertexCount = count,
    .vertices = vertices,
    .indexCount = indexCount,
    .indices = &indices,
    .baseVertex = &baseVertex
  });

  if (style == STYLE_LINE) {
    for (uint32_t i = 0; i < count; i += 3) {
      *indices++ = 0xffff;
      *indices++ = baseVertex + i + 0;
      *indices++ = baseVertex + i + 1;
      *indices++ = baseVertex + i + 2;
    }
  }
}

void lovrGraphicsPlane(DrawStyle style, Material* material, mat4 transform, float u, float v, float w, float h) {
  float* vertices = NULL;
  uint16_t* indices = NULL;
  uint16_t baseVertex;

  lovrGraphicsBatch(&(BatchRequest) {
    .type = BATCH_PLANE,
    .params.plane.style = style,
    .drawMode = style == STYLE_LINE ? DRAW_LINE_LOOP : DRAW_TRIANGLES,
    .material = material,
    .transform = transform,
    .vertexCount = 4,
    .indexCount = style == STYLE_LINE ? 5 : 6,
    .vertices = &vertices,
    .indices = &indices,
    .baseVertex = &baseVertex
  });

  if (style == STYLE_LINE) {
    static float vertexData[] = {
      -.5f,  .5f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
       .5f,  .5f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
       .5f, -.5f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
      -.5f, -.5f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f
    };

    memcpy(vertices, vertexData, sizeof(vertexData));

    indices[0] = 0xffff;
    indices[1] = 0 + baseVertex;
    indices[2] = 1 + baseVertex;
    indices[3] = 2 + baseVertex;
    indices[4] = 3 + baseVertex;
  } else {
    float vertexData[] = {
      -.5f,  .5f, 0.f, 0.f, 0.f, -1.f, u, v + h,
      -.5f, -.5f, 0.f, 0.f, 0.f, -1.f, u, v,
       .5f,  .5f, 0.f, 0.f, 0.f, -1.f, u + w, v + h,
       .5f, -.5f, 0.f, 0.f, 0.f, -1.f, u + w, v
    };

    memcpy(vertices, vertexData, sizeof(vertexData));

    static uint16_t indexData[] = { 0, 1, 2, 2, 1, 3 };

    for (size_t i = 0; i < sizeof(indexData) / sizeof(indexData[0]); i++) {
      indices[i] = indexData[i] + baseVertex;
    }
  }
}

void lovrGraphicsBox(DrawStyle style, Material* material, mat4 transform) {
  float* vertices = NULL;
  uint16_t* indices = NULL;
  uint16_t baseVertex;

  lovrGraphicsBatch(&(BatchRequest) {
    .type = BATCH_BOX,
    .params.box.style = style,
    .drawMode = style == STYLE_LINE ? DRAW_LINES : DRAW_TRIANGLES,
    .material = material,
    .transform = transform,
    .vertexCount = style == STYLE_LINE ? 8 : 24,
    .indexCount = style == STYLE_LINE ? 24 : 36,
    .vertices = &vertices,
    .indices = &indices,
    .baseVertex = &baseVertex,
    .instanced = true
  });

  if (vertices) {
    if (style == STYLE_LINE) {
      static float vertexData[] = {
        -.5f,  .5f, -.5f, 0.f, 0.f, 0.f, 0.f, 0.f, // Front
         .5f,  .5f, -.5f, 0.f, 0.f, 0.f, 0.f, 0.f,
         .5f, -.5f, -.5f, 0.f, 0.f, 0.f, 0.f, 0.f,
        -.5f, -.5f, -.5f, 0.f, 0.f, 0.f, 0.f, 0.f,
        -.5f,  .5f,  .5f, 0.f, 0.f, 0.f, 0.f, 0.f, // Back
         .5f,  .5f,  .5f, 0.f, 0.f, 0.f, 0.f, 0.f,
         .5f, -.5f,  .5f, 0.f, 0.f, 0.f, 0.f, 0.f,
        -.5f, -.5f,  .5f, 0.f, 0.f, 0.f, 0.f, 0.f
      };

      memcpy(vertices, vertexData, sizeof(vertexData));

      static uint16_t indexData[] = {
        0, 1, 1, 2, 2, 3, 3, 0, // Front
        4, 5, 5, 6, 6, 7, 7, 4, // Back
        0, 4, 1, 5, 2, 6, 3, 7  // Connections
      };

      for (size_t i = 0; i < sizeof(indexData) / sizeof(indexData[0]); i++) {
        indices[i] = indexData[i] + baseVertex;
      }
    } else {
      static float vertexData[] = {
        -.5f, -.5f, -.5f,  0.f,  0.f, -1.f, 0.f, 0.f, // Front
        -.5f,  .5f, -.5f,  0.f,  0.f, -1.f, 0.f, 1.f,
         .5f, -.5f, -.5f,  0.f,  0.f, -1.f, 1.f, 0.f,
         .5f,  .5f, -.5f,  0.f,  0.f, -1.f, 1.f, 1.f,
         .5f,  .5f, -.5f,  1.f,  0.f,  0.f, 0.f, 1.f, // Right
         .5f,  .5f,  .5f,  1.f,  0.f,  0.f, 1.f, 1.f,
         .5f, -.5f, -.5f,  1.f,  0.f,  0.f, 0.f, 0.f,
         .5f, -.5f,  .5f,  1.f,  0.f,  0.f, 1.f, 0.f,
         .5f, -.5f,  .5f,  0.f,  0.f,  1.f, 0.f, 0.f, // Back
         .5f,  .5f,  .5f,  0.f,  0.f,  1.f, 0.f, 1.f,
        -.5f, -.5f,  .5f,  0.f,  0.f,  1.f, 1.f, 0.f,
        -.5f,  .5f,  .5f,  0.f,  0.f,  1.f, 1.f, 1.f,
        -.5f,  .5f,  .5f, -1.f,  0.f,  0.f, 0.f, 1.f, // Left
        -.5f,  .5f, -.5f, -1.f,  0.f,  0.f, 1.f, 1.f,
        -.5f, -.5f,  .5f, -1.f,  0.f,  0.f, 0.f, 0.f,
        -.5f, -.5f, -.5f, -1.f,  0.f,  0.f, 1.f, 0.f,
        -.5f, -.5f, -.5f,  0.f, -1.f,  0.f, 0.f, 0.f, // Bottom
         .5f, -.5f, -.5f,  0.f, -1.f,  0.f, 1.f, 0.f,
        -.5f, -.5f,  .5f,  0.f, -1.f,  0.f, 0.f, 1.f,
         .5f, -.5f,  .5f,  0.f, -1.f,  0.f, 1.f, 1.f,
        -.5f,  .5f, -.5f,  0.f,  1.f,  0.f, 0.f, 1.f, // Top
        -.5f,  .5f,  .5f,  0.f,  1.f,  0.f, 0.f, 0.f,
         .5f,  .5f, -.5f,  0.f,  1.f,  0.f, 1.f, 1.f,
         .5f,  .5f,  .5f,  0.f,  1.f,  0.f, 1.f, 0.f
      };

      memcpy(vertices, vertexData, sizeof(vertexData));

      uint16_t indexData[] = {
        0,  1,   2,  2,  1,  3,
        4,  5,   6,  6,  5,  7,
        8,  9,  10, 10,  9, 11,
        12, 13, 14, 14, 13, 15,
        16, 17, 18, 18, 17, 19,
        20, 21, 22, 22, 21, 23
      };

      for (size_t i = 0; i < sizeof(indexData) / sizeof(indexData[0]); i++) {
        indices[i] = indexData[i] + baseVertex;
      }
    }
  }
}

void lovrGraphicsArc(DrawStyle style, ArcMode mode, Material* material, mat4 transform, float r1, float r2, int segments) {
  bool hasCenterPoint = false;

  if (fabsf(r1 - r2) >= 2.f * (float) M_PI) {
    r1 = 0.f;
    r2 = 2.f * (float) M_PI;
  } else {
    hasCenterPoint = mode == ARC_MODE_PIE;
  }

  uint32_t vertexCount = segments + 1 + hasCenterPoint;
  float* vertices = NULL;

  lovrGraphicsBatch(&(BatchRequest) {
    .type = BATCH_ARC,
    .params.arc.r1 = r1,
    .params.arc.r2 = r2,
    .params.arc.mode = mode,
    .params.arc.style = style,
    .params.arc.segments = segments,
    .drawMode = style == STYLE_LINE ? (mode == ARC_MODE_OPEN ? DRAW_LINE_STRIP : DRAW_LINE_LOOP) : DRAW_TRIANGLE_FAN,
    .material = material,
    .transform = transform,
    .vertexCount = vertexCount,
    .vertices = &vertices,
    .instanced = true
  });

  if (vertices) {
    if (hasCenterPoint) {
      memcpy(vertices, ((float[]) { 0.f, 0.f, 0.f, 0.f, 0.f, 1.f, .5f, .5f }), 8 * sizeof(float));
      vertices += 8;
    }

    float theta = r1;
    float angleShift = (r2 - r1) / (float) segments;

    for (int i = 0; i <= segments; i++) {
      float x = cosf(theta);
      float y = sinf(theta);
      memcpy(vertices, ((float[]) { x, y, 0.f, 0.f, 0.f, 1.f, x + .5f, 1.f - (y + .5f) }), 8 * sizeof(float));
      vertices += 8;
      theta += angleShift;
    }
  }
}

void lovrGraphicsCircle(DrawStyle style, Material* material, mat4 transform, int segments) {
  lovrGraphicsArc(style, ARC_MODE_OPEN, material, transform, 0, 2.f * (float) M_PI, segments);
}

void lovrGraphicsCylinder(Material* material, mat4 transform, float r1, float r2, bool capped, int segments) {
  float length = vec3_length((float[3]) { transform[8], transform[9], transform[10] });
  r1 /= length;
  r2 /= length;

  uint32_t vertexCount = ((capped && r1) * (segments + 2) + (capped && r2) * (segments + 2) + 2 * (segments + 1));
  uint32_t indexCount = 3 * segments * ((capped && r1) + (capped && r2) + 2);
  float* vertices = NULL;
  uint16_t* indices = NULL;
  uint16_t baseVertex;

  lovrGraphicsBatch(&(BatchRequest) {
    .type = BATCH_CYLINDER,
    .params.cylinder.r1 = r1,
    .params.cylinder.r2 = r2,
    .params.cylinder.capped = capped,
    .params.cylinder.segments = segments,
    .drawMode = DRAW_TRIANGLES,
    .material = material,
    .transform = transform,
    .vertexCount = vertexCount,
    .indexCount = indexCount,
    .vertices = &vertices,
    .indices = &indices,
    .baseVertex = &baseVertex,
    .instanced = true
  });

  if (vertices) {
    float* v = vertices;

    // Ring
    for (int i = 0; i <= segments; i++) {
      float theta = i * (2 * M_PI) / segments;
      float X = cosf(theta);
      float Y = sinf(theta);
      memcpy(vertices, (float[16]) {
        r1 * X, r1 * Y, -.5f, X, Y, 0.f, 0.f, 0.f,
        r2 * X, r2 * Y,  .5f, X, Y, 0.f, 0.f, 0.f
      }, 16 * sizeof(float));
      vertices += 16;
    }

    // Top
    int top = (segments + 1) * 2 + baseVertex;
    if (capped && r1 != 0) {
      memcpy(vertices, (float[8]) { 0.f, 0.f, -.5f, 0.f, 0.f, -1.f, 0.f, 0.f }, 8 * sizeof(float));
      vertices += 8;
      for (int i = 0; i <= segments; i++) {
        int j = i * 2 * 8;
        memcpy(vertices, (float[8]) { v[j + 0], v[j + 1], v[j + 2], 0.f, 0.f, -1.f, 0.f, 0.f }, 8 * sizeof(float));
        vertices += 8;
      }
    }

    // Bottom
    int bot = (segments + 1) * 2 + (1 + segments + 1) * (capped && r1 != 0) + baseVertex;
    if (capped && r2 != 0) {
      memcpy(vertices, (float[8]) { 0.f, 0.f, .5f, 0.f, 0.f, 1.f, 0.f, 0.f }, 8 * sizeof(float));
      vertices += 8;
      for (int i = 0; i <= segments; i++) {
        int j = i * 2 * 8 + 8;
        memcpy(vertices, (float[8]) { v[j + 0], v[j + 1], v[j + 2], 0.f, 0.f, 1.f, 0.f, 0.f }, 8 * sizeof(float));
        vertices += 8;
      }
    }

    // Indices
    for (int i = 0; i < segments; i++) {
      int j = 2 * i + baseVertex;
      memcpy(indices, (uint16_t[6]) { j, j + 1, j + 2, j + 1, j + 3, j + 2 }, 6 * sizeof(uint16_t));
      indices += 6;

      if (capped && r1 != 0.f) {
        memcpy(indices, (uint16_t[3]) { top, top + i + 1, top + i + 2 }, 3 * sizeof(uint16_t));
        indices += 3;
      }

      if (capped && r2 != 0.f) {
        memcpy(indices, (uint16_t[3]) { bot, bot + i + 1, bot + i + 2 }, 3 * sizeof(uint16_t));
        indices += 3;
      }
    }
  }
}

void lovrGraphicsSphere(Material* material, mat4 transform, int segments) {
  float* vertices = NULL;
  uint16_t* indices = NULL;
  uint16_t baseVertex;

  lovrGraphicsBatch(&(BatchRequest) {
    .type = BATCH_SPHERE,
    .params.sphere.segments = segments,
    .drawMode = DRAW_TRIANGLES,
    .material = material,
    .transform = transform,
    .vertexCount = (segments + 1) * (segments + 1),
    .indexCount = segments * segments * 6,
    .vertices = &vertices,
    .indices = &indices,
    .baseVertex = &baseVertex,
    .instanced = true
  });

  if (vertices) {
    for (int i = 0; i <= segments; i++) {
      float v = i / (float) segments;
      float sinV = sinf(v * (float) M_PI);
      float cosV = cosf(v * (float) M_PI);
      for (int k = 0; k <= segments; k++) {
        float u = k / (float) segments;
        float x = sinf(u * 2.f * (float) M_PI) * sinV;
        float y = cosV;
        float z = -cosf(u * 2.f * (float) M_PI) * sinV;
        memcpy(vertices, ((float[8]) { x, y, z, x, y, z, u, 1.f - v }), 8 * sizeof(float));
        vertices += 8;
      }
    }

    for (int i = 0; i < segments; i++) {
      uint16_t offset0 = i * (segments + 1) + baseVertex;
      uint16_t offset1 = (i + 1) * (segments + 1) + baseVertex;
      for (int j = 0; j < segments; j++) {
        uint16_t i0 = offset0 + j;
        uint16_t i1 = offset1 + j;
        memcpy(indices, ((uint16_t[]) { i0, i1, i0 + 1, i1, i1 + 1, i0 + 1 }), 6 * sizeof(uint16_t));
        indices += 6;
      }
    }
  }
}

void lovrGraphicsSkybox(Texture* texture, float angle, float ax, float ay, float az) {
  TextureType type = lovrTextureGetType(texture);
  lovrAssert(type == TEXTURE_CUBE || type == TEXTURE_2D, "Only 2D and cube textures can be used as skyboxes");

  Pipeline pipeline = state.pipeline;
  pipeline.winding = WINDING_COUNTERCLOCKWISE;

  float transform[16] = MAT4_IDENTITY;
  mat4_rotate(transform, angle, ax, ay, az);

  float* vertices = NULL;

  lovrGraphicsBatch(&(BatchRequest) {
    .type = BATCH_SKYBOX,
    .drawMode = DRAW_TRIANGLE_STRIP,
    .shader = type == TEXTURE_CUBE ? SHADER_CUBE : SHADER_PANO,
    .pipeline = &pipeline,
    .transform = transform,
    .diffuseTexture = type == TEXTURE_2D ? texture : NULL,
    .environmentMap = type == TEXTURE_CUBE ? texture : NULL,
    .vertexCount = 4,
    .vertices = &vertices,
    .instanced = true
  });

  if (vertices) {
    static float vertexData[] = {
      -1, 1, 1,  0, 0, 0, 0, 0,
      -1, -1, 1, 0, 0, 0, 0, 0,
      1, 1, 1,   0, 0, 0, 0, 0,
      1, -1, 1,  0, 0, 0, 0, 0
    };

    memcpy(vertices, vertexData, sizeof(vertexData));
  }
}

void lovrGraphicsPrint(const char* str, size_t length, mat4 transform, float wrap, HorizontalAlign halign, VerticalAlign valign) {
  float width;
  uint32_t lineCount;
  uint32_t glyphCount;
  Font* font = lovrGraphicsGetFont();
  lovrFontMeasure(font, str, length, wrap, &width, &lineCount, &glyphCount);

  float scale = 1.f / font->pixelDensity;
  float offsetY = ((lineCount + 1) * font->rasterizer->height * font->lineHeight) * (valign / 2.f) * (font->flip ? -1 : 1);
  mat4_scale(transform, scale, scale, scale);
  mat4_translate(transform, 0.f, offsetY, 0.f);

  Pipeline pipeline = state.pipeline;
  pipeline.blendMode = pipeline.blendMode == BLEND_NONE ? BLEND_ALPHA : pipeline.blendMode;

  float* vertices;
  uint16_t* indices;
  uint16_t baseVertex;
  lovrGraphicsBatch(&(BatchRequest) {
    .type = BATCH_TEXT,
    .drawMode = DRAW_TRIANGLES,
    .shader = SHADER_FONT,
    .pipeline = &pipeline,
    .transform = transform,
    .diffuseTexture = font->texture,
    .vertexCount = glyphCount * 4,
    .indexCount = glyphCount * 6,
    .vertices = &vertices,
    .indices = &indices,
    .baseVertex = &baseVertex
  });

  lovrFontRender(font, str, length, wrap, halign, vertices, indices, baseVertex);
}

void lovrGraphicsFill(Texture* texture, float u, float v, float w, float h) {
  Pipeline pipeline = state.pipeline;
  pipeline.depthTest = COMPARE_NONE;
  pipeline.depthWrite = false;

  float* vertices = NULL;

  lovrGraphicsBatch(&(BatchRequest) {
    .type = BATCH_FILL,
    .params.fill = { .u = u, .v = v, .w = w, .h = h },
    .drawMode = DRAW_TRIANGLE_STRIP,
    .shader = SHADER_FILL,
    .pipeline = &pipeline,
    .diffuseTexture = texture,
    .vertexCount = 4,
    .vertices = &vertices
  });

  if (vertices) {
    memcpy(vertices, (float[32]) {
      -1, 1, 0,  0, 0, 0, u, v + h,
      -1, -1, 0, 0, 0, 0, u, v,
      1, 1, 0,   0, 0, 0, u + w, v + h,
      1, -1, 0,  0, 0, 0, u + w, v
    }, 32 * sizeof(float));
  }
}
