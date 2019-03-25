#include "graphics/graphics.h"
#include "data/rasterizer.h"
#include "event/event.h"
#include "filesystem/filesystem.h"
#include "math/math.h"
#include "util.h"
#include "lib/math.h"
#include "lib/stb/stb_image.h"
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
  [STREAM_DRAW_DATA] = 768
#else
  [STREAM_DRAW_DATA] = 256 * MAX_BATCHES * 2
#endif
};

static const size_t BUFFER_STRIDES[] = {
  [STREAM_VERTEX] = 8 * sizeof(float),
  [STREAM_INDEX] = sizeof(uint16_t),
  [STREAM_DRAW_ID] = sizeof(uint8_t),
  [STREAM_DRAW_DATA] = sizeof(DrawData)
};

static const BufferType BUFFER_TYPES[] = {
  [STREAM_VERTEX] = BUFFER_VERTEX,
  [STREAM_INDEX] = BUFFER_INDEX,
  [STREAM_DRAW_ID] = BUFFER_GENERIC,
  [STREAM_DRAW_DATA] = BUFFER_UNIFORM
};

static void lovrGraphicsInitBuffers() {
  for (int i = 0; i < MAX_BUFFER_ROLES; i++) {
    state.buffers[i] = lovrBufferCreate(BUFFER_COUNTS[i] * BUFFER_STRIDES[i], NULL, BUFFER_TYPES[i], USAGE_STREAM, false);
  }

  // Compute the max number of draws per batch, since the hard cap of 256 won't always fit in a UBO
  int maxBlockSize = lovrGpuGetLimits()->blockSize;
  state.maxDraws = MIN(maxBlockSize / sizeof(DrawData) / 64 * 64, 256);

  // The identity buffer is used for autoinstanced meshes and instanced primitives and maps the
  // instance ID to a vertex attribute.  Its contents never change, so they are initialized here.
  state.identityBuffer = lovrBufferCreate(256, NULL, BUFFER_VERTEX, USAGE_STATIC, false);
  uint8_t* id = lovrBufferMap(state.identityBuffer, 0);
  for (int i = 0; i < 256; i++) id[i] = i;
  lovrBufferFlushRange(state.identityBuffer, 0, 256);

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

    // If we roll over the vertex/index streams, we can't reuse their contents
    if (role == STREAM_VERTEX || role == STREAM_INDEX) {
      state.cachedGeometry.vertexCount = 0;
    }
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

static void writeGeometry(Batch* batch, float* vertices, uint16_t* indices, uint16_t I, uint32_t vertexCount, int n);

// Base

bool lovrGraphicsInit(bool gammaCorrect) {
  state.gammaCorrect = gammaCorrect;
  return false;
}

void lovrGraphicsDestroy() {
  if (!state.initialized) return;
  lovrGraphicsSetShader(NULL);
  lovrGraphicsSetFont(NULL);
  lovrGraphicsSetCanvas(NULL);
  for (int i = 0; i < MAX_DEFAULT_SHADERS; i++) {
    lovrRelease(state.defaultShaders[i]);
  }
  for (int i = 0; i < MAX_BUFFER_ROLES; i++) {
    lovrRelease(state.buffers[i]);
    for (int j = 0; j < MAX_LOCKS; j++) {
      lovrGpuDestroyLock(state.locks[i][j]);
    }
  }
  lovrRelease(state.mesh);
  lovrRelease(state.instancedMesh);
  lovrRelease(state.identityBuffer);
  lovrRelease(state.defaultMaterial);
  lovrRelease(state.defaultFont);
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
  flags->srgb = state.gammaCorrect;
#ifdef EMSCRIPTEN
  flags->vsync = 1;
#else
  flags->vsync = 0;
#endif
  lovrAssert(lovrPlatformCreateWindow(flags), "Could not create window");
  lovrPlatformOnWindowClose(onCloseWindow);
  lovrPlatformOnWindowResize(onResizeWindow);
  lovrPlatformGetFramebufferSize(&state.width, &state.height);
  lovrGpuInit(state.gammaCorrect, lovrGetProcAddress);
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
    mat4_perspective(state.camera.projection[0], .01f, 100.f, 67.f * M_PI / 180.f, (float) state.width / state.height);
    mat4_perspective(state.camera.projection[1], .01f, 100.f, 67.f * M_PI / 180.f, (float) state.width / state.height);
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
  return state.pipeline.lineWidth;
}

void lovrGraphicsSetLineWidth(uint8_t width) {
  lovrAssert(width > 0 && width <= 255, "Line width must be between 0 and 255");
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
  lovrRelease(state.shader);
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

  if (req->type == BATCH_MESH) {
    float* pose = req->params.mesh.pose ? req->params.mesh.pose : (float[]) MAT4_IDENTITY;
    int count = req->params.mesh.pose ? (MAX_BONES * 16) : 16;
    lovrShaderSetMatrices(shader, "lovrPose", pose, 0, count);
  }

  // Try to find an existing batch to use
  Batch* batch = NULL;
  if (req->type != BATCH_MESH || req->params.mesh.instances == 1) {
    for (int i = state.batchCount - 1; i >= 0; i--) {
      Batch* b = &state.batches[i];

      if (b->drawCount >= state.maxDraws) { continue; }
      if (!areBatchParamsEqual(req->type, b->type, &req->params, &b->params)) { continue; }
      if (b->canvas == canvas && b->shader == shader && !memcmp(&b->pipeline, pipeline, sizeof(Pipeline)) && b->material == material) {
        batch = b;
        break;
      }

      // Draws can't be reordered when blending is on, depth test is off, or either of the batches
      // are streaming their vertices (since buffers are append-only)
      if (b->pipeline.blendMode != BLEND_NONE || pipeline->blendMode != BLEND_NONE) { break; }
      if (b->pipeline.depthTest == COMPARE_NONE || pipeline->depthTest == COMPARE_NONE) { break; }
      if (b->vertexCount > 0 && req->vertexCount > 0) { break; }
    }
  }

  size_t streamRequirements[] = {
    [STREAM_VERTEX] = req->vertexCount,
    [STREAM_INDEX] = req->indexCount,
    [STREAM_DRAW_ID] = req->vertexCount,
    [STREAM_DRAW_DATA] = 0
  };

  if (!batch) {
    streamRequirements[STREAM_DRAW_DATA] = state.maxDraws;
    if (state.batchCount >= MAX_BATCHES) {
      lovrGraphicsFlush();
    }
  }

  for (int i = 0; i < MAX_BUFFER_ROLES; i++) {
    if (streamRequirements[i] > 0 && state.cursors[i] + streamRequirements[i] > BUFFER_COUNTS[i]) {
      uint32_t oldCursor = state.cursors[i];
      lovrGraphicsFlush();
      state.locks[i][MAX_LOCKS - 1] = lovrGpuLock();
      state.cursors[i] = state.cursors[i] >= oldCursor ? 0 : state.cursors[i];
      batch = NULL;
      streamRequirements[STREAM_DRAW_DATA] = state.maxDraws;
      state.cachedGeometry.vertexCount = 0;
      i = 0;
    } else {
      streamRequirements[i] = 0;
    }
  }

  // Start a new batch
  if (!batch) {
    DrawData* drawData = lovrGraphicsMapBuffer(STREAM_DRAW_DATA, state.maxDraws);
    batch = &state.batches[state.batchCount++];
    *batch = (Batch) {
      .type = req->type,
      .params = req->params,
      .canvas = canvas,
      .shader = shader,
      .pipeline = *pipeline,
      .material = material,
      .vertexStart = req->vertexCount > 0 ? state.cursors[STREAM_VERTEX] : 0,
      .vertexCount = 0,
      .indexStart = req->indexCount > 0 ? state.cursors[STREAM_INDEX] : 0,
      .indexCount = 0,
      .drawStart = state.cursors[STREAM_DRAW_DATA],
      .drawCount = 0,
      .drawData = drawData
    };

    state.cursors[STREAM_DRAW_DATA] += state.maxDraws;
  }

  // Transform
  if (req->transform) {
    float transform[16];
    mat4_multiply(mat4_init(transform, state.transforms[state.transform]), req->transform);
    memcpy(batch->drawData[batch->drawCount].transform, transform, 16 * sizeof(float));
  } else {
    memcpy(batch->drawData[batch->drawCount].transform, state.transforms[state.transform], 16 * sizeof(float));
  }

  // Color
  Color color = state.color;
  gammaCorrectColor(&color);
  batch->drawData[batch->drawCount].color = color;

  // Handle streams
  uint8_t* ids = NULL;
  if (req->vertexCount > 0) {
    *(req->vertices) = lovrGraphicsMapBuffer(STREAM_VERTEX, req->vertexCount);
    ids = lovrGraphicsMapBuffer(STREAM_DRAW_ID, req->vertexCount);
    memset(ids, batch->drawCount, req->vertexCount * sizeof(uint8_t));

    if (req->indexCount > 0) {
      *(req->indices) = lovrGraphicsMapBuffer(STREAM_INDEX, req->indexCount);
      *(req->baseVertex) = state.cursors[STREAM_VERTEX];
    }

    batch->vertexCount += req->vertexCount;
    batch->indexCount += req->indexCount;

    state.cursors[STREAM_VERTEX] += req->vertexCount;
    state.cursors[STREAM_DRAW_ID] += req->vertexCount;
    state.cursors[STREAM_INDEX] += req->indexCount;
  }

  batch->drawCount++;
}

void lovrGraphicsFlush() {
  if (state.batchCount == 0) {
    return;
  }

  // Prevent infinite flushing >_>
  int batchCount = state.batchCount;
  state.batchCount = 0;

  for (int b = 0; b < batchCount; b++) {
    Batch* batch = &state.batches[b];
    BatchParams* params = &batch->params;

    // Resolve geometry
    Mesh* mesh = NULL;
    DrawMode drawMode;
    bool instanced = batch->vertexCount == 0 && batch->drawCount >= 4;
    int instances = instanced ? batch->drawCount : 1;
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
    switch (batch->type) {
      case BATCH_POINTS: mesh = state.mesh; drawMode = DRAW_POINTS; break;
      case BATCH_LINES: mesh = state.mesh; drawMode = DRAW_LINE_STRIP; break;
      case BATCH_TRIANGLES:
        mesh = state.mesh;
        drawMode = params->triangles.style == STYLE_LINE ? DRAW_LINE_LOOP : DRAW_TRIANGLES;
        break;

      case BATCH_PLANE:
        vertexCount = 4;
        indexCount = params->plane.style == STYLE_LINE ? 5 : 6;
        mesh = instanced ? state.instancedMesh : state.mesh;
        drawMode = params->plane.style == STYLE_LINE ? DRAW_LINE_LOOP : DRAW_TRIANGLES;
        break;

      case BATCH_BOX:
        vertexCount = params->box.style == STYLE_LINE ? 8 : 24;
        indexCount = params->box.style == STYLE_LINE ? 24 : 36;
        mesh = instanced ? state.instancedMesh : state.mesh;
        drawMode = params->box.style == STYLE_LINE ? DRAW_LINES : DRAW_TRIANGLES;
        break;

      case BATCH_ARC: {
        bool hasCenterPoint = params->arc.mode == ARC_MODE_PIE && fabsf(params->arc.r1 - params->arc.r2) < 2.f * (float) M_PI;
        vertexCount = params->arc.segments + 1 + hasCenterPoint;
        indexCount = vertexCount + 1;
        mesh = instanced ? state.instancedMesh : state.mesh;
        drawMode = params->arc.style == STYLE_LINE ? (params->arc.mode == ARC_MODE_OPEN ? DRAW_LINE_STRIP : DRAW_LINE_LOOP) : DRAW_TRIANGLE_FAN;
        break;
      }

      case BATCH_CYLINDER: {
        bool r1 = params->cylinder.r1 > 0;
        bool r2 = params->cylinder.r2 > 0;
        bool capped = params->cylinder.capped;
        int segments = params->cylinder.segments;
        vertexCount = ((capped && r1) * (segments + 2) + (capped && r2) * (segments + 2) + 2 * (segments + 1));
        indexCount = 3 * segments * ((capped && r1) + (capped && r2) + 2);
        mesh = instanced ? state.instancedMesh : state.mesh;
        drawMode = DRAW_TRIANGLES;
        break;
      }

      case BATCH_SPHERE: {
        int segments = params->sphere.segments;
        vertexCount = (segments + 1) * (segments + 1);
        indexCount = segments * segments * 6;
        mesh = instanced ? state.instancedMesh : state.mesh;
        drawMode = DRAW_TRIANGLES;
        break;
      }

      case BATCH_SKYBOX:
        vertexCount = 4;
        instanced = false;
        instances = 1;
        mesh = state.mesh;
        drawMode = DRAW_TRIANGLE_STRIP;
        break;

      case BATCH_TEXT:
        mesh = state.mesh;
        drawMode = DRAW_TRIANGLES;
        break;

      case BATCH_FILL:
        vertexCount = 4;
        instanced = false;
        instances = 1;
        mesh = state.mesh;
        drawMode = DRAW_TRIANGLE_STRIP;
        break;

      case BATCH_MESH:
        mesh = params->mesh.object;
        drawMode = lovrMeshGetDrawMode(mesh);
        if (params->mesh.instances > 1) {
          lovrMeshSetAttributeEnabled(mesh, "lovrDrawID", false);
          instances = params->mesh.instances;
        } else {
          lovrMeshSetAttributeEnabled(mesh, "lovrDrawID", true);
          instances = batch->drawCount;
        }
        break;

      default: break;
    }

    // Write geometry
    if (vertexCount > 0) {
      int n = instanced ? 1 : batch->drawCount;

      // Try to re-use the geometry from the last batch
      Batch* cached = &state.cachedGeometry;
      if (areBatchParamsEqual(batch->type, cached->type, &batch->params, &cached->params) && cached->vertexCount >= vertexCount * n) {
        batch->vertexStart = cached->vertexStart;
        batch->indexStart = cached->indexStart;
        batch->vertexCount = vertexCount * n;
        batch->indexCount = indexCount * n;
      } else {
        float* vertices = lovrGraphicsMapBuffer(STREAM_VERTEX, vertexCount * n);
        uint16_t* indices = lovrGraphicsMapBuffer(STREAM_INDEX, indexCount * n);
        uint16_t I = (uint16_t) state.cursors[STREAM_VERTEX];

        cached->type = batch->type;
        cached->params = batch->params;
        batch->vertexStart = cached->vertexStart = state.cursors[STREAM_VERTEX];
        batch->indexStart = cached->indexStart = state.cursors[STREAM_INDEX];
        batch->vertexCount = cached->vertexCount = vertexCount * n;
        batch->indexCount = cached->indexCount = indexCount * n;

        uint8_t* ids = lovrGraphicsMapBuffer(STREAM_DRAW_ID, batch->vertexCount);
        for (int i = 0; i < n; i++) {
          memset(ids, i, vertexCount * sizeof(uint8_t));
          ids += vertexCount;
        }

        state.cursors[STREAM_VERTEX] += batch->vertexCount;
        state.cursors[STREAM_INDEX] += batch->indexCount;
        state.cursors[STREAM_DRAW_ID] += batch->vertexCount;

        writeGeometry(batch, vertices, indices, I, vertexCount, n);
      }
    }

    // Flush vertex buffer
    if (batch->vertexCount > 0) {
      size_t stride = BUFFER_STRIDES[STREAM_VERTEX];
      lovrBufferFlushRange(state.buffers[STREAM_VERTEX], batch->vertexStart * stride, batch->vertexCount * stride);

      if (!instanced) {
        lovrBufferFlushRange(state.buffers[STREAM_DRAW_ID], batch->vertexStart, batch->vertexCount);
      }
    }

    // Flush index buffer
    if (batch->indexCount > 0) {
      size_t stride = BUFFER_STRIDES[STREAM_INDEX];
      lovrBufferFlushRange(state.buffers[STREAM_INDEX], batch->indexStart * stride, batch->indexCount * stride);
    }

    // Flush draw data buffer
    size_t drawDataOffset = batch->drawStart * BUFFER_STRIDES[STREAM_DRAW_DATA];
    size_t drawDataSize = batch->drawCount * BUFFER_STRIDES[STREAM_DRAW_DATA];
    lovrBufferFlushRange(state.buffers[STREAM_DRAW_DATA], drawDataOffset, drawDataSize);
    lovrShaderSetBlock(batch->shader, "lovrDrawData", state.buffers[STREAM_DRAW_DATA], drawDataOffset, state.maxDraws * BUFFER_STRIDES[STREAM_DRAW_DATA], ACCESS_READ);

    // Uniforms
    lovrMaterialBind(batch->material, batch->shader);
    lovrShaderSetMatrices(batch->shader, "lovrViews", state.camera.viewMatrix[0], 0, 32);
    lovrShaderSetMatrices(batch->shader, "lovrProjections", state.camera.projection[0], 0, 32);

    if (drawMode == DRAW_POINTS) {
      lovrShaderSetFloats(batch->shader, "lovrPointSize", &state.pointSize, 0, 1);
    }

    uint32_t rangeStart, rangeCount;
    if (batch->type == BATCH_MESH) {
      rangeStart = batch->params.mesh.rangeStart;
      rangeCount = batch->params.mesh.rangeCount;
    } else {
      rangeStart = batch->indexCount ? batch->indexStart : batch->vertexStart;
      rangeCount = batch->indexCount ? batch->indexCount : batch->vertexCount;
      if (batch->indexCount > 0) {
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
      .drawMode = drawMode,
      .instances = instances,
      .rangeStart = rangeStart,
      .rangeCount = rangeCount,
      .width = batch->canvas ? lovrCanvasGetWidth(batch->canvas) : state.width,
      .height = batch->canvas ? lovrCanvasGetHeight(batch->canvas) : state.height,
      .stereo = batch->type != BATCH_FILL && (batch->canvas ? lovrCanvasIsStereo(batch->canvas) : state.camera.stereo)
    });

    // Pop lock and drop it

    if (batch->vertexCount > 0) {
      size_t lockSize = BUFFER_COUNTS[STREAM_VERTEX] / MAX_LOCKS + 1;
      size_t firstLock = batch->vertexStart / lockSize;
      size_t lastLock = (batch->vertexStart + batch->vertexCount) / lockSize;
      for (size_t i = firstLock; i < lastLock; i++) {
        state.locks[STREAM_VERTEX][i] = lovrGpuLock();
        if (!instanced) {
          state.locks[STREAM_DRAW_ID][i] = lovrGpuLock();
        }
      }
    }

    if (batch->indexCount > 0) {
      size_t lockSize = BUFFER_COUNTS[STREAM_INDEX] / MAX_LOCKS + 1;
      size_t firstLock = batch->indexStart / lockSize;
      size_t lastLock = (batch->indexStart + batch->indexCount) / lockSize;
      for (size_t i = firstLock; i < lastLock; i++) {
        state.locks[STREAM_INDEX][i] = lovrGpuLock();
      }
    }

    if (batch->drawCount > 0) {
      size_t lockSize = BUFFER_COUNTS[STREAM_DRAW_DATA] / MAX_LOCKS;
      size_t firstLock = batch->drawStart / lockSize;
      size_t lastLock = MIN(batch->drawStart + state.maxDraws, BUFFER_COUNTS[STREAM_DRAW_DATA] - 1) / lockSize;
      for (size_t i = firstLock; i < lastLock; i++) {
        state.locks[STREAM_DRAW_DATA][i] = lovrGpuLock();
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

void lovrGraphicsPlane(DrawStyle style, Material* material, mat4 transform) {
  lovrGraphicsBatch(&(BatchRequest) {
    .type = BATCH_PLANE,
    .params.plane.style = style,
    .material = material,
    .transform = transform
  });
}

void lovrGraphicsBox(DrawStyle style, Material* material, mat4 transform) {
  lovrGraphicsBatch(&(BatchRequest) {
    .type = BATCH_BOX,
    .params.box.style = style,
    .material = material,
    .transform = transform
  });
}

void lovrGraphicsArc(DrawStyle style, ArcMode mode, Material* material, mat4 transform, float r1, float r2, int segments) {
  if (fabsf(r1 - r2) >= 2.f * (float) M_PI) {
    r1 = 0.f;
    r2 = 2.f * (float) M_PI;
  }

  lovrGraphicsBatch(&(BatchRequest) {
    .type = BATCH_ARC,
    .params.arc.r1 = r1,
    .params.arc.r2 = r2,
    .params.arc.mode = mode,
    .params.arc.style = style,
    .params.arc.segments = segments,
    .material = material,
    .transform = transform
  });
}

void lovrGraphicsCircle(DrawStyle style, Material* material, mat4 transform, int segments) {
  lovrGraphicsArc(style, ARC_MODE_OPEN, material, transform, 0, 2. * M_PI, segments);
}

void lovrGraphicsCylinder(Material* material, mat4 transform, float r1, float r2, bool capped, int segments) {
  float length = vec3_length((float[3]) { transform[8], transform[9], transform[10] });

  lovrGraphicsBatch(&(BatchRequest) {
    .type = BATCH_CYLINDER,
    .params.cylinder.r1 = r1 / length,
    .params.cylinder.r2 = r2 / length,
    .params.cylinder.capped = capped,
    .params.cylinder.segments = segments,
    .material = material,
    .transform = transform
  });
}

void lovrGraphicsSphere(Material* material, mat4 transform, int segments) {
  lovrGraphicsBatch(&(BatchRequest) {
    .type = BATCH_SPHERE,
    .params.sphere.segments = segments,
    .material = material,
    .transform = transform
  });
}

void lovrGraphicsSkybox(Texture* texture, float angle, float ax, float ay, float az) {
  TextureType type = lovrTextureGetType(texture);
  lovrAssert(type == TEXTURE_CUBE || type == TEXTURE_2D, "Only 2D and cube textures can be used as skyboxes");

  Pipeline pipeline = state.pipeline;
  pipeline.winding = WINDING_COUNTERCLOCKWISE;

  float transform[16] = MAT4_IDENTITY;
  mat4_rotate(transform, angle, ax, ay, az);

  lovrGraphicsBatch(&(BatchRequest) {
    .type = BATCH_SKYBOX,
    .shader = type == TEXTURE_CUBE ? SHADER_CUBE : SHADER_PANO,
    .pipeline = &pipeline,
    .transform = transform,
    .diffuseTexture = type == TEXTURE_2D ? texture : NULL,
    .environmentMap = type == TEXTURE_CUBE ? texture : NULL
  });
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

  lovrGraphicsBatch(&(BatchRequest) {
    .type = BATCH_FILL,
    .params.fill = { .u = u, .v = v, .w = w, .h = h },
    .shader = SHADER_FILL,
    .pipeline = &pipeline,
    .diffuseTexture = texture
  });
}

static void writeGeometry(Batch* batch, float* vertices, uint16_t* indices, uint16_t I, uint32_t vertexCount, int n) {
  BatchParams* params = &batch->params;
  switch (batch->type) {
    case BATCH_PLANE:
      if (params->plane.style == STYLE_LINE) {
        for (int i = 0; i < n; i++) {
          memcpy(vertices, (float[32]) {
            -.5, .5, 0,  0, 0, 0, 0, 0,
            .5, .5, 0,   0, 0, 0, 0, 0,
            .5, -.5, 0,  0, 0, 0, 0, 0,
            -.5, -.5, 0, 0, 0, 0, 0, 0
          }, 32 * sizeof(float));
          vertices += 32;

          memcpy(indices, (uint16_t[5]) { 0xffff, I + 0, I + 1, I + 2, I + 3 }, 5 * sizeof(uint16_t));
          I += vertexCount;
          indices += 5;
        }
      } else {
        for (int i = 0; i < n; i++) {
          memcpy(vertices, (float[32]) {
            -.5, .5, 0,  0, 0, -1, 0, 1,
            -.5, -.5, 0, 0, 0, -1, 0, 0,
            .5, .5, 0,   0, 0, -1, 1, 1,
            .5, -.5, 0,  0, 0, -1, 1, 0
          }, 32 * sizeof(float));
          vertices += 32;

          memcpy(indices, (uint16_t[6]) { I + 0, I + 1, I + 2, I + 2, I + 1, I + 3 }, 6 * sizeof(uint16_t));
          I += vertexCount;
          indices += 6;
        }
      }
      break;

    case BATCH_BOX:
      if (params->box.style == STYLE_LINE) {
        for (int i = 0; i < n; i++) {
          memcpy(vertices, (float[64]) {
            -.5, .5, -.5,  0, 0, 0, 0, 0, // Front
            .5, .5, -.5,   0, 0, 0, 0, 0,
            .5, -.5, -.5,  0, 0, 0, 0, 0,
            -.5, -.5, -.5, 0, 0, 0, 0, 0,
            -.5, .5, .5,   0, 0, 0, 0, 0, // Back
            .5, .5, .5,    0, 0, 0, 0, 0,
            .5, -.5, .5,   0, 0, 0, 0, 0,
            -.5, -.5, .5,  0, 0, 0, 0, 0
          }, 64 * sizeof(float));
          vertices += 64;

          memcpy(indices, (uint16_t[24]) {
            I + 0, I + 1, I + 1, I + 2, I + 2, I + 3, I + 3, I + 0, // Front
            I + 4, I + 5, I + 5, I + 6, I + 6, I + 7, I + 7, I + 4, // Back
            I + 0, I + 4, I + 1, I + 5, I + 2, I + 6, I + 3, I + 7  // Connections
          }, 24 * sizeof(uint16_t));
          indices += 24;
          I += 8;
        }
      } else {
        for (int i = 0; i < n; i++) {
          memcpy(vertices, (float[192]) {
            -.5, -.5, -.5,  0, 0, -1, 0, 0, // Front
            -.5, .5, -.5,   0, 0, -1, 0, 1,
            .5, -.5, -.5,   0, 0, -1, 1, 0,
            .5, .5, -.5,    0, 0, -1, 1, 1,
            .5, .5, -.5,    1, 0, 0,  0, 1, // Right
            .5, .5, .5,     1, 0, 0,  1, 1,
            .5, -.5, -.5,   1, 0, 0,  0, 0,
            .5, -.5, .5,    1, 0, 0,  1, 0,
            .5, -.5, .5,    0, 0, 1,  0, 0, // Back
            .5, .5, .5,     0, 0, 1,  0, 1,
            -.5, -.5, .5,   0, 0, 1,  1, 0,
            -.5, .5, .5,    0, 0, 1,  1, 1,
            -.5, .5, .5,   -1, 0, 0,  0, 1, // Left
            -.5, .5, -.5,  -1, 0, 0,  1, 1,
            -.5, -.5, .5,  -1, 0, 0,  0, 0,
            -.5, -.5, -.5, -1, 0, 0,  1, 0,
            -.5, -.5, -.5,  0, -1, 0, 0, 0, // Bottom
            .5, -.5, -.5,   0, -1, 0, 1, 0,
            -.5, -.5, .5,   0, -1, 0, 0, 1,
            .5, -.5, .5,    0, -1, 0, 1, 1,
            -.5, .5, -.5,   0, 1, 0,  0, 1, // Top
            -.5, .5, .5,    0, 1, 0,  0, 0,
            .5, .5, -.5,    0, 1, 0,  1, 1,
            .5, .5, .5,     0, 1, 0,  1, 0
          }, 192 * sizeof(float));
          vertices += 192;

          memcpy(indices, (uint16_t[36]) {
            I + 0,  I + 1,   I + 2,  I + 2,  I + 1,  I + 3,
            I + 4,  I + 5,   I + 6,  I + 6,  I + 5,  I + 7,
            I + 8,  I + 9,  I + 10, I + 10,  I + 9, I + 11,
            I + 12, I + 13, I + 14, I + 14, I + 13, I + 15,
            I + 16, I + 17, I + 18, I + 18, I + 17, I + 19,
            I + 20, I + 21, I + 22, I + 22, I + 21, I + 23
          }, 36 * sizeof(uint16_t));
          I += vertexCount;
          indices += 36;
        }
      }
      break;

    case BATCH_ARC: {
      float r1 = params->arc.r1;
      float r2 = params->arc.r2;
      int segments = params->arc.segments;
      bool hasCenterPoint = params->arc.mode == ARC_MODE_PIE && fabsf(r1 - r2) < 2.f * (float) M_PI;

      for (int i = 0; i < n; i++) {
        if (hasCenterPoint) {
          memcpy(vertices, ((float[]) { 0.f, 0.f, 0.f, 0.f, 0.f, 1.f, .5f, .5f }), 8 * sizeof(float));
          vertices += 8;
        }

        float theta = r1;
        float angleShift = (r2 - r1) / (float) segments;

        for (int i = 0; i <= segments; i++) {
          float x = cosf(theta) * .5f;
          float y = sinf(theta) * .5f;
          memcpy(vertices, ((float[]) { x, y, 0.f, 0.f, 0.f, 1.f, x + .5f, 1.f - (y + .5f) }), 8 * sizeof(float));
          vertices += 8;
          theta += angleShift;
        }

        *indices++ = 0xffff;
        for (uint32_t i = 0; i < vertexCount; i++) {
          *indices++ = I + i;
        }
        I += vertexCount;
      }
      break;
    }

    case BATCH_CYLINDER: {
      bool capped = params->cylinder.capped;
      float r1 = params->cylinder.r1;
      float r2 = params->cylinder.r2;
      int segments = params->cylinder.segments;

      for (int i = 0; i < n; i++) {
        float* v = vertices;

        // Ring
        for (int j = 0; j <= segments; j++) {
          float theta = j * (2 * M_PI) / segments;
          float X = cosf(theta);
          float Y = sinf(theta);
          memcpy(vertices, (float[16]) {
            r1 * X, r1 * Y, -.5f, X, Y, 0.f, 0.f, 0.f,
            r2 * X, r2 * Y,  .5f, X, Y, 0.f, 0.f, 0.f
          }, 16 * sizeof(float));
          vertices += 16;
        }

        // Top
        int top = (segments + 1) * 2 + I;
        if (capped && r1 != 0) {
          memcpy(vertices, (float[8]) { 0.f, 0.f, -.5f, 0.f, 0.f, -1.f, 0.f, 0.f }, 8 * sizeof(float));
          vertices += 8;
          for (int j = 0; j <= segments; j++) {
            int k = j * 2 * 8;
            memcpy(vertices, (float[8]) { v[k + 0], v[k + 1], v[k + 2], 0.f, 0.f, -1.f, 0.f, 0.f }, 8 * sizeof(float));
            vertices += 8;
          }
        }

        // Bottom
        int bot = (segments + 1) * 2 + (1 + segments + 1) * (capped && r1 != 0) + I;
        if (capped && r2 != 0) {
          memcpy(vertices, (float[8]) { 0.f, 0.f, .5f, 0.f, 0.f, 1.f, 0.f, 0.f }, 8 * sizeof(float));
          vertices += 8;
          for (int j = 0; j <= segments; j++) {
            int k = j * 2 * 8 + 8;
            memcpy(vertices, (float[8]) { v[k + 0], v[k + 1], v[k + 2], 0.f, 0.f, 1.f, 0.f, 0.f }, 8 * sizeof(float));
            vertices += 8;
          }
        }

        // Indices
        for (int i = 0; i < segments; i++) {
          int j = 2 * i + I;
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

        I += vertexCount;
      }
      break;
    }

    case BATCH_SPHERE: {
      int segments = params->sphere.segments;
      for (int i = 0; i < n; i++) {
        for (int j = 0; j <= segments; j++) {
          float v = j / (float) segments;
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

        for (int j = 0; j < segments; j++) {
          uint16_t offset0 = j * (segments + 1) + I;
          uint16_t offset1 = (j + 1) * (segments + 1) + I;
          for (int k = 0; k < segments; k++) {
            uint16_t i0 = offset0 + k;
            uint16_t i1 = offset1 + k;
            memcpy(indices, ((uint16_t[]) { i0, i1, i0 + 1, i1, i1 + 1, i0 + 1 }), 6 * sizeof(uint16_t));
            indices += 6;
          }
        }

        I += vertexCount;
      }
      break;
    }

    case BATCH_SKYBOX:
      for (int i = 0; i < n; i++) {
        memcpy(vertices, (float[32]) {
          -1, 1, 1,  0, 0, 0, 0, 0,
          -1, -1, 1, 0, 0, 0, 0, 0,
          1, 1, 1,   0, 0, 0, 0, 0,
          1, -1, 1,  0, 0, 0, 0, 0
        }, 32 * sizeof(float));
        vertices += 32;
      }
      break;

    case BATCH_FILL:
      for (int i = 0; i < n; i++) {
        float u = params->fill.u;
        float v = params->fill.v;
        float w = params->fill.w;
        float h = params->fill.h;
        memcpy(vertices, (float[32]) {
          -1, 1, 0,  0, 0, 0, u, v + h,
          -1, -1, 0, 0, 0, 0, u, v,
          1, 1, 0,   0, 0, 0, u + w, v + h,
          1, -1, 0,  0, 0, 0, u + w, v
        }, 32 * sizeof(float));
        vertices += 32;
      }
      break;

    default: break;
  }
}
