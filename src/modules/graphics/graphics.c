#include "graphics/graphics.h"
#include "graphics/buffer.h"
#include "graphics/canvas.h"
#include "graphics/material.h"
#include "graphics/mesh.h"
#include "graphics/shader.h"
#include "graphics/texture.h"
#include "data/rasterizer.h"
#include "event/event.h"
#include "math/math.h"
#include "core/maf.h"
#include "core/ref.h"
#include "core/util.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_TRANSFORMS 64
#define MAX_BATCHES 4
#define MAX_DRAWS 256

typedef enum {
  STREAM_VERTEX,
  STREAM_DRAWID,
  STREAM_INDEX,
  STREAM_MODEL,
  STREAM_COLOR,
  STREAM_FRAME,
  MAX_STREAMS
} StreamType;

typedef enum {
  BATCH_POINTS,
  BATCH_LINES,
  BATCH_TRIANGLES,
  BATCH_PLANE,
  BATCH_BOX,
  BATCH_ARC,
  BATCH_SPHERE,
  BATCH_CYLINDER,
  BATCH_SKYBOX,
  BATCH_TEXT,
  BATCH_FILL,
  BATCH_MESH
} BatchType;

typedef union {
  struct { DrawStyle style; } triangles;
  struct { DrawStyle style; } plane;
  struct { DrawStyle style; } box;
  struct { DrawStyle style; ArcMode mode; float r1; float r2; int segments; } arc;
  struct { float r1; float r2; bool capped; int segments; } cylinder;
  struct { int segments; } sphere;
  struct { float u; float v; float w; float h; } fill;
  struct { uint32_t rangeStart; uint32_t rangeCount; uint32_t instances; float* pose; } mesh;
} BatchParams;

typedef struct {
  BatchType type;
  BatchParams params;
  DrawMode topology;
  DefaultShader shader;
  Mesh* mesh;
  Pipeline* pipeline;
  Material* material;
  Texture* texture;
  mat4 transform;
  uint32_t vertexCount;
  uint32_t indexCount;
  float** vertices;
  uint16_t** indices;
  uint16_t* baseVertex;
  bool instanced;
} BatchRequest;

typedef struct {
  BatchType type;
  BatchParams params;
  DrawCommand draw;
  Material* material;
  mat4 transforms;
  Color* colors;
  uint32_t drawStart;
  uint32_t drawCount;
  bool indexed;
} Batch;

typedef struct {
  float viewMatrix[2][16];
  float projection[2][16];
} FrameData;

static struct {
  bool initialized;
  bool debug;
  int width;
  int height;
  Canvas* backbuffer;
  FrameData frameData;
  bool frameDataDirty;
  Canvas* defaultCanvas;
  Shader* defaultShaders[MAX_DEFAULT_SHADERS][2];
  Material* defaultMaterial;
  Font* defaultFont;
  TextureFilter defaultFilter;
  float transforms[MAX_TRANSFORMS][16];
  int transform;
  Color backgroundColor;
  Color linearBackgroundColor;
  Canvas* canvas;
  Color color;
  Color linearColor;
  Font* font;
  Pipeline pipeline;
  float pointSize;
  Shader* shader;
  Mesh* mesh;
  Mesh* instancedMesh;
  Buffer* identityBuffer;
  Buffer* buffers[MAX_STREAMS];
  uint32_t head[MAX_STREAMS];
  uint32_t tail[MAX_STREAMS];
  Batch batches[MAX_BATCHES];
  uint8_t batchCount;
} state;

static const uint32_t bufferCount[] = {
  [STREAM_VERTEX] = (1 << 16) - 1,
  [STREAM_DRAWID] = (1 << 16) - 1,
  [STREAM_INDEX] = 1 << 16,
#if defined(LOVR_WEBGL) // Work around bugs where big UBOs don't work
  [STREAM_MODEL] = MAX_DRAWS,
  [STREAM_COLOR] = MAX_DRAWS,
#else
  [STREAM_MODEL] = MAX_DRAWS * MAX_BATCHES,
  [STREAM_COLOR] = MAX_DRAWS * MAX_BATCHES,
#endif
  [STREAM_FRAME] = 4
};

static const size_t bufferStride[] = {
  [STREAM_VERTEX] = 8 * sizeof(float),
  [STREAM_DRAWID] = sizeof(uint8_t),
  [STREAM_INDEX] = sizeof(uint16_t),
  [STREAM_MODEL] = 16 * sizeof(float),
  [STREAM_COLOR] = 4 * sizeof(float),
  [STREAM_FRAME] = sizeof(FrameData)
};

static const BufferType bufferType[] = {
  [STREAM_VERTEX] = BUFFER_VERTEX,
  [STREAM_DRAWID] = BUFFER_GENERIC,
  [STREAM_INDEX] = BUFFER_INDEX,
  [STREAM_MODEL] = BUFFER_UNIFORM,
  [STREAM_COLOR] = BUFFER_UNIFORM,
  [STREAM_FRAME] = BUFFER_UNIFORM
};

static void gammaCorrect(Color* color) {
  color->r = lovrMathGammaToLinear(color->r);
  color->g = lovrMathGammaToLinear(color->g);
  color->b = lovrMathGammaToLinear(color->b);
}

static void onQuitRequest(void) {
  lovrEventPush((Event) { .type = EVENT_QUIT, .data.quit = { .exitCode = 0 } });
}

static void onResizeWindow(int width, int height) {
  state.width = width;
  state.height = height;
  lovrCanvasSetWidth(state.defaultCanvas, width);
  lovrCanvasSetHeight(state.defaultCanvas, height);
  lovrEventPush((Event) { .type = EVENT_RESIZE, .data.resize = { width, height } });
}

static void* lovrGraphicsMapBuffer(StreamType type, uint32_t count) {
  lovrAssert(count <= bufferCount[type], "Whoa there!  Tried to get %d elements from a buffer that only has %d elements.", count, bufferCount[type]);

  if (state.head[type] + count > bufferCount[type]) {
    lovrAssert(state.batchCount == 0, "Internal error: Batches still exist during Buffer reset");
    lovrBufferDiscard(state.buffers[type]);
    state.tail[type] = 0;
    state.head[type] = 0;
  }

  return lovrBufferMap(state.buffers[type], state.head[type] * bufferStride[type], true);
}

// Base

bool lovrGraphicsInit(bool debug) {
  state.debug = debug;
  return false; // See lovrGraphicsCreateWindow for actual initialization
}

void lovrGraphicsDestroy() {
  if (!state.initialized) return;
  lovrGraphicsSetShader(NULL);
  lovrGraphicsSetFont(NULL);
  lovrGraphicsSetCanvas(NULL);
  for (int i = 0; i < MAX_DEFAULT_SHADERS; i++) {
    lovrRelease(Shader, state.defaultShaders[i][false]);
    lovrRelease(Shader, state.defaultShaders[i][true]);
  }
  for (int i = 0; i < MAX_STREAMS; i++) {
    lovrRelease(Buffer, state.buffers[i]);
  }
  lovrRelease(Mesh, state.mesh);
  lovrRelease(Mesh, state.instancedMesh);
  lovrRelease(Buffer, state.identityBuffer);
  lovrRelease(Material, state.defaultMaterial);
  lovrRelease(Font, state.defaultFont);
  lovrRelease(Canvas, state.defaultCanvas);
  lovrGpuDestroy();
  memset(&state, 0, sizeof(state));
}

void lovrGraphicsPresent() {
  lovrGraphicsFlush();
  lovrPlatformSwapBuffers();
  lovrGpuPresent();
}

void lovrGraphicsCreateWindow(WindowFlags* flags) {
  flags->debug = state.debug;
  lovrAssert(!state.initialized, "Window is already created");
  lovrAssert(lovrPlatformCreateWindow(flags), "Could not create window");
  lovrPlatformSetSwapInterval(flags->vsync); // Force vsync in case lovr.headset changed it in a previous restart
  lovrPlatformOnQuitRequest(onQuitRequest);
  lovrPlatformOnWindowResize(onResizeWindow);
  lovrPlatformGetFramebufferSize(&state.width, &state.height);
  lovrGpuInit(lovrPlatformGetProcAddress, state.debug);

  state.defaultCanvas = lovrCanvasCreateFromHandle(state.width, state.height, (CanvasFlags) { .stereo = false }, 0, 0, 0, 1, true);
  state.backbuffer = state.defaultCanvas;

  for (int i = 0; i < MAX_STREAMS; i++) {
    state.buffers[i] = lovrBufferCreate(bufferCount[i] * bufferStride[i], NULL, bufferType[i], USAGE_STREAM, false);
  }

  // The identity buffer is used for autoinstanced meshes and instanced primitives and maps the
  // instance ID to a vertex attribute.  Its contents never change, so they are initialized here.
  state.identityBuffer = lovrBufferCreate(MAX_DRAWS * sizeof(uint8_t), NULL, BUFFER_VERTEX, USAGE_STATIC, false);
  uint8_t* id = lovrBufferMap(state.identityBuffer, 0, true);
  for (int i = 0; i < MAX_DRAWS; i++) id[i] = i;
  lovrBufferFlush(state.identityBuffer, 0, MAX_DRAWS);
  lovrBufferUnmap(state.identityBuffer);

  Buffer* vertexBuffer = state.buffers[STREAM_VERTEX];
  size_t stride = bufferStride[STREAM_VERTEX];

  MeshAttribute position = { .buffer = vertexBuffer, .offset = 0, .stride = stride, .type = F32, .components = 3 };
  MeshAttribute normal = { .buffer = vertexBuffer, .offset = 12, .stride = stride, .type = F32, .components = 3 };
  MeshAttribute texCoord = { .buffer = vertexBuffer, .offset = 24, .stride = stride, .type = F32, .components = 2 };
  MeshAttribute drawId = { .buffer = state.buffers[STREAM_DRAWID], .type = U8, .components = 1 };
  MeshAttribute identity = { .buffer = state.identityBuffer, .type = U8, .components = 1, .divisor = 1 };

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
  int width, height, framebufferWidth, framebufferHeight;
  lovrPlatformGetWindowSize(&width, &height);
  lovrPlatformGetFramebufferSize(&framebufferWidth, &framebufferHeight);
  if (width == 0 || framebufferWidth == 0) {
    return 0.f;
  } else {
    return (float) framebufferWidth / (float) width;
  }
}

void lovrGraphicsSetBackbuffer(Canvas* canvas, bool stereo, bool clear) {
  lovrGraphicsFlush();

  if (!canvas) {
    canvas = state.defaultCanvas;
    lovrCanvasSetStereo(canvas, stereo);
  }

  if (canvas != state.backbuffer) {
    lovrCanvasResolve(state.backbuffer);
    state.backbuffer = canvas;
  }

  if (clear) {
    lovrGpuClear(state.backbuffer, &state.linearBackgroundColor, &(float) { 1. }, &(int) { 0 });
  }
}

void lovrGraphicsGetViewMatrix(uint32_t index, float* viewMatrix) {
  lovrAssert(index < 2, "Invalid view index %d", index);
  mat4_init(viewMatrix, state.frameData.viewMatrix[index]);
}

void lovrGraphicsSetViewMatrix(uint32_t index, float* viewMatrix) {
  lovrAssert(index < 2, "Invalid view index %d", index);
  lovrGraphicsFlush();
  if (viewMatrix) {
    mat4_init(state.frameData.viewMatrix[index], viewMatrix);
  } else {
    mat4_identity(state.frameData.viewMatrix[index]);
  }
  state.frameDataDirty = true;
}

void lovrGraphicsGetProjection(uint32_t index, float* projection) {
  lovrAssert(index < 2, "Invalid view index %d", index);
  mat4_init(projection, state.frameData.projection[index]);
}

void lovrGraphicsSetProjection(uint32_t index, float* projection) {
  lovrAssert(index < 2, "Invalid view index %d", index);
  lovrGraphicsFlush();
  if (projection) {
    mat4_init(state.frameData.projection[index], projection);
  } else {
    float fov = 67.f * (float) M_PI / 180.f;
    float aspect = (float) state.width / state.height;
    mat4_perspective(state.frameData.projection[index], .01f, 100.f, fov, aspect);
  }
  state.frameDataDirty = true;
}

Buffer* lovrGraphicsGetIdentityBuffer() {
  return state.identityBuffer;
}

// State

void lovrGraphicsReset() {
  state.transform = 0;
  lovrGraphicsSetViewMatrix(0, NULL);
  lovrGraphicsSetViewMatrix(1, NULL);
  lovrGraphicsSetProjection(0, NULL);
  lovrGraphicsSetProjection(1, NULL);
  lovrGraphicsSetAlphaSampling(false);
  lovrGraphicsSetBackgroundColor((Color) { 0, 0, 0, 1 });
  lovrGraphicsSetBlendMode(BLEND_ALPHA, BLEND_ALPHA_MULTIPLY);
  lovrGraphicsSetCanvas(NULL);
  lovrGraphicsSetColor((Color) { 1, 1, 1, 1 });
  lovrGraphicsSetColorMask(true, true, true, true);
  lovrGraphicsSetCullingEnabled(false);
  lovrGraphicsSetDefaultFilter((TextureFilter) { .mode = FILTER_TRILINEAR });
  lovrGraphicsSetDepthTest(COMPARE_LEQUAL, true);
  lovrGraphicsSetFont(NULL);
  lovrGraphicsSetLineWidth(1.f);
  lovrGraphicsSetPointSize(1.f);
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
  state.backgroundColor = state.linearBackgroundColor = color;
#if !defined(LOVR_WEBGL) && !defined(LOVR_USE_PICO)
  gammaCorrect(&state.linearBackgroundColor);
#endif
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
  state.color = state.linearColor = color;
  gammaCorrect(&state.linearColor);
}

void lovrGraphicsGetColorMask(bool* r, bool* g, bool* b, bool* a) {
  *r = state.pipeline.colorMask & 0x8;
  *g = state.pipeline.colorMask & 0x4;
  *b = state.pipeline.colorMask & 0x2;
  *a = state.pipeline.colorMask & 0x1;
}

void lovrGraphicsSetColorMask(bool r, bool g, bool b, bool a) {
  state.pipeline.colorMask = (r << 3) | (g << 2) | (b << 1) | a;
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

void lovrGraphicsSetLineWidth(float width) {
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

// Rendering

static void lovrGraphicsBatch(BatchRequest* req) {

  // Resolve objects
  Mesh* mesh = req->mesh ? req->mesh : (req->instanced ? state.instancedMesh : state.mesh);
  Canvas* canvas = state.canvas ? state.canvas : state.backbuffer;
  bool stereo = lovrCanvasIsStereo(canvas);
  Shader* shader = state.shader ? state.shader : (state.defaultShaders[req->shader][stereo] ? state.defaultShaders[req->shader][stereo] : (state.defaultShaders[req->shader][stereo] = lovrShaderCreateDefault(req->shader, NULL, 0, stereo)));
  Pipeline* pipeline = req->pipeline ? req->pipeline : &state.pipeline;
  Material* material = req->material ? req->material : (state.defaultMaterial ? state.defaultMaterial : (state.defaultMaterial = lovrMaterialCreate()));

  if (!req->material) {
    if (req->type == BATCH_SKYBOX && lovrTextureGetType(req->texture) == TEXTURE_CUBE) {
      lovrShaderSetTextures(shader, "lovrSkyboxTexture", &req->texture, 0, 1);
    } else {
      lovrMaterialSetTexture(material, TEXTURE_DIFFUSE, req->texture);
    }
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
  for (int i = state.batchCount - 1; i >= 0; i--) {
    if (req->type == BATCH_MESH && req->params.mesh.instances > 1) { break; }

    Batch* b = &state.batches[i];
    if (b->type != req->type) { goto next; }
    if (b->drawCount >= MAX_DRAWS) { goto next; }
    if (b->draw.mesh != mesh) { goto next; }
    if (b->draw.canvas != canvas) { goto next; }
    if (b->draw.shader != shader) { goto next; }
    if (b->material != material) { goto next; }
    if (memcmp(&b->draw.pipeline, pipeline, sizeof(Pipeline))) { goto next; }
    if (memcmp(&b->params, &req->params, sizeof(BatchParams))) { goto next; }
    batch = b;
    break;

next:
    // Draws can't be reordered when blending is on, depth test is off, or either of the batches
    // are streaming their vertices (since the vertices of a batch must be contiguous)
    if (b->draw.pipeline.blendMode != BLEND_NONE || pipeline->blendMode != BLEND_NONE) { break; }
    if (b->draw.pipeline.depthTest == COMPARE_NONE || pipeline->depthTest == COMPARE_NONE) { break; }
    if (!req->instanced) { break; }
  }

  // The final draw id isn't known until the batch is fully resolved and all the potential flushes
  // have occurred, so we have to do this weird thing where we map the draw id buffer early on but
  // write the ids much later.
  uint8_t* ids = NULL;

  // Figure out if a flush is necessary before mapping buffers for vertex data or UBOs.
  // - A flush is necessary if vertices are about to be written (during the first element of an
  //   instanced batch or any element of a stream batch) and any of the ranges go past the end.
  // - If a new batch is required but there isn't space for it, flush to make space.
  // - If a new batch is required, make sure there is space for the matrix/color UBO streams.
  // It's important to flush before mapping any streams, because flushing unmaps all streams.
  bool needFlush = false;
  bool hasVertices = req->vertexCount > 0 && (!req->instanced || !batch);
  bool hasIndices = hasVertices && req->indexCount > 0;
  needFlush = needFlush || (hasVertices && state.head[STREAM_VERTEX] + req->vertexCount > bufferCount[STREAM_VERTEX]);
  needFlush = needFlush || (hasVertices && state.head[STREAM_DRAWID] + req->vertexCount > bufferCount[STREAM_DRAWID]);
  needFlush = needFlush || (hasIndices && state.head[STREAM_INDEX] + req->indexCount > bufferCount[STREAM_INDEX]);
  needFlush = needFlush || (!batch && state.batchCount >= MAX_BATCHES);
  needFlush = needFlush || (!batch && state.head[STREAM_MODEL] + MAX_DRAWS > bufferCount[STREAM_MODEL]);
  needFlush = needFlush || (!batch && state.head[STREAM_COLOR] + MAX_DRAWS > bufferCount[STREAM_COLOR]);
  if (needFlush) lovrGraphicsFlush();

  if (req->vertexCount > 0 && (!req->instanced || !batch)) {
    *(req->vertices) = lovrGraphicsMapBuffer(STREAM_VERTEX, req->vertexCount);
    ids = lovrGraphicsMapBuffer(STREAM_DRAWID, req->vertexCount);

    if (req->indexCount > 0) {
      *(req->indices) = lovrGraphicsMapBuffer(STREAM_INDEX, req->indexCount);
      *(req->baseVertex) = state.head[STREAM_VERTEX];
    }
  }

  // Start a new batch
  if (!batch || state.batchCount == 0) {
    float* transforms = lovrGraphicsMapBuffer(STREAM_MODEL, MAX_DRAWS);
    Color* colors = lovrGraphicsMapBuffer(STREAM_COLOR, MAX_DRAWS);

    uint32_t rangeStart, rangeCount, instances;
    if (req->type == BATCH_MESH) {
      rangeStart = req->params.mesh.rangeStart;
      rangeCount = req->params.mesh.rangeCount;
      instances = req->instanced ? 0 : req->params.mesh.instances;
    } else {
      rangeStart = req->indexCount > 0 ? state.head[STREAM_INDEX] : state.head[STREAM_VERTEX];
      rangeCount = 0;
      instances = 0;
    }

    batch = &state.batches[state.batchCount++];
    *batch = (Batch) {
      .type = req->type,
      .params = req->params,
      .draw = {
        .mesh = mesh,
        .canvas = canvas,
        .shader = shader,
        .pipeline = *pipeline,
        .topology = req->topology,
        .rangeStart = rangeStart,
        .rangeCount = rangeCount,
        .instances = instances
      },
      .material = material,
      .transforms = transforms,
      .colors = colors,
      .drawStart = state.head[STREAM_MODEL],
      .indexed = req->indexCount > 0
    };

    state.head[STREAM_MODEL] += MAX_DRAWS;
    state.head[STREAM_COLOR] += MAX_DRAWS;
  }

  // Transform
  if (req->transform) {
    float transform[16];
    mat4_multiply(mat4_init(transform, state.transforms[state.transform]), req->transform);
    memcpy(&batch->transforms[16 * batch->drawCount], transform, 16 * sizeof(float));
  } else {
    memcpy(&batch->transforms[16 * batch->drawCount], state.transforms[state.transform], 16 * sizeof(float));
  }

  // Color
  batch->colors[batch->drawCount] = state.linearColor;

  // Cursors
  if (!req->instanced || batch->drawCount == 0) {
    if (ids) {
      memset(ids, batch->drawCount, req->vertexCount * sizeof(uint8_t));
    }

    batch->draw.rangeCount += batch->indexed ? req->indexCount : req->vertexCount;
    state.head[STREAM_VERTEX] += req->vertexCount;
    state.head[STREAM_DRAWID] += req->vertexCount;
    state.head[STREAM_INDEX] += req->indexCount;
  }

  if (req->instanced) {
    batch->draw.instances++;
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

  if (state.frameDataDirty) {
    state.frameDataDirty = false;
    void* data = lovrGraphicsMapBuffer(STREAM_FRAME, 1);
    memcpy(data, &state.frameData, sizeof(FrameData));
    state.head[STREAM_FRAME]++;
  }

  // Flush buffers
  for (int i = 0; i < MAX_STREAMS; i++) {
    lovrBufferFlush(state.buffers[i], state.tail[i] * bufferStride[i], (state.head[i] - state.tail[i]) * bufferStride[i]);
    lovrBufferUnmap(state.buffers[i]);
    state.tail[i] = state.head[i];
  }

  for (int b = 0; b < batchCount; b++) {
    Batch* batch = &state.batches[b];

    // Uniforms
    lovrMaterialBind(batch->material, batch->draw.shader);
    lovrShaderSetBlock(batch->draw.shader, "lovrModelBlock", state.buffers[STREAM_MODEL], batch->drawStart * bufferStride[STREAM_MODEL], MAX_DRAWS * bufferStride[STREAM_MODEL], ACCESS_READ);
    lovrShaderSetBlock(batch->draw.shader, "lovrColorBlock", state.buffers[STREAM_COLOR], batch->drawStart * bufferStride[STREAM_COLOR], MAX_DRAWS * bufferStride[STREAM_COLOR], ACCESS_READ);
    lovrShaderSetBlock(batch->draw.shader, "lovrFrameBlock", state.buffers[STREAM_FRAME], (state.head[STREAM_FRAME] - 1) * bufferStride[STREAM_FRAME], bufferStride[STREAM_FRAME], ACCESS_READ);
    if (batch->draw.topology == DRAW_POINTS) {
      lovrShaderSetFloats(batch->draw.shader, "lovrPointSize", &state.pointSize, 0, 1);
    }

    // Other bindings (TODO try to get rid of all this!)
    if (batch->type == BATCH_MESH) {
      lovrMeshSetAttributeEnabled(batch->draw.mesh, "lovrDrawID", batch->params.mesh.instances <= 1);
    } else {
      if (batch->draw.mesh == state.instancedMesh && batch->draw.instances <= 1) {
        batch->draw.mesh = state.mesh;
      }

      if (batch->indexed) {
        lovrMeshSetIndexBuffer(batch->draw.mesh, state.buffers[STREAM_INDEX], bufferCount[STREAM_INDEX], sizeof(uint16_t), 0);
      } else {
        lovrMeshSetIndexBuffer(batch->draw.mesh, NULL, 0, 0, 0);
      }
    }

    lovrGpuDraw(&batch->draw);
  }
}

void lovrGraphicsFlushCanvas(Canvas* canvas) {
  for (int i = state.batchCount - 1; i >= 0; i--) {
    if (state.batches[i].draw.canvas == canvas) {
      lovrGraphicsFlush();
      return;
    }
  }
}

void lovrGraphicsFlushShader(Shader* shader) {
  for (int i = state.batchCount - 1; i >= 0; i--) {
    if (state.batches[i].draw.shader == shader) {
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
    if (state.batches[i].draw.mesh == mesh) {
      lovrGraphicsFlush();
      return;
    }
  }
}

void lovrGraphicsClear(Color* color, float* depth, int* stencil) {
#if !defined(LOVR_WEBGL) && !defined(LOVR_USE_PICO)
  if (color) gammaCorrect(color);
#endif
  if (color || depth || stencil) lovrGraphicsFlush();
  lovrGpuClear(state.canvas ? state.canvas : state.backbuffer, color, depth, stencil);
}

void lovrGraphicsDiscard(bool color, bool depth, bool stencil) {
  if (color || depth || stencil) lovrGraphicsFlush();
  lovrGpuDiscard(state.canvas ? state.canvas : state.backbuffer, color, depth, stencil);
}

void lovrGraphicsPoints(uint32_t count, float** vertices) {
  lovrGraphicsBatch(&(BatchRequest) {
    .type = BATCH_POINTS,
    .topology = DRAW_POINTS,
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
    .topology = DRAW_LINE_STRIP,
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
    .topology = style == STYLE_LINE ? DRAW_LINE_LOOP : DRAW_TRIANGLES,
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
    .topology = style == STYLE_LINE ? DRAW_LINE_LOOP : DRAW_TRIANGLES,
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
    .topology = style == STYLE_LINE ? DRAW_LINES : DRAW_TRIANGLES,
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
    .topology = style == STYLE_LINE ? (mode == ARC_MODE_OPEN ? DRAW_LINE_STRIP : DRAW_LINE_LOOP) : DRAW_TRIANGLE_FAN,
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
  float length = vec3_length((float[4]) { transform[8], transform[9], transform[10] });
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
    .topology = DRAW_TRIANGLES,
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
      memcpy(indices, (uint16_t[6]) { j, j + 2, j + 1, j + 1, j + 2, j + 3 }, 6 * sizeof(uint16_t));
      indices += 6;

      if (capped && r1 != 0.f) {
        memcpy(indices, (uint16_t[3]) { top, top + i + 2, top + i + 1 }, 3 * sizeof(uint16_t));
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
    .topology = DRAW_TRIANGLES,
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
        memcpy(indices, ((uint16_t[]) { i0, i0 + 1, i1, i1, i0 + 1, i1 + 1 }), 6 * sizeof(uint16_t));
        indices += 6;
      }
    }
  }
}

void lovrGraphicsSkybox(Texture* texture) {
  TextureType type = lovrTextureGetType(texture);
  lovrAssert(type == TEXTURE_CUBE || type == TEXTURE_2D, "Only 2D and cube textures can be used as skyboxes");

  Pipeline pipeline = state.pipeline;
  pipeline.winding = WINDING_COUNTERCLOCKWISE;

  float* vertices = NULL;

  lovrGraphicsBatch(&(BatchRequest) {
    .type = BATCH_SKYBOX,
    .topology = DRAW_TRIANGLE_STRIP,
    .shader = type == TEXTURE_CUBE ? SHADER_CUBE : SHADER_PANO,
    .pipeline = &pipeline,
    .texture = texture,
    .vertexCount = 4,
    .vertices = &vertices,
    .instanced = true
  });

  if (vertices) {
    static float vertexData[] = {
      -1.f,  1.f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f,
      -1.f, -1.f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f,
       1.f,  1.f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f,
       1.f, -1.f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f
    };

    memcpy(vertices, vertexData, sizeof(vertexData));
  }
}

void lovrGraphicsPrint(const char* str, size_t length, mat4 transform, float wrap, HorizontalAlign halign, VerticalAlign valign) {
  float width;
  float height;
  uint32_t lineCount;
  uint32_t glyphCount;
  Font* font = lovrGraphicsGetFont();
  lovrFontMeasure(font, str, length, wrap, &width, &height, &lineCount, &glyphCount);

  if (glyphCount == 0) {
    return;
  }

  float scale = 1.f / lovrFontGetPixelDensity(font);
  mat4_scale(transform, scale, scale, scale);
  mat4_translate(transform, 0.f, height * (valign / 2.f), 0.f);

  Pipeline pipeline = state.pipeline;
  pipeline.blendMode = pipeline.blendMode == BLEND_NONE ? BLEND_ALPHA : pipeline.blendMode;

  float* vertices;
  uint16_t* indices;
  uint16_t baseVertex;
  lovrGraphicsBatch(&(BatchRequest) {
    .type = BATCH_TEXT,
    .topology = DRAW_TRIANGLES,
    .shader = SHADER_FONT,
    .pipeline = &pipeline,
    .transform = transform,
    .texture = lovrFontGetTexture(font),
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
    .topology = DRAW_TRIANGLE_STRIP,
    .shader = SHADER_FILL,
    .texture = texture,
    .pipeline = &pipeline,
    .vertexCount = 4,
    .vertices = &vertices
  });

  if (vertices) {
    memcpy(vertices, (float[32]) {
      -1.f,  1.f, 0.f, 0.f, 0.f, 0.f, u, v + h,
      -1.f, -1.f, 0.f, 0.f, 0.f, 0.f, u, v,
       1.f,  1.f, 0.f, 0.f, 0.f, 0.f, u + w, v + h,
       1.f, -1.f, 0.f, 0.f, 0.f, 0.f, u + w, v
    }, 32 * sizeof(float));
  }
}

void lovrGraphicsDrawMesh(Mesh* mesh, mat4 transform, uint32_t instances, float* pose) {
  uint32_t vertexCount = lovrMeshGetVertexCount(mesh);
  uint32_t indexCount = lovrMeshGetIndexCount(mesh);
  uint32_t defaultCount = indexCount > 0 ? indexCount : vertexCount;
  uint32_t rangeStart, rangeCount;
  lovrMeshGetDrawRange(mesh, &rangeStart, &rangeCount);
  rangeCount = rangeCount > 0 ? rangeCount : defaultCount;
  DrawMode mode = lovrMeshGetDrawMode(mesh);
  Material* material = lovrMeshGetMaterial(mesh);

  lovrGraphicsBatch(&(BatchRequest) {
    .type = BATCH_MESH,
    .params.mesh.rangeStart = rangeStart,
    .params.mesh.rangeCount = rangeCount,
    .params.mesh.instances = instances,
    .params.mesh.pose = pose,
    .mesh = mesh,
    .topology = mode,
    .transform = transform,
    .material = material,
    .instanced = instances <= 1
  });
}
