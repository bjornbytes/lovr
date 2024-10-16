#include "graphics/graphics.h"
#include "data/blob.h"
#include "data/image.h"
#include "data/modelData.h"
#include "data/rasterizer.h"
#include "event/event.h"
#include "headset/headset.h"
#include "math/math.h"
#include "core/gpu.h"
#include "core/maf.h"
#include "core/spv.h"
#include "core/os.h"
#include "util.h"
#include "monkey.h"
#include "shaders.h"
#include <math.h>
#include <stdatomic.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef LOVR_USE_GLSLANG
#include "glslang_c_interface.h"
#include "resource_limits_c.h"
#endif

#define MAX_PIPELINES 65536
#define MAX_TALLIES 255
#define TRANSFORM_STACK_SIZE 16
#define PIPELINE_STACK_SIZE 4
#define MAX_SHADER_RESOURCES 32
#define MAX_CUSTOM_ATTRIBUTES 10
#define FLOAT_BITS(f) ((union { float f; uint32_t u; }) { f }).u

typedef struct {
  void* next;
  void* pointer;
  gpu_buffer* handle;
  uint32_t tick;
  uint32_t size;
  uint32_t ref;
} BufferBlock;

typedef struct {
  BufferBlock* freelist;
  BufferBlock* current;
  uint32_t cursor;
} BufferAllocator;

typedef struct {
  BufferBlock* block;
  gpu_buffer* buffer;
  uint32_t offset;
  uint32_t extent;
  void* pointer;
} BufferView;

typedef struct {
  gpu_phase readPhase;
  gpu_phase writePhase;
  gpu_cache pendingReads;
  gpu_cache pendingWrite;
  uint32_t lastTransferRead;
  uint32_t lastTransferWrite;
  gpu_barrier* barrier;
} Sync;

struct Buffer {
  uint32_t ref;
  uint32_t base;
  Sync sync;
  gpu_buffer* gpu;
  BufferBlock* block;
  bool supportsMesh;
  BufferInfo info;
};

struct Texture {
  uint32_t ref;
  bool xrAcquired;
  Sync sync;
  gpu_texture* gpu;
  gpu_texture* sampleView;
  gpu_texture* renderView;
  gpu_texture* storageView;
  Material* material;
  Sampler* sampler;
  Texture* root;
  uint32_t baseLayer;
  uint32_t baseLevel;
  TextureInfo info;
};

struct Sampler {
  uint32_t ref;
  gpu_sampler* gpu;
  SamplerInfo info;
};

enum {
  FLAG_VERTEX = (1 << 0),
  FLAG_FRAGMENT = (1 << 1),
  FLAG_COMPUTE = (1 << 2)
};

typedef struct {
  uint32_t hash;
  uint32_t binding;
  gpu_slot_type type;
  gpu_phase phase;
  gpu_cache cache;
  uint32_t fieldCount;
  DataField* format;
} ShaderResource;

typedef struct {
  uint32_t location;
  uint32_t hash;
} ShaderAttribute;

typedef struct {
  void* next;
  gpu_bundle_pool* gpu;
  gpu_bundle* bundles;
  uint32_t cursor;
  uint32_t tick;
} BundlePool;

typedef struct Layout {
  struct Layout* next;
  uint64_t hash;
  gpu_layout* gpu;
  BundlePool* head;
  BundlePool* tail;
} Layout;

struct Shader {
  uint32_t ref;
  Shader* parent;
  gpu_shader* gpu;
  gpu_pipeline* computePipeline;
  ShaderInfo info;
  Layout* layout;
  uint32_t workgroupSize[3];
  bool hasCustomAttributes;
  uint32_t attributeCount;
  uint32_t resourceCount;
  uint32_t bufferMask;
  uint32_t textureMask;
  uint32_t samplerMask;
  uint32_t storageMask;
  uint32_t pushConstantSize;
  uint32_t uniformSize;
  uint32_t uniformCount;
  uint32_t stageMask;
  ShaderAttribute* attributes;
  ShaderResource* resources;
  DataField* uniforms;
  DataField* fields;
  uint32_t flagCount;
  uint32_t overrideCount;
  gpu_shader_flag* flags;
  uint32_t* flagLookup;
  char* names;
};

struct Material {
  uint32_t ref;
  uint32_t next;
  uint32_t tick;
  uint16_t index;
  uint16_t block;
  gpu_bundle* bundle;
  MaterialInfo info;
  bool hasWritableTexture;
};

typedef struct {
  float advance;
  uint16_t x, y;
  uint16_t uv[4];
  float box[4];
} Glyph;

struct Font {
  uint32_t ref;
  FontInfo info;
  Material* material;
  arr_t(Glyph) glyphs;
  map_t glyphLookup;
  float pixelDensity;
  float lineSpacing;
  uint32_t padding;
  Texture* atlas;
  uint32_t atlasWidth;
  uint32_t atlasHeight;
  uint32_t rowHeight;
  uint32_t atlasX;
  uint32_t atlasY;
};

struct Mesh {
  uint32_t ref;
  MeshStorage storage;
  Buffer* vertexBuffer;
  Buffer* indexBuffer;
  uint32_t indexCount;
  uint32_t dirtyVertices[2];
  bool dirtyIndices;
  void* vertices;
  void* indices;
  float bounds[6];
  bool hasBounds;
  DrawMode mode;
  uint32_t drawStart;
  uint32_t drawCount;
  uint32_t baseVertex;
  Material* material;
};

typedef struct {
  float transform[12];
  float color[4];
} DrawData;

typedef enum {
  VERTEX_SHAPE,
  VERTEX_POINT,
  VERTEX_GLYPH,
  VERTEX_MODEL,
  VERTEX_EMPTY,
  VERTEX_FORMAT_COUNT
} VertexFormat;

typedef struct {
  uint64_t hash;
  DrawMode mode;
  DefaultShader shader;
  Material* material;
  float* transform;
  float* bounds;
  struct {
    Buffer* buffer;
    VertexFormat format;
    uint32_t count;
    void** pointer;
  } vertex;
  struct {
    Buffer* buffer;
    uint32_t count;
    void** pointer;
  } index;
  uint32_t start;
  uint32_t count;
  uint32_t instances;
  uint32_t baseVertex;
} DrawInfo;

typedef struct {
  float position[3];
  float rotation[4];
  float scale[3];
} NodeTransform;

typedef struct {
  uint32_t index;
  uint32_t count;
  uint32_t vertexIndex;
  uint32_t vertexCount;
} BlendGroup;

struct Model {
  uint32_t ref;
  Model* parent;
  ModelInfo info;
  DrawInfo* draws;
  Buffer* rawVertexBuffer;
  Buffer* vertexBuffer;
  Buffer* indexBuffer;
  Buffer* blendBuffer;
  Buffer* skinBuffer;
  Mesh** meshes;
  Texture** textures;
  Material** materials;
  NodeTransform* localTransforms;
  float* globalTransforms;
  float* boundingBoxes;
  bool transformsDirty;
  bool blendShapesDirty;
  float* blendShapeWeights;
  BlendGroup* blendGroups;
  uint32_t blendGroupCount;
  uint32_t lastVertexAnimation;
};

typedef enum {
  READBACK_BUFFER,
  READBACK_TEXTURE,
  READBACK_TIMESTAMP
} ReadbackType;

typedef struct {
  Pass* pass;
  double cpuTime;
} TimingInfo;

struct Readback {
  uint32_t ref;
  uint32_t tick;
  Readback* next;
  BufferView view;
  ReadbackType type;
  union {
    struct {
      Buffer* buffer;
      Blob* blob;
    };
    struct {
      Texture* texture;
      Image* image;
    };
    struct {
      TimingInfo* times;
      uint32_t count;
    };
  };
};

typedef struct {
  float resolution[2];
  float time;
} Globals;

typedef struct {
  float viewMatrix[16];
  float projection[16];
  float viewProjection[16];
  float inverseProjection[16];
} Camera;

typedef struct {
  struct { float x, y, z; } position;
  struct { float x, y, z; } normal;
  struct { float u, v; } uv;
} ShapeVertex;

typedef struct {
  struct { float x, y, z; } position;
  uint32_t normal;
  struct { float u, v; } uv;
  struct { uint8_t r, g, b, a; } color;
  uint32_t tangent;
} ModelVertex;

typedef struct {
  struct { float x, y, z; } position;
  struct { float x, y, z; } normal;
  struct { float x, y, z; } tangent;
} BlendVertex;

enum {
  SHAPE_PLANE,
  SHAPE_BOX,
  SHAPE_CIRCLE,
  SHAPE_SPHERE,
  SHAPE_CYLINDER,
  SHAPE_CONE,
  SHAPE_CAPSULE,
  SHAPE_TORUS,
  SHAPE_MONKEY
};

enum {
  DIRTY_BINDINGS = (1 << 0),
  DIRTY_UNIFORMS = (1 << 1),
  DIRTY_CAMERA = (1 << 2),
  DIRTY_VIEWPORT = (1 << 3),
  DIRTY_SCISSOR = (1 << 4),
  NEEDS_VIEW_CULL = (1 << 5)
};

typedef struct {
  char* memory;
  size_t cursor;
  size_t length;
  size_t limit;
} Allocator;

typedef struct {
  uint64_t hash;
  uint32_t start;
  uint32_t baseVertex;
  uint32_t vertexBufferOffset;
  gpu_buffer* vertexBuffer;
  gpu_buffer* indexBuffer;
} CachedShape;

enum {
  ACCESS_COMPUTE,
  ACCESS_RENDER
};

typedef struct {
  Sync* sync;
  void* object;
  gpu_phase phase;
  gpu_cache cache;
} Access;

typedef struct {
  void* prev;
  void* next;
  uint64_t count;
  uint64_t textureMask;
  uint64_t padding;
  Access list[41];
} AccessBlock;

typedef struct {
  Texture* texture;
  LoadAction load;
  float clear[4];
} ColorAttachment;

typedef struct {
  Texture* texture;
  TextureFormat format;
  LoadAction load;
  float clear;
} DepthAttachment;

typedef struct {
  ColorAttachment color[4];
  DepthAttachment depth;
  uint32_t count;
  uint32_t width;
  uint32_t height;
  uint32_t views;
  uint32_t samples;
  bool resolve;
} Canvas;

typedef struct {
  bool dirty;
  bool viewCull;
  DrawMode mode;
  float color[4];
  Buffer* lastVertexBuffer;
  VertexFormat lastVertexFormat;
  gpu_pipeline_info info;
  Material* material;
  Shader* shader;
  Font* font;
} Pipeline;

enum {
  COMPUTE_INDIRECT = (1 << 0),
  COMPUTE_BARRIER = (1 << 1)
};

typedef struct {
  uint32_t flags;
  Shader* shader;
  gpu_bundle_info* bundleInfo;
  gpu_bundle* bundle;
  gpu_buffer* uniformBuffer;
  uint32_t uniformOffset;
  union {
    struct {
      uint32_t x;
      uint32_t y;
      uint32_t z;
    };
    struct {
      gpu_buffer* buffer;
      uint32_t offset;
    } indirect;
  };
} Compute;

enum {
  DRAW_INDIRECT = (1 << 0),
  DRAW_INDEX32 = (1 << 1),
  DRAW_HAS_BOUNDS = (1 << 2)
};

typedef struct {
  uint8_t flags;
  uint8_t tally;
  uint16_t camera;
  uint16_t viewport;
  uint16_t scissor;
  Shader* shader;
  Material* material;
  gpu_pipeline_info* pipelineInfo;
  gpu_bundle_info* bundleInfo;
  gpu_pipeline* pipeline;
  gpu_bundle* bundle;
  gpu_buffer* vertexBuffer;
  gpu_buffer* indexBuffer;
  gpu_buffer* uniformBuffer;
  uint32_t vertexBufferOffset;
  uint32_t uniformOffset;
  union {
    struct {
      uint32_t start;
      uint32_t count;
      uint32_t instances;
      uint32_t baseVertex;
    };
    struct {
      gpu_buffer* buffer;
      uint32_t offset;
      uint32_t count;
      uint32_t stride; // Deprecated
    } indirect;
  };
  float transform[16];
  float color[4];
  float bounds[6];
} Draw;

typedef struct {
  gpu_tally* gpu;
  Buffer* tempBuffer;
  bool active;
  uint32_t count;
  uint32_t bufferOffset;
  Buffer* buffer;
} Tally;

struct Pass {
  uint32_t ref;
  uint32_t flags;
  gpu_pass* gpu;
  Allocator allocator;
  BufferAllocator buffers;
  CachedShape geocache[16];
  AccessBlock* access[2];
  Tally tally;
  Canvas canvas;
  Camera* cameras;
  float* viewports;
  uint32_t* scissors;
  uint32_t cameraCount;
  uint32_t viewportCount;
  uint32_t scissorCount;
  Sampler* sampler;
  float* transform;
  Pipeline* pipeline;
  uint32_t transformIndex;
  uint32_t pipelineIndex;
  gpu_binding* bindings;
  void* uniforms;
  uint32_t computeCount;
  Compute* computes;
  uint32_t drawCount;
  uint32_t drawCapacity;
  Draw* draws;
  PassStats stats;
  char* label;
};

typedef struct {
  Material* list;
  BufferView view;
  gpu_bundle_pool* bundlePool;
  gpu_bundle* bundles;
  uint32_t head;
  uint32_t tail;
} MaterialBlock;

typedef struct {
  gpu_texture* texture;
  uint32_t hash;
  uint32_t tick;
} ScratchTexture;

static struct {
  uint32_t ref;
  bool glslang;
  bool active;
  bool resized;
  bool shouldPresent;
  bool timingEnabled;
  GraphicsConfig config;
  gpu_device_info device;
  gpu_features features;
  gpu_limits limits;
  gpu_stream* stream;
  gpu_barrier barrier;
  gpu_barrier transferBarrier;
  gpu_tally* timestamps;
  uint32_t timestampCount;
  uint32_t tick;
  float background[4];
  TextureFormat depthFormat;
  Texture* window;
  Pass* windowPass;
  Font* defaultFont;
  Buffer* defaultBuffer;
  Texture* defaultTexture;
  Sampler* defaultSamplers[2];
  Shader* defaultShaders[DEFAULT_SHADER_COUNT];
  gpu_vertex_format vertexFormats[VERTEX_FORMAT_COUNT];
  Readback* oldestReadback;
  Readback* newestReadback;
  Material* defaultMaterial;
  size_t materialBlock;
  arr_t(MaterialBlock) materialBlocks;
  BufferAllocator bufferAllocators[4];
  arr_t(ScratchTexture) scratchTextures;
  map_t passLookup;
  map_t pipelineLookup;
  gpu_pipeline* pipelines;
  uint32_t pipelineCount;
  Layout* layouts;
  Layout* builtinLayout;
  Layout* materialLayout;
  Layout* uniformLayout;
  Allocator allocator;
} state;

// Helpers

static void* tempAlloc(Allocator* allocator, size_t size);
static size_t tempPush(Allocator* allocator);
static void tempPop(Allocator* allocator, size_t stack);
static gpu_pipeline* getPipeline(uint32_t index);
static BufferBlock* getBlock(gpu_buffer_type type, uint32_t size);
static void freeBlock(BufferAllocator* allocator, BufferBlock* block);
static BufferView allocateBuffer(BufferAllocator* allocator, gpu_buffer_type type, uint32_t size, size_t align);
static BufferView getBuffer(gpu_buffer_type type, uint32_t size, size_t align);
static int u64cmp(const void* a, const void* b);
static uint32_t lcm(uint32_t a, uint32_t b);
static bool beginFrame(void);
static void flushTransfers(void);
static void processReadbacks(void);
static gpu_pass* getPass(Canvas* canvas);
static Layout* getLayout(gpu_slot* slots, uint32_t count);
static gpu_bundle* getBundle(Layout* layout, gpu_binding* bindings, uint32_t count);
static gpu_texture* getScratchTexture(gpu_stream* stream, Canvas* canvas, TextureFormat format, bool srgb);
static bool isDepthFormat(TextureFormat format);
static bool supportsSRGB(TextureFormat format);
static uint32_t measureTexture(TextureFormat format, uint32_t w, uint32_t h, uint32_t d);
static bool checkTextureBounds(const TextureInfo* info, uint32_t offset[4], uint32_t extent[3]);
static void mipmapTexture(gpu_stream* stream, Texture* texture, uint32_t base, uint32_t count);
static ShaderResource* findShaderResource(Shader* shader, const char* name, size_t length);
static Access* getNextAccess(Pass* pass, int type, bool texture);
static void trackBuffer(Pass* pass, Buffer* buffer, gpu_phase phase, gpu_cache cache);
static void trackTexture(Pass* pass, Texture* texture, gpu_phase phase, gpu_cache cache);
static void trackMaterial(Pass* pass, Material* material);
static bool syncResource(Access* access, gpu_barrier* barrier);
static gpu_barrier syncTransfer(Sync* sync, gpu_phase phase, gpu_cache cache);
static void updateModelTransforms(Model* model, uint32_t nodeIndex, float* parent);
static bool checkShaderFeatures(uint32_t* features, uint32_t count);
static void onResize(uint32_t width, uint32_t height);
static void onMessage(void* context, const char* message);

// Entry

bool lovrGraphicsInit(GraphicsConfig* config) {
  if (atomic_fetch_add(&state.ref, 1)) return false;

  gpu_config gpu = {
    .debug = config->debug,
    .fnLog = onMessage,
    .fnAlloc = lovrMalloc,
    .fnFree = lovrFree,
    .engineName = "LOVR",
    .engineVersion = { LOVR_VERSION_MAJOR, LOVR_VERSION_MINOR, LOVR_VERSION_PATCH },
    .device = &state.device,
    .features = &state.features,
    .limits = &state.limits,
#ifdef LOVR_VK
    .vk.cacheData = config->cacheData,
    .vk.cacheSize = config->cacheSize,
#endif
#if defined(LOVR_VK) && !defined(LOVR_DISABLE_HEADSET)
    .vk.getPhysicalDevice = lovrHeadsetInterface ? lovrHeadsetInterface->getVulkanPhysicalDevice : NULL,
    .vk.createInstance = lovrHeadsetInterface ? lovrHeadsetInterface->createVulkanInstance : NULL,
    .vk.createDevice = lovrHeadsetInterface ? lovrHeadsetInterface->createVulkanDevice : NULL,
#endif
  };

  if (!gpu_init(&gpu)) {
#if _WIN32
    const char* format = "This program requires a graphics card with support for Vulkan 1.1, but no device was found or it failed to initialize properly.  The error message was:\n\n%s";
    size_t size = snprintf(NULL, 0, format, gpu_get_error()) + 1;
    char* string = lovrMalloc(size);
    snprintf(string, size, format, gpu_get_error());
    os_window_message_box(string);
    lovrFree(string);
#endif
    return lovrSetError("Failed to initialize GPU: %s", gpu_get_error());
  }

  state.config = *config;
  state.timingEnabled = config->debug;

  // Temporary frame memory uses a large 1GiB virtual memory allocation, committing pages as needed
  state.allocator.length = 1 << 14;
  state.allocator.limit = 1 << 30;
  state.allocator.memory = os_vm_init(state.allocator.limit);
  lovrAssertGoto(fail, state.allocator.memory && os_vm_commit(state.allocator.memory, state.allocator.length), "Failed to initialize temp allocator");

  state.pipelines = os_vm_init(MAX_PIPELINES * gpu_sizeof_pipeline());
  lovrAssertGoto(fail, state.pipelines, "Failed to allocate memory for pipelines");

  map_init(&state.passLookup, 4);
  map_init(&state.pipelineLookup, 64);
  arr_init(&state.materialBlocks);
  arr_init(&state.scratchTextures);

  gpu_slot builtinSlots[] = {
    { 0, GPU_SLOT_UNIFORM_BUFFER, GPU_STAGE_GRAPHICS }, // Globals
    { 1, GPU_SLOT_UNIFORM_BUFFER_DYNAMIC, GPU_STAGE_GRAPHICS }, // Cameras
    { 2, GPU_SLOT_UNIFORM_BUFFER_DYNAMIC, GPU_STAGE_GRAPHICS }, // DrawData
    { 3, GPU_SLOT_SAMPLER, GPU_STAGE_GRAPHICS } // Sampler
  };

  gpu_slot materialSlots[] = {
    { 0, GPU_SLOT_UNIFORM_BUFFER, GPU_STAGE_GRAPHICS }, // Data
    { 1, GPU_SLOT_SAMPLED_TEXTURE, GPU_STAGE_GRAPHICS }, // Color
    { 2, GPU_SLOT_SAMPLED_TEXTURE, GPU_STAGE_GRAPHICS }, // Glow
    { 3, GPU_SLOT_SAMPLED_TEXTURE, GPU_STAGE_GRAPHICS }, // Occlusion
    { 4, GPU_SLOT_SAMPLED_TEXTURE, GPU_STAGE_GRAPHICS }, // Metalness
    { 5, GPU_SLOT_SAMPLED_TEXTURE, GPU_STAGE_GRAPHICS }, // Roughness
    { 6, GPU_SLOT_SAMPLED_TEXTURE, GPU_STAGE_GRAPHICS }, // Clearcoat
    { 7, GPU_SLOT_SAMPLED_TEXTURE, GPU_STAGE_GRAPHICS } // Normal
  };

  gpu_slot uniformSlots[] = {
    { 0, GPU_SLOT_UNIFORM_BUFFER_DYNAMIC, GPU_STAGE_GRAPHICS | GPU_STAGE_COMPUTE }
  };

  state.builtinLayout = getLayout(builtinSlots, COUNTOF(builtinSlots));
  state.materialLayout = getLayout(materialSlots, COUNTOF(materialSlots));
  state.uniformLayout = getLayout(uniformSlots, COUNTOF(uniformSlots));
  lovrAssertGoto(fail, state.builtinLayout && state.materialLayout && state.uniformLayout, "Failed to create GPU layouts: %s", gpu_get_error());

  // Default Buffer

  float defaultBufferData[] = { 0.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f };
  state.defaultBuffer = lovrBufferCreate(&(BufferInfo) { .size = sizeof(defaultBufferData), }, NULL);
  if (!state.defaultBuffer) goto fail;
  if (!beginFrame()) goto fail;

  BufferView view = getBuffer(GPU_BUFFER_UPLOAD, sizeof(defaultBufferData), 4);
  if (!view.buffer) goto fail;

  memcpy(view.pointer, defaultBufferData, sizeof(defaultBufferData));
  gpu_copy_buffers(state.stream, view.buffer, state.defaultBuffer->gpu, view.offset, state.defaultBuffer->base, sizeof(defaultBufferData));

  state.barrier.prev |= GPU_PHASE_COPY;
  state.barrier.next |= GPU_PHASE_INPUT_VERTEX | GPU_PHASE_SHADER_VERTEX | GPU_PHASE_SHADER_VERTEX | GPU_PHASE_SHADER_FRAGMENT | GPU_PHASE_SHADER_COMPUTE;
  state.barrier.flush |= GPU_CACHE_TRANSFER_WRITE;
  state.barrier.clear |= GPU_CACHE_VERTEX | GPU_CACHE_UNIFORM | GPU_CACHE_STORAGE_READ;

  // Default Texture

  Image* image = lovrImageCreateRaw(4, 4, FORMAT_RGBA8, false);
  if (!image) goto fail;

  float white[4] = { 1.f, 1.f, 1.f, 1.f };
  for (uint32_t y = 0; y < 4; y++) {
    for (uint32_t x = 0; x < 4; x++) {
      lovrImageSetPixel(image, x, y, white);
    }
  }

  state.defaultTexture = lovrTextureCreate(&(TextureInfo) {
    .type = TEXTURE_2D,
    .usage = TEXTURE_SAMPLE,
    .format = FORMAT_RGBA8,
    .width = 4,
    .height = 4,
    .layers = 1,
    .mipmaps = 1,
    .srgb = false,
    .imageCount = 1,
    .images = &image,
    .label = "Default Texture"
  });

  lovrRelease(image, lovrImageDestroy);
  if (!state.defaultTexture) goto fail;

  // Default Samplers

  for (uint32_t i = 0; i < 2; i++) {
    state.defaultSamplers[i] = lovrSamplerCreate(&(SamplerInfo) {
      .min = i == 0 ? FILTER_NEAREST : FILTER_LINEAR,
      .mag = i == 0 ? FILTER_NEAREST : FILTER_LINEAR,
      .mip = i == 0 ? FILTER_NEAREST : FILTER_LINEAR,
      .wrap = { WRAP_REPEAT, WRAP_REPEAT, WRAP_REPEAT },
      .range = { 0.f, -1.f }
    });

    if (!state.defaultSamplers[i]) {
      goto fail;
    }
  }

  // Vertex Formats

  state.vertexFormats[VERTEX_SHAPE] = (gpu_vertex_format) {
    .bufferCount = 2,
    .attributeCount = 5,
    .bufferStrides[0] = sizeof(ShapeVertex),
    .attributes[0] = { 0, 10, offsetof(ShapeVertex, position), GPU_TYPE_F32x3 },
    .attributes[1] = { 0, 11, offsetof(ShapeVertex, normal), GPU_TYPE_F32x3 },
    .attributes[2] = { 0, 12, offsetof(ShapeVertex, uv), GPU_TYPE_F32x2 },
    .attributes[3] = { 1, 13, 16, GPU_TYPE_F32x4 },
    .attributes[4] = { 1, 14, 0, GPU_TYPE_F32x4 }
  };

  state.vertexFormats[VERTEX_POINT] = (gpu_vertex_format) {
    .bufferCount = 2,
    .attributeCount = 5,
    .bufferStrides[0] = 12,
    .attributes[0] = { 0, 10, 0, GPU_TYPE_F32x3 },
    .attributes[1] = { 1, 11, 0, GPU_TYPE_F32x4 },
    .attributes[2] = { 1, 12, 0, GPU_TYPE_F32x4 },
    .attributes[3] = { 1, 13, 16, GPU_TYPE_F32x4 },
    .attributes[4] = { 1, 14, 0, GPU_TYPE_F32x4 }
  };

  state.vertexFormats[VERTEX_GLYPH] = (gpu_vertex_format) {
    .bufferCount = 2,
    .attributeCount = 5,
    .bufferStrides[0] = sizeof(GlyphVertex),
    .attributes[0] = { 0, 10, offsetof(GlyphVertex, position), GPU_TYPE_F32x2 },
    .attributes[1] = { 1, 11, 0, GPU_TYPE_F32x4 },
    .attributes[2] = { 0, 12, offsetof(GlyphVertex, uv), GPU_TYPE_UN16x2 },
    .attributes[3] = { 0, 13, offsetof(GlyphVertex, color), GPU_TYPE_UN8x4 },
    .attributes[4] = { 1, 14, 0, GPU_TYPE_F32x4 }
  };

  state.vertexFormats[VERTEX_MODEL] = (gpu_vertex_format) {
    .bufferCount = 2,
    .attributeCount = 5,
    .bufferStrides[0] = sizeof(ModelVertex),
    .attributes[0] = { 0, 10, offsetof(ModelVertex, position), GPU_TYPE_F32x3 },
    .attributes[1] = { 0, 11, offsetof(ModelVertex, normal), GPU_TYPE_SN10x3 },
    .attributes[2] = { 0, 12, offsetof(ModelVertex, uv), GPU_TYPE_F32x2 },
    .attributes[3] = { 0, 13, offsetof(ModelVertex, color), GPU_TYPE_UN8x4 },
    .attributes[4] = { 0, 14, offsetof(ModelVertex, tangent), GPU_TYPE_SN10x3 }
  };

  state.vertexFormats[VERTEX_EMPTY] = (gpu_vertex_format) {
    .bufferCount = 2,
    .attributeCount = 5,
    .attributes[0] = { 1, 10, 0, GPU_TYPE_F32x3 },
    .attributes[1] = { 1, 11, 0, GPU_TYPE_F32x3 },
    .attributes[2] = { 1, 12, 0, GPU_TYPE_F32x2 },
    .attributes[3] = { 1, 13, 16, GPU_TYPE_F32x4 },
    .attributes[4] = { 1, 14, 0, GPU_TYPE_F32x4 }
  };

  state.defaultMaterial = lovrMaterialCreate(&(MaterialInfo) {
    .data.color = { 1.f, 1.f, 1.f, 1.f },
    .data.uvScale = { 1.f, 1.f },
    .data.metalness = 0.f,
    .data.roughness = 1.f,
    .data.normalScale = 1.f,
    .texture = state.defaultTexture
  });

  if (!state.defaultMaterial) goto fail;

  float16Init();
#ifdef LOVR_USE_GLSLANG
  glslang_initialize_process();
  state.glslang = true;
#endif
  return true;
fail:
  lovrGraphicsDestroy();
  return false;
}

void lovrGraphicsDestroy(void) {
  if (atomic_fetch_sub(&state.ref, 1) != 1) return;
#ifndef LOVR_DISABLE_HEADSET
  // If there's an active headset session it needs to be stopped so it can clean up its Pass and
  // swapchain textures before gpu_destroy is called.  This is really hacky and should be solved
  // with module-level refcounting in the future.
  if (lovrHeadsetInterface && lovrHeadsetInterface->stop) {
    lovrHeadsetInterface->stop();
  }
#endif
  Readback* readback = state.oldestReadback;
  while (readback) {
    Readback* next = readback->next;
    lovrReadbackDestroy(readback);
    readback = next;
  }
  if (state.timestamps) gpu_tally_destroy(state.timestamps);
  lovrFree(state.timestamps);
  lovrRelease(state.window, lovrTextureDestroy);
  lovrRelease(state.windowPass, lovrPassDestroy);
  lovrRelease(state.defaultFont, lovrFontDestroy);
  lovrRelease(state.defaultBuffer, lovrBufferDestroy);
  lovrRelease(state.defaultTexture, lovrTextureDestroy);
  lovrRelease(state.defaultSamplers[0], lovrSamplerDestroy);
  lovrRelease(state.defaultSamplers[1], lovrSamplerDestroy);
  for (size_t i = 0; i < COUNTOF(state.defaultShaders); i++) {
    lovrRelease(state.defaultShaders[i], lovrShaderDestroy);
  }
  lovrRelease(state.defaultMaterial, lovrMaterialDestroy);
  for (size_t i = 0; i < state.materialBlocks.length; i++) {
    MaterialBlock* block = &state.materialBlocks.data[i];
    BufferBlock* current = state.bufferAllocators[GPU_BUFFER_STATIC].current;
    if (block->view.block != current && atomic_fetch_sub(&block->view.block->ref, 1) == 1) {
      freeBlock(&state.bufferAllocators[GPU_BUFFER_STATIC], block->view.block);
    }
    gpu_bundle_pool_destroy(block->bundlePool);
    lovrFree(block->list);
    lovrFree(block->bundlePool);
    lovrFree(block->bundles);
  }
  arr_free(&state.materialBlocks);
  for (size_t i = 0; i < state.scratchTextures.length; i++) {
    gpu_texture_destroy(state.scratchTextures.data[i].texture);
    lovrFree(state.scratchTextures.data[i].texture);
  }
  arr_free(&state.scratchTextures);
  for (uint32_t i = 0; i < state.pipelineCount; i++) {
    gpu_pipeline_destroy(getPipeline(i));
  }
  os_vm_free(state.pipelines, MAX_PIPELINES * gpu_sizeof_pipeline());
  map_free(&state.pipelineLookup);
  for (size_t i = 0; i < state.passLookup.size; i++) {
    if (state.passLookup.values[i] != MAP_NIL) {
      gpu_pass* pass = (gpu_pass*) (uintptr_t) state.passLookup.values[i];
      gpu_pass_destroy(pass);
      lovrFree(pass);
    }
  }
  map_free(&state.passLookup);
  for (size_t i = 0; i < COUNTOF(state.bufferAllocators); i++) {
    BufferBlock* block = state.bufferAllocators[i].freelist;
    while (block) {
      gpu_buffer_destroy(block->handle);
      BufferBlock* next = block->next;
      lovrFree(block);
      block = next;
    }

    BufferBlock* current = state.bufferAllocators[i].current;

    if (current) {
      gpu_buffer_destroy(current->handle);
      lovrFree(current);
    }
  }
  Layout* layout = state.layouts;
  while (layout) {
    BundlePool* pool = layout->head;
    while (pool) {
      BundlePool* next = pool->next;
      gpu_bundle_pool_destroy(pool->gpu);
      lovrFree(pool->gpu);
      lovrFree(pool->bundles);
      lovrFree(pool);
      pool = next;
    }
    gpu_layout_destroy(layout->gpu);
    Layout* next = layout->next;
    lovrFree(layout);
    layout = next;
  }
  gpu_destroy();
#ifdef LOVR_USE_GLSLANG
  if (state.glslang) glslang_finalize_process();
#endif
  os_vm_free(state.allocator.memory, state.allocator.limit);
  memset(&state, 0, sizeof(state));
}

bool lovrGraphicsIsInitialized(void) {
  return state.ref;
}

void lovrGraphicsGetDevice(GraphicsDevice* device) {
  device->deviceId = state.device.deviceId;
  device->vendorId = state.device.vendorId;
  device->name = state.device.deviceName;
  device->renderer = state.device.renderer;
  device->subgroupSize = state.device.subgroupSize;
  device->discrete = state.device.discrete;
}

void lovrGraphicsGetFeatures(GraphicsFeatures* features) {
  features->textureBC = state.features.textureBC;
  features->textureASTC = state.features.textureASTC;
  features->wireframe = state.features.wireframe;
  features->depthClamp = state.features.depthClamp;
  features->depthResolve = state.features.depthResolve;
  features->indirectDrawFirstInstance = state.features.indirectDrawFirstInstance;
  features->float64 = state.features.float64;
  features->int64 = state.features.int64;
  features->int16 = state.features.int16;
}

void lovrGraphicsGetLimits(GraphicsLimits* limits) {
  limits->textureSize2D = state.limits.textureSize2D;
  limits->textureSize3D = state.limits.textureSize3D;
  limits->textureSizeCube = state.limits.textureSizeCube;
  limits->textureLayers = state.limits.textureLayers;
  limits->renderSize[0] = state.limits.renderSize[0];
  limits->renderSize[1] = state.limits.renderSize[1];
  limits->renderSize[2] = MIN(state.limits.renderSize[2], 6);
  limits->uniformBuffersPerStage = MIN(state.limits.uniformBuffersPerStage - 3, MAX_SHADER_RESOURCES);
  limits->storageBuffersPerStage = MIN(state.limits.storageBuffersPerStage, MAX_SHADER_RESOURCES);
  limits->sampledTexturesPerStage = MIN(state.limits.sampledTexturesPerStage - 7, MAX_SHADER_RESOURCES);
  limits->storageTexturesPerStage = MIN(state.limits.storageTexturesPerStage, MAX_SHADER_RESOURCES);
  limits->samplersPerStage = MIN(state.limits.samplersPerStage - 1, MAX_SHADER_RESOURCES);
  limits->resourcesPerShader = MAX_SHADER_RESOURCES;
  limits->uniformBufferRange = state.limits.uniformBufferRange;
  limits->storageBufferRange = state.limits.storageBufferRange;
  limits->uniformBufferAlign = state.limits.uniformBufferAlign;
  limits->storageBufferAlign = state.limits.storageBufferAlign;
  limits->vertexAttributes = 10;
  limits->vertexBufferStride = state.limits.vertexBufferStride;
  limits->vertexShaderOutputs = 10;
  limits->clipDistances = state.limits.clipDistances;
  limits->cullDistances = state.limits.cullDistances;
  limits->clipAndCullDistances = state.limits.clipAndCullDistances;
  memcpy(limits->workgroupCount, state.limits.workgroupCount, 3 * sizeof(uint32_t));
  memcpy(limits->workgroupSize, state.limits.workgroupSize, 3 * sizeof(uint32_t));
  limits->totalWorkgroupSize = state.limits.totalWorkgroupSize;
  limits->computeSharedMemory = state.limits.computeSharedMemory;
  limits->indirectDrawCount = state.limits.indirectDrawCount;
  limits->instances = state.limits.instances;
  limits->anisotropy = state.limits.anisotropy;
  limits->pointSize = state.limits.pointSize;
}

uint32_t lovrGraphicsGetFormatSupport(uint32_t format, uint32_t features) {
  uint32_t support = 0;
  for (uint32_t i = 0; i < 2; i++) {
    uint8_t supports = state.features.formats[format][i];
    if (features) {
      support |=
        (((~features & TEXTURE_FEATURE_SAMPLE) || (supports & GPU_FEATURE_SAMPLE)) &&
        ((~features & TEXTURE_FEATURE_RENDER) || (supports & GPU_FEATURE_RENDER)) &&
        ((~features & TEXTURE_FEATURE_STORAGE) || (supports & GPU_FEATURE_STORAGE)) &&
        ((~features & TEXTURE_FEATURE_BLIT) || (supports & GPU_FEATURE_BLIT))) << i;
    } else {
      support |= !!supports << i;
    }
  }
  return support;
}

void lovrGraphicsGetShaderCache(void* data, size_t* size) {
  gpu_pipeline_get_cache(data, size);
}

void lovrGraphicsGetBackgroundColor(float background[4]) {
  background[0] = lovrMathLinearToGamma(state.background[0]);
  background[1] = lovrMathLinearToGamma(state.background[1]);
  background[2] = lovrMathLinearToGamma(state.background[2]);
  background[3] = state.background[3];
}

void lovrGraphicsSetBackgroundColor(float background[4]) {
  state.background[0] = lovrMathGammaToLinear(background[0]);
  state.background[1] = lovrMathGammaToLinear(background[1]);
  state.background[2] = lovrMathGammaToLinear(background[2]);
  state.background[3] = background[3];
}

bool lovrGraphicsIsTimingEnabled(void) {
  return state.timingEnabled;
}

void lovrGraphicsSetTimingEnabled(bool enable) {
  state.timingEnabled = enable;
}

static bool recordComputePass(Pass* pass, gpu_stream* stream) {
  if (pass->computeCount == 0) {
    return true;
  }

  gpu_pipeline* pipeline = NULL;
  gpu_bundle_info* bundleInfo = NULL;
  gpu_bundle* uniformBundle = NULL;
  gpu_buffer* uniformBuffer = NULL;
  uint32_t uniformOffset = 0;
  uint32_t uniformSize = 0;

  gpu_compute_begin(stream);

  for (uint32_t i = 0; i < pass->computeCount; i++) {
    Compute* compute = &pass->computes[i];

    if (compute->shader->computePipeline != pipeline) {
      gpu_bind_pipeline(stream, compute->shader->computePipeline, GPU_PIPELINE_COMPUTE);
      pipeline = compute->shader->computePipeline;
    }

    if (compute->bundleInfo != bundleInfo) {
      bundleInfo = compute->bundleInfo;
      gpu_bundle* bundle = getBundle(compute->shader->layout, bundleInfo->bindings, bundleInfo->count);
      if (!bundle) return false;
      gpu_bind_bundles(stream, compute->shader->gpu, &bundle, 0, 1, NULL, 0);
    }

    if (compute->uniformBuffer && (compute->uniformBuffer != uniformBuffer || compute->uniformOffset != uniformOffset)) {
      if (compute->uniformBuffer != uniformBuffer || compute->shader->uniformSize != uniformSize) {
        uniformSize = compute->shader->uniformSize;
        uniformBundle = getBundle(state.uniformLayout, &(gpu_binding) {
          .number = 0,
          .type = GPU_SLOT_UNIFORM_BUFFER_DYNAMIC,
          .buffer.object = compute->uniformBuffer,
          .buffer.extent = compute->shader->uniformSize
        }, 1);

        if (!uniformBundle) {
          return false;
        }
      }

      gpu_bind_bundles(stream, compute->shader->gpu, &uniformBundle, 1, 1, &compute->uniformOffset, 1);
      uniformBuffer = compute->uniformBuffer;
      uniformOffset = compute->uniformOffset;
    }

    if (compute->flags & COMPUTE_INDIRECT) {
      gpu_compute_indirect(stream, compute->indirect.buffer, compute->indirect.offset);
    } else {
      gpu_compute(stream, compute->x, compute->y, compute->z);
    }

    if ((compute->flags & COMPUTE_BARRIER) && i < pass->computeCount - 1) {
      gpu_sync(stream, &(gpu_barrier) {
        .prev = GPU_PHASE_SHADER_COMPUTE,
        .next = GPU_PHASE_INDIRECT | GPU_PHASE_SHADER_COMPUTE,
        .flush = GPU_CACHE_STORAGE_WRITE,
        .clear = GPU_CACHE_INDIRECT | GPU_CACHE_UNIFORM | GPU_CACHE_TEXTURE | GPU_CACHE_STORAGE_READ
      }, 1);
    }
  }

  gpu_compute_end(stream);
  return true;
}

static bool recordRenderPass(Pass* pass, gpu_stream* stream) {
  Canvas* canvas = &pass->canvas;

  if (canvas->count == 0 && !canvas->depth.texture) {
    return true;
  }

  // Canvas

  gpu_canvas target = { 0 };

  Texture* texture = canvas->color[0].texture;
  for (uint32_t i = 0; i < canvas->count; i++, texture = canvas->color[i].texture) {
    target.color[i] = (gpu_color_attachment) {
      .texture = canvas->resolve ? getScratchTexture(stream, canvas, texture->info.format, texture->info.srgb) : texture->renderView,
      .resolve = canvas->resolve ? texture->renderView : NULL,
      .clear[0] = canvas->color[i].clear[0],
      .clear[1] = canvas->color[i].clear[1],
      .clear[2] = canvas->color[i].clear[2],
      .clear[3] = canvas->color[i].clear[3]
    };

    if (canvas->resolve && !target.color[i].texture) {
      return false;
    }
  }

  if ((texture = canvas->depth.texture) != NULL || canvas->depth.format) {
    target.depth = (gpu_depth_attachment) {
      .texture = canvas->resolve || !texture ? getScratchTexture(stream, canvas, canvas->depth.format, false) : texture->renderView,
      .resolve = canvas->resolve && texture ? texture->renderView : NULL,
      .clear = canvas->depth.clear
    };

    if ((canvas->resolve || !texture) && !target.depth.texture) {
      return false;
    }
  }

  target.pass = pass->gpu;
  target.width = canvas->width;
  target.height = canvas->height;

  // Cameras

  Camera* camera = pass->cameras;
  for (uint32_t c = 0; c < pass->cameraCount; c++) {
    for (uint32_t v = 0; v < canvas->views; v++, camera++) {
      mat4_init(camera->viewProjection, camera->projection);
      mat4_init(camera->inverseProjection, camera->projection);
      mat4_mul(camera->viewProjection, camera->viewMatrix);
      mat4_invert(camera->inverseProjection);
    }
  }

  // Frustum Culling

  uint32_t activeDrawCount = 0;
  uint16_t* activeDraws = tempAlloc(&state.allocator, pass->drawCount * sizeof(uint16_t));

  if (pass->flags & NEEDS_VIEW_CULL) {
    typedef struct { float planes[6][4]; } Frustum;
    Frustum* frusta = tempAlloc(&state.allocator, canvas->views * sizeof(Frustum));
    uint32_t drawIndex = 0;

    for (uint32_t c = 0; c < pass->cameraCount; c++) {
      for (uint32_t v = 0; v < canvas->views; v++) {
        float* m = pass->cameras[c * canvas->views + v].viewProjection;
        memcpy(frusta[v].planes, (float[6][4]) {
          { (m[3] + m[0]), (m[7] + m[4]), (m[11] + m[8]), (m[15] + m[12]) }, // Left
          { (m[3] - m[0]), (m[7] - m[4]), (m[11] - m[8]), (m[15] - m[12]) }, // Right
          { (m[3] + m[1]), (m[7] + m[5]), (m[11] + m[9]), (m[15] + m[13]) }, // Bottom
          { (m[3] - m[1]), (m[7] - m[5]), (m[11] - m[9]), (m[15] - m[13]) }, // Top
          { m[2], m[6], m[10], m[14] }, // Near
          { (m[3] - m[2]), (m[7] - m[6]), (m[11] - m[10]), (m[15] - m[14]) } // Far
        }, sizeof(Frustum));
      }

      while (drawIndex < pass->drawCount) {
        Draw* draw = &pass->draws[drawIndex];

        if (draw->camera != c) {
          break;
        }

        if (~draw->flags & DRAW_HAS_BOUNDS) {
          activeDraws[activeDrawCount++] = drawIndex++;
          continue;
        }

        float* center = draw->bounds + 0;
        float* extent = draw->bounds + 3;

        float corners[8][3] = {
          { center[0] - extent[0], center[1] - extent[1], center[2] - extent[2] },
          { center[0] - extent[0], center[1] - extent[1], center[2] + extent[2] },
          { center[0] - extent[0], center[1] + extent[1], center[2] - extent[2] },
          { center[0] - extent[0], center[1] + extent[1], center[2] + extent[2] },
          { center[0] + extent[0], center[1] - extent[1], center[2] - extent[2] },
          { center[0] + extent[0], center[1] - extent[1], center[2] + extent[2] },
          { center[0] + extent[0], center[1] + extent[1], center[2] - extent[2] },
          { center[0] + extent[0], center[1] + extent[1], center[2] + extent[2] }
        };

        for (uint32_t i = 0; i < COUNTOF(corners); i++) {
          mat4_mulPoint(draw->transform, corners[i]);
        }

        uint32_t visible = canvas->views;

        for (uint32_t v = 0; v < canvas->views; v++) {
          for (uint32_t p = 0; p < 6; p++) {
            bool inside = false;

            for (uint32_t c = 0; c < COUNTOF(corners); c++) {
              if (vec3_dot(corners[c], frusta[v].planes[p]) + frusta[v].planes[p][3] > 0.f) {
                inside = true;
                break;
              }
            }

            if (!inside) {
              visible--;
              break;
            }
          }
        }

        if (visible) {
          activeDraws[activeDrawCount++] = drawIndex;
        }

        drawIndex++;
      }
    }
  } else {
    for (uint32_t i = 0; i < pass->drawCount; i++) {
      activeDraws[activeDrawCount++] = i;
    }
  }

  pass->stats.drawsCulled = pass->drawCount - activeDrawCount;

  if (activeDrawCount == 0) {
    gpu_render_begin(stream, &target);
    gpu_render_end(stream, &target);
    return true;
  }

  // Builtins

  gpu_binding builtins[] = {
    { 0, GPU_SLOT_UNIFORM_BUFFER, .buffer = { 0 } },
    { 1, GPU_SLOT_UNIFORM_BUFFER_DYNAMIC, .buffer = { 0 } },
    { 2, GPU_SLOT_UNIFORM_BUFFER_DYNAMIC, .buffer = { 0 } },
    { 3, GPU_SLOT_SAMPLER, .texture.sampler = pass->sampler ? pass->sampler->gpu : state.defaultSamplers[FILTER_LINEAR]->gpu }
  };

  BufferView view;
  size_t align = state.limits.uniformBufferAlign;

  // Globals
  view = getBuffer(GPU_BUFFER_STREAM, sizeof(Globals), align);
  if (!view.buffer) return false;
  builtins[0].buffer = (gpu_buffer_binding) { view.buffer, view.offset, view.extent };
  Globals* global = view.pointer;
  global->resolution[0] = canvas->width;
  global->resolution[1] = canvas->height;
  global->time = lovrHeadsetInterface ? lovrHeadsetInterface->getDisplayTime() : os_get_time();

  // Cameras
  uint32_t padding = (6 - canvas->views) * sizeof(Camera); // Ensure shader can always access all 6 cameras
  view = getBuffer(GPU_BUFFER_STREAM, pass->cameraCount * canvas->views * sizeof(Camera) + padding, align);
  if (!view.buffer) return false;
  builtins[1].buffer = (gpu_buffer_binding) { view.buffer, view.offset, 6 * sizeof(Camera) };
  memcpy(view.pointer, pass->cameras, pass->cameraCount * canvas->views * sizeof(Camera));

  // DrawData
  uint32_t alignedDrawCount = ALIGN(activeDrawCount, 256);
  view = getBuffer(GPU_BUFFER_STREAM, alignedDrawCount * sizeof(DrawData), align);
  if (!view.buffer) return false;
  builtins[2].buffer = (gpu_buffer_binding) { view.buffer, view.offset, 256 * sizeof(DrawData) };
  DrawData* data = view.pointer;

  for (uint32_t i = 0; i < activeDrawCount; i++, data++) {
    Draw* draw = &pass->draws[activeDraws[i]];
    // transform is provided as 4x3 row-major matrix for packing reasons, need to transpose
    data->transform[0] = draw->transform[0];
    data->transform[1] = draw->transform[4];
    data->transform[2] = draw->transform[8];
    data->transform[3] = draw->transform[12];
    data->transform[4] = draw->transform[1];
    data->transform[5] = draw->transform[5];
    data->transform[6] = draw->transform[9];
    data->transform[7] = draw->transform[13];
    data->transform[8] = draw->transform[2];
    data->transform[9] = draw->transform[6];
    data->transform[10] = draw->transform[10];
    data->transform[11] = draw->transform[14];
    data->color[0] = draw->color[0];
    data->color[1] = draw->color[1];
    data->color[2] = draw->color[2];
    data->color[3] = draw->color[3];
  }

  gpu_bundle* builtinBundle = getBundle(state.builtinLayout, builtins, COUNTOF(builtins));
  if (!builtinBundle) return false;

  // Pipelines

  if (!pass->draws[pass->drawCount - 1].pipeline) {
    uint32_t first = 0;

    while (pass->draws[first].pipeline) {
      first++; // TODO could binary search or cache
    }

    for (uint32_t i = first; i < pass->drawCount; i++) {
      Draw* prev = &pass->draws[i - 1];
      Draw* draw = &pass->draws[i];

      if (i > 0 && draw->pipelineInfo == prev->pipelineInfo) {
        draw->pipeline = prev->pipeline;
        continue;
      }

      uint64_t hash = hash64(draw->pipelineInfo, sizeof(gpu_pipeline_info));
      uint64_t index = map_get(&state.pipelineLookup, hash);

      if (index == MAP_NIL) {
        index = state.pipelineCount++;
        lovrAssert(index < MAX_PIPELINES, "Too many pipelines!");
        lovrAssert(os_vm_commit(state.pipelines, state.pipelineCount * gpu_sizeof_pipeline()), "Out of memory");
        lovrAssert(gpu_pipeline_init_graphics(getPipeline(index), draw->pipelineInfo), "Failed to create GPU pipeline: %s", gpu_get_error());
        map_set(&state.pipelineLookup, hash, index);
      }

      draw->pipeline = getPipeline(index);
    }
  }

  // Bundles

  Draw* prev = NULL;
  for (uint32_t i = 0; i < activeDrawCount; i++) {
    Draw* draw = &pass->draws[activeDraws[i]];

    if (i > 0 && draw->bundleInfo == prev->bundleInfo) {
      draw->bundle = prev->bundle;
      continue;
    }

    if (draw->bundleInfo) {
      draw->bundle = getBundle(draw->shader->layout, draw->bundleInfo->bindings, draw->bundleInfo->count);
      if (!draw->bundle) {
        return false;
      }
    } else {
      draw->bundle = NULL;
    }

    prev = draw;
  }

  // Tally

  if (pass->tally.active) {
    lovrPassFinishTally(pass, NULL);
  }

  if (pass->tally.buffer && pass->tally.count > 0) {
    if (!pass->tally.gpu) {
      pass->tally.gpu = lovrMalloc(gpu_sizeof_tally());

      gpu_tally_info tallyInfo = {
        .type = GPU_TALLY_PIXEL,
        .count = MAX_TALLIES * state.limits.renderSize[2]
      };

      lovrAssert(gpu_tally_init(pass->tally.gpu, &tallyInfo), "Failed to create tally: %s", gpu_get_error());

      BufferInfo bufferInfo = {
        .size = MAX_TALLIES * state.limits.renderSize[2] * sizeof(uint32_t)
      };

      pass->tally.tempBuffer = lovrBufferCreate(&bufferInfo, NULL);
      lovrAssert(pass->tally.buffer, "Failed to create tally buffer: %s", lovrGetError());
    }

    gpu_clear_tally(stream, pass->tally.gpu, 0, pass->tally.count * canvas->views);
  }

  // Do the thing!

  uint8_t tally = 0xff;
  uint16_t cameraIndex = 0xffff;
  uint16_t viewport = 0xffff;
  uint16_t scissor = 0xffff;
  gpu_pipeline* pipeline = NULL;
  gpu_bundle* bundle = NULL;
  Material* material = NULL;
  gpu_buffer* vertexBuffer = NULL;
  uint32_t vertexBufferOffset = 0;
  gpu_buffer* indexBuffer = NULL;
  gpu_index_type indexType = 0;
  gpu_buffer* uniformBuffer = NULL;
  uint32_t uniformOffset = 0;
  uint32_t uniformSize = 0;
  gpu_bundle* uniformBundle = NULL;

  gpu_render_begin(stream, &target);
  gpu_bind_vertex_buffers(stream, &state.defaultBuffer->gpu, &state.defaultBuffer->base, 1, 1);

  for (uint32_t i = 0; i < activeDrawCount; i++) {
    Draw* draw = &pass->draws[activeDraws[i]];

    if (pass->tally.buffer && draw->tally != tally) {
      if (tally != 0xff) gpu_tally_finish(stream, pass->tally.gpu, tally * canvas->views);
      if (draw->tally != 0xff) gpu_tally_begin(stream, pass->tally.gpu, draw->tally * canvas->views);
      tally = draw->tally;
    }

    if (draw->viewport != viewport) {
      float* v = pass->viewports + draw->viewport * 6;
      gpu_set_viewport(stream, v, v + 4);
      viewport = draw->viewport;
    }

    if (draw->scissor != scissor) {
      gpu_set_scissor(stream, pass->scissors + draw->scissor * 4);
      scissor = draw->scissor;
    }

    if (draw->pipeline != pipeline) {
      gpu_bind_pipeline(stream, draw->pipeline, GPU_PIPELINE_GRAPHICS);
      pipeline = draw->pipeline;
    }

    if ((i & 0xff) == 0 || draw->camera != cameraIndex) {
      uint32_t dynamicOffsets[] = { draw->camera * canvas->views * sizeof(Camera), (i >> 8) * 256 * sizeof(DrawData) };
      gpu_bind_bundles(stream, draw->shader->gpu, &builtinBundle, 0, 1, dynamicOffsets, COUNTOF(dynamicOffsets));
      cameraIndex = draw->camera;
    }

    if (draw->material != material) {
      gpu_bind_bundles(stream, draw->shader->gpu, &draw->material->bundle, 1, 1, NULL, 0);
      material = draw->material;
    }

    if (draw->bundle && (draw->bundle != bundle)) {
      gpu_bind_bundles(stream, draw->shader->gpu, &draw->bundle, 2, 1, NULL, 0);
      bundle = draw->bundle;
    }

    if (draw->uniformBuffer && (draw->uniformBuffer != uniformBuffer || draw->uniformOffset != uniformOffset)) {
      if (draw->uniformBuffer != uniformBuffer || draw->shader->uniformSize != uniformSize) {
        uniformSize = draw->shader->uniformSize;
        uniformBundle = getBundle(state.uniformLayout, &(gpu_binding) {
          .number = 0,
          .type = GPU_SLOT_UNIFORM_BUFFER_DYNAMIC,
          .buffer.object = draw->uniformBuffer,
          .buffer.extent = draw->shader->uniformSize
        }, 1);

        if (!uniformBundle) {
          return false;
        }
      }

      gpu_bind_bundles(stream, draw->shader->gpu, &uniformBundle, 3, 1, &draw->uniformOffset, 1);
      uniformBuffer = draw->uniformBuffer;
      uniformOffset = draw->uniformOffset;
    }

    if (draw->vertexBuffer && (draw->vertexBuffer != vertexBuffer || draw->vertexBufferOffset != vertexBufferOffset)) {
      gpu_bind_vertex_buffers(stream, &draw->vertexBuffer, &draw->vertexBufferOffset, 0, 1);
      vertexBuffer = draw->vertexBuffer;
      vertexBufferOffset = draw->vertexBufferOffset;
    }

    if (draw->indexBuffer) {
      gpu_index_type type = (draw->flags & DRAW_INDEX32) ? GPU_INDEX_U32 : GPU_INDEX_U16;
      if (draw->indexBuffer != indexBuffer || type != indexType) {
        gpu_bind_index_buffer(stream, draw->indexBuffer, 0, type);
        indexBuffer = draw->indexBuffer;
        indexType = type;
      }
    }

    if (draw->shader->pushConstantSize >= 4) {
      gpu_push_constants(stream, draw->shader->gpu, (uint32_t[1]) { i & 0xff }, 4);
    }

    if (draw->flags & DRAW_INDIRECT) {
      if (draw->indexBuffer) {
        gpu_draw_indirect_indexed(stream, draw->indirect.buffer, draw->indirect.offset, draw->indirect.count, draw->indirect.stride);
      } else {
        gpu_draw_indirect(stream, draw->indirect.buffer, draw->indirect.offset, draw->indirect.count, draw->indirect.stride);
      }
    } else {
      if (draw->indexBuffer) {
        gpu_draw_indexed(stream, draw->count, draw->instances, draw->start, draw->baseVertex, 0);
      } else {
        gpu_draw(stream, draw->count, draw->instances, draw->start, 0);
      }
    }
  }

  if (tally != 0xff) {
    gpu_tally_finish(stream, pass->tally.gpu, tally * canvas->views);
  }

  gpu_render_end(stream, &target);

  // Automipmap

  bool synchronized = false;
  for (uint32_t t = 0; t < canvas->count; t++) {
    if (canvas->color[t].texture->info.mipmaps > 1) {
      if (!synchronized) {
        synchronized = true;
        gpu_sync(stream, &(gpu_barrier) {
          .prev = GPU_PHASE_COLOR,
          .next = GPU_PHASE_BLIT,
          .flush = GPU_CACHE_COLOR_WRITE,
          .clear = GPU_CACHE_TRANSFER_READ
        }, 1);
      }

      mipmapTexture(stream, canvas->color[t].texture, 0, ~0u);
    }
  }

  texture = canvas->depth.texture;
  if (canvas->depth.texture && canvas->depth.texture->info.mipmaps > 1) {
    gpu_sync(stream, &(gpu_barrier) {
      .prev = GPU_PHASE_DEPTH_EARLY | GPU_PHASE_DEPTH_LATE,
      .next = GPU_PHASE_BLIT,
      .flush = GPU_CACHE_DEPTH_WRITE,
      .clear = GPU_CACHE_TRANSFER_READ
    }, 1);

    mipmapTexture(stream, canvas->depth.texture, 0, ~0u);
  }

  // Tally copy

  if (pass->tally.buffer && pass->tally.count > 0) {
    Tally* tally = &pass->tally;
    uint32_t count = MIN(tally->count, (tally->buffer->info.size - tally->bufferOffset) / 4);
    Buffer* tempBuffer = pass->tally.tempBuffer;

    gpu_copy_tally_buffer(stream, tally->gpu, tempBuffer->gpu, 0, tempBuffer->base, count * canvas->views);

    gpu_barrier barrier = {
      .prev = GPU_PHASE_COPY,
      .next = GPU_PHASE_SHADER_COMPUTE,
      .flush = GPU_CACHE_TRANSFER_WRITE,
      .clear = GPU_CACHE_STORAGE_READ
    };

    Access access = {
      .sync = &tally->buffer->sync,
      .object = tally->buffer,
      .phase = GPU_PHASE_SHADER_COMPUTE,
      .cache = GPU_CACHE_STORAGE_WRITE
    };

    syncResource(&access, &barrier);
    gpu_sync(stream, &barrier, 1);

    gpu_binding bindings[] = {
      { 0, GPU_SLOT_STORAGE_BUFFER, .buffer = { tempBuffer->gpu, tempBuffer->base, count * canvas->views * sizeof(uint32_t) } },
      { 1, GPU_SLOT_STORAGE_BUFFER, .buffer = { tally->buffer->gpu, tally->buffer->base + tally->bufferOffset, count * sizeof(uint32_t) } }
    };

    Shader* shader = lovrGraphicsGetDefaultShader(SHADER_TALLY_MERGE);
    gpu_bundle* bundle = getBundle(shader->layout, bindings, COUNTOF(bindings));
    if (!shader || !bundle) return false;

    gpu_compute_begin(stream);
    gpu_bind_pipeline(stream, shader->computePipeline, GPU_PIPELINE_COMPUTE);
    gpu_bind_bundles(stream, shader->gpu, &bundle, 0, 1, NULL, 0);
    gpu_push_constants(stream, shader->gpu, (uint32_t[2]) { count, canvas->views }, 2 * sizeof(uint32_t));
    gpu_compute(stream, (count + 31) / 32, 1, 1);
    gpu_compute_end(stream);
  }

  return true;
}

static Readback* lovrReadbackCreateTimestamp(TimingInfo* passes, uint32_t count, BufferView view);

bool lovrGraphicsSubmit(Pass** passes, uint32_t count) {
  if (!beginFrame()) {
    return false;
  }

  bool xrCanvas = false;
  uint32_t streamCount = 0;
  uint32_t maxStreams = count + 3;
  gpu_stream** streams = tempAlloc(&state.allocator, maxStreams * sizeof(gpu_stream*));
  gpu_barrier* computeBarriers = tempAlloc(&state.allocator, count * sizeof(gpu_barrier));
  gpu_barrier* renderBarriers = tempAlloc(&state.allocator, count * sizeof(gpu_barrier));

  if (count > 0) {
    memset(computeBarriers, 0, count * sizeof(gpu_barrier));
    memset(renderBarriers, 0, count * sizeof(gpu_barrier));
  }

  if (state.transferBarrier.prev != 0 && state.transferBarrier.next != 0) {
    gpu_stream* stream = streams[streamCount++] = gpu_stream_begin(NULL);
    gpu_sync(stream, &state.transferBarrier, 1);
    gpu_stream_end(stream);
  }

  streams[streamCount++] = state.stream;

  // Synchronization
  for (uint32_t i = 0; i < count; i++) {
    Pass* pass = passes[i];
    Canvas* canvas = &pass->canvas;

    state.shouldPresent |= pass == state.windowPass;

    // Compute
    for (AccessBlock* block = pass->access[ACCESS_COMPUTE]; block != NULL; block = block->next) {
      for (uint64_t j = 0; j < block->count; j++) {
        Access* access = &block->list[j];
        if (access->sync->barrier != &computeBarriers[i] && syncResource(access, access->sync->barrier)) {
          access->sync->barrier = &computeBarriers[i];
        }
      }
    }

    // Color attachments
    for (uint32_t t = 0; t < canvas->count; t++) {
      if (canvas->color[t].texture == state.window) continue;
      Texture* texture = canvas->color[t].texture;

      Access access = {
        .sync = &texture->root->sync,
        .object = texture,
        .phase = GPU_PHASE_COLOR,
        .cache = GPU_CACHE_COLOR_WRITE | ((!canvas->resolve && canvas->color[t].load == LOAD_KEEP) ? GPU_CACHE_COLOR_READ : 0)
      };

      syncResource(&access, access.sync->barrier);
      access.sync->barrier = &renderBarriers[i];

      if (texture->info.mipmaps > 1) {
        access.sync->writePhase = GPU_PHASE_BLIT;
        access.sync->pendingWrite = GPU_CACHE_TRANSFER_WRITE;
      }

      if (texture->info.xr && !texture->xrAcquired) {
        gpu_xr_acquire(state.stream, texture->gpu);
        texture->xrAcquired = true;
        xrCanvas = true;
      }
    }

    // Depth attachment
    if (canvas->depth.texture) {
      Texture* texture = canvas->depth.texture;

      Access access = {
        .sync = &texture->root->sync,
        .object = texture
      };

      if (canvas->resolve) {
        access.phase = GPU_PHASE_COLOR; // Depth resolve operations act like color resolves w.r.t. sync
        access.cache = GPU_CACHE_COLOR_WRITE;
      } else {
        access.phase = canvas->depth.load == LOAD_KEEP ? GPU_PHASE_DEPTH_EARLY : GPU_PHASE_DEPTH_LATE;
        access.cache = GPU_CACHE_DEPTH_WRITE | (canvas->depth.load == LOAD_KEEP ? GPU_CACHE_DEPTH_READ : 0);
      }

      syncResource(&access, access.sync->barrier);
      access.sync->barrier = &renderBarriers[i];

      if (texture->info.mipmaps > 1) {
        access.sync->writePhase = GPU_PHASE_BLIT;
        access.sync->pendingWrite = GPU_CACHE_TRANSFER_WRITE;
      }

      if (texture->info.xr && !texture->xrAcquired) {
        gpu_xr_acquire(state.stream, texture->gpu);
        texture->xrAcquired = true;
        xrCanvas = true;
      }
    }

    // Render resources (all read-only)
    for (AccessBlock* block = pass->access[ACCESS_RENDER]; block != NULL; block = block->next) {
      for (uint64_t j = 0; j < block->count; j++) {
        syncResource(&block->list[j], block->list[j].sync->barrier);
      }
    }
  }

  TimingInfo* times = NULL;

  if (state.timingEnabled && count > 0) {
    times = lovrMalloc(count * sizeof(TimingInfo));

    for (uint32_t i = 0; i < count; i++) {
      times[i].pass = passes[i];
      lovrRetain(passes[i]);
    }

    uint32_t timestampCount = 2 * count;

    if (timestampCount > state.timestampCount) {
      if (state.timestamps) {
        gpu_tally_destroy(state.timestamps);
      } else {
        state.timestamps = lovrMalloc(gpu_sizeof_tally());
      }

      gpu_tally_info info = {
        .type = GPU_TALLY_TIME,
        .count = timestampCount
      };

      lovrAssert(gpu_tally_init(state.timestamps, &info), "Failed to create timestamp tally: %s", gpu_get_error());
      state.timestampCount = timestampCount;
    }

    gpu_clear_tally(state.stream, state.timestamps, 0, timestampCount);
  }

  gpu_sync(state.stream, &state.barrier, 1);

  lovrAssert(gpu_stream_end(state.stream), "Failed to end GPU command buffer: %s", gpu_get_error());

  for (uint32_t i = 0; i < count; i++) {
    gpu_stream* stream = streams[streamCount++] = gpu_stream_begin(passes[i]->label);

    if (state.timingEnabled) {
      times[i].cpuTime = os_get_time();
      gpu_tally_mark(stream, state.timestamps, 2 * i + 0);
    }

    if (!recordComputePass(passes[i], stream)) {
      return false;
    }

    gpu_sync(stream, &computeBarriers[i], 1);

    if (!recordRenderPass(passes[i], stream)) {
      return false;
    }

    gpu_sync(stream, &renderBarriers[i], 1);

    if (state.timingEnabled) {
      times[i].cpuTime = os_get_time() - times[i].cpuTime;
      gpu_tally_mark(stream, state.timestamps, 2 * i + 1);
    }

    lovrAssert(gpu_stream_end(stream), "Failed to end GPU command buffer: %s", gpu_get_error());
  }

  if (xrCanvas || (state.timingEnabled && count > 0)) {
    gpu_stream* stream = streams[streamCount++] = gpu_stream_begin(NULL);
    lovrAssert(stream, "Failed to begin command buffer: %s", gpu_get_error());

    // Timestamp Readback
    if (state.timingEnabled) {
      BufferView view = getBuffer(GPU_BUFFER_DOWNLOAD, 2 * count * sizeof(uint32_t), 4);
      if (!view.buffer) return false;
      gpu_copy_tally_buffer(stream, state.timestamps, view.buffer, 0, view.offset, 2 * count);
      Readback* readback = lovrReadbackCreateTimestamp(times, count, view);
      if (!readback) return false;
      lovrRelease(readback, lovrReadbackDestroy); // It gets freed when it completes
    }

    // OpenXR Swapchain Layout Transitions
    for (uint32_t i = 0; i < count; i++) {
      Canvas* canvas = &passes[i]->canvas;

      for (uint32_t t = 0; t < canvas->count; t++) {
        Texture* texture = canvas->color[t].texture;
        if (texture->info.xr && texture->xrAcquired) {
          gpu_xr_release(stream, texture->gpu);
          texture->xrAcquired = false;
        }
      }

      if (canvas->depth.texture) {
        Texture* texture = canvas->depth.texture;
        if (texture->info.xr && texture->xrAcquired) {
          gpu_xr_release(stream, texture->gpu);
          texture->xrAcquired = false;
        }
      }
    }

    lovrAssert(gpu_stream_end(stream), "Failed to end GPU command buffer: %s", gpu_get_error());
  }

  // Cleanup
  for (uint32_t i = 0; i < count; i++) {
    Canvas* canvas = &passes[i]->canvas;

    // Reset barriers back to the default
    for (uint32_t t = 0; t < canvas->count; t++) {
      canvas->color[t].texture->sync.barrier = &state.barrier;
    }

    if (canvas->depth.texture) {
      canvas->depth.texture->sync.barrier = &state.barrier;
    }

    for (uint32_t j = 0; j < COUNTOF(passes[i]->access); j++) {
      for (AccessBlock* block = passes[i]->access[j]; block != NULL; block = block->next) {
        for (uint32_t k = 0; k < block->count; k++) {
          block->list[k].sync->barrier = &state.barrier;
        }
      }
    }

    // Mark the tick for any buffers that filled up, so we know when to recycle them
    for (BufferBlock* block = passes[i]->buffers.freelist; block; block = block->next) {
      block->tick = state.tick;
    }
  }

  lovrAssert(gpu_submit(streams, streamCount), "Failed to submit GPU command buffers: %s", gpu_get_error());

  state.active = false;
  state.stream = NULL;
  return true;
}

bool lovrGraphicsPresent(void) {
  if (state.shouldPresent) {
    state.window->gpu = NULL;
    state.window->renderView = NULL;
    state.shouldPresent = false;
    lovrAssert(gpu_surface_present(), "Failed to present: %s", gpu_get_error());
  }

  lovrProfileMarkFrame();
  return true;
}

bool lovrGraphicsWait(void) {
  if (state.active) {
    if (!lovrGraphicsSubmit(NULL, 0)) {
      return false;
    }
  }

  lovrAssert(gpu_wait_idle(), "Failed to wait: %s", gpu_get_error());
  processReadbacks();
  return true;
}

// Buffer

uint32_t lovrGraphicsAlignFields(DataField* parent, DataLayout layout) {
  static const struct { uint32_t size, scalarAlign, baseAlign; } table[] = {
    [TYPE_I8x4] = { 4, 1, 4 },
    [TYPE_U8x4] = { 4, 1, 4 },
    [TYPE_SN8x4] = { 4, 1, 4 },
    [TYPE_UN8x4] = { 4, 1, 4 },
    [TYPE_SN10x3] = { 4, 4, 4 },
    [TYPE_UN10x3] = { 4, 4, 4 },
    [TYPE_I16] = { 2, 2, 2 },
    [TYPE_I16x2] = { 4, 2, 4 },
    [TYPE_I16x4] = { 8, 2, 8 },
    [TYPE_U16] = { 2, 2, 2 },
    [TYPE_U16x2] = { 4, 2, 4 },
    [TYPE_U16x4] = { 8, 2, 8 },
    [TYPE_SN16x2] = { 4, 2, 4 },
    [TYPE_SN16x4] = { 8, 2, 8 },
    [TYPE_UN16x2] = { 4, 2, 4 },
    [TYPE_UN16x4] = { 8, 2, 8 },
    [TYPE_I32] = { 4, 4, 4 },
    [TYPE_I32x2] = { 8, 4, 8 },
    [TYPE_I32x3] = { 12, 4, 16 },
    [TYPE_I32x4] = { 16, 4, 16 },
    [TYPE_U32] = { 4, 4, 4 },
    [TYPE_U32x2] = { 8, 4, 8 },
    [TYPE_U32x3] = { 12, 4, 16 },
    [TYPE_U32x4] = { 16, 4, 16 },
    [TYPE_F16x2] = { 4, 2, 4 },
    [TYPE_F16x4] = { 8, 2, 8 },
    [TYPE_F32] = { 4, 4, 4 },
    [TYPE_F32x2] = { 8, 4, 8 },
    [TYPE_F32x3] = { 12, 4, 16 },
    [TYPE_F32x4] = { 16, 4, 16 },
    [TYPE_MAT2] = { 16, 4, 8 },
    [TYPE_MAT3] = { 48, 4, 16 },
    [TYPE_MAT4] = { 64, 4, 16 },
    [TYPE_INDEX16] = { 2, 2, 2 },
    [TYPE_INDEX32] = { 4, 4, 4 }
  };

  uint32_t cursor = 0;
  uint32_t extent = 0;
  uint32_t align = 1;

  if (parent->fieldCount == 0) {
    align = layout == LAYOUT_PACKED ? table[parent->type].scalarAlign : table[parent->type].baseAlign;
    extent = table[parent->type].size;
  }

  for (uint32_t i = 0; i < parent->fieldCount; i++) {
    DataField* field = &parent->fields[i];
    uint32_t length = MAX(field->length, 1);
    uint32_t subalign;

    if (field->fieldCount > 0) {
      subalign = lovrGraphicsAlignFields(field, layout);
    } else {
      subalign = layout == LAYOUT_PACKED ? table[field->type].scalarAlign : table[field->type].baseAlign;

      if (field->length > 0) {
        subalign = layout == LAYOUT_STD140 ? MAX(subalign, 16) : subalign;
        field->stride = MAX(subalign, table[field->type].size);
      } else {
        field->stride = table[field->type].size;
      }
    }

    if (field->offset == 0) {
      field->offset = ALIGN(cursor, subalign);
      cursor = field->offset + length * field->stride;
    }

    align = MAX(align, subalign);
    extent = MAX(extent, field->offset + length * field->stride);
  }

  if (layout == LAYOUT_STD140) align = MAX(align, 16);
  if (parent->stride == 0) parent->stride = ALIGN(extent, align);

  return align;
}

Buffer* lovrBufferCreate(const BufferInfo* info, void** data) {
  uint32_t size = info->size;
  if (size == 0 && info->format) size = info->format->stride * MAX(info->format->length, 1);
  lovrCheck(size > 0, "Buffer size can not be zero");
  lovrCheck(size <= 1 << 30, "Max buffer size is 1GiB");

  uint32_t fieldCount = info->format ? MAX(info->fieldCount, info->format->fieldCount + 1) : 0;

  size_t charCount = 0;
  for (uint32_t i = 0; i < fieldCount; i++) {
    if (!info->format[i].name) continue;
    charCount += strlen(info->format[i].name) + 1;
  }

  charCount = ALIGN(charCount, 8);

  Buffer* buffer = lovrCalloc(sizeof(Buffer) + charCount + fieldCount * sizeof(DataField));
  buffer->ref = 1;
  buffer->info = *info;
  buffer->info.fieldCount = fieldCount;
  buffer->info.size = size;

  if (info->format) {
    char* names = (char*) buffer + sizeof(Buffer);
    DataField* format = buffer->info.format = (DataField*) (names + charCount);
    memcpy(format, info->format, fieldCount * sizeof(DataField));

    // Copy names, hash names, fixup children pointers, set parent pointers
    for (uint32_t i = 0; i < fieldCount; i++) {
      if (format[i].name) {
        size_t length = strlen(format[i].name);
        memcpy(names, format[i].name, length);
        names[length] = '\0';
        format[i].name = names;
        format[i].hash = (uint32_t) hash64(format[i].name, length);
        names += length + 1;
      }

      if (format[i].fields) {
        format[i].fields = format + (format[i].fields - info->format);
      }
    }

    // Root child pointer is optional, and if absent it implicitly points to next field
    if (format->fieldCount > 0 && !format->fields) {
      format->fields = format + 1;
    }

    // Set parent pointers
    for (uint32_t i = 0; i < fieldCount; i++) {
      if (format[i].fields) {
        for (uint32_t j = 0; j < format[i].fieldCount; j++) {
          format[i].fields[j].parent = &format[i];
        }
      }
    }

    buffer->supportsMesh = true;

    // Formats with array/struct fields have extra restrictions, cache it
    for (uint32_t i = 0; i < format->fieldCount; i++) {
      if (format->fields[i].fieldCount > 0 || format->fields[i].length > 0) {
        buffer->supportsMesh = false;
        break;
      }

      if (format->fields[i].type >= TYPE_MAT2 && format->fields[i].type <= TYPE_MAT4) {
        buffer->supportsMesh = false;
        break;
      }

      if (format->fields[i].type == TYPE_INDEX16 && format->fields[i].type == TYPE_INDEX32) {
        buffer->supportsMesh = false;
        break;
      }
    }
  }

  if (!beginFrame()) {
    lovrFree(buffer);
    return NULL;
  }

  uint32_t stride = buffer->info.format ? buffer->info.format->stride : 4;
  uint32_t align = lcm(stride, MAX(state.limits.storageBufferAlign, state.limits.uniformBufferAlign));
  BufferView view = getBuffer(GPU_BUFFER_STATIC, size, align);

  if (!view.buffer) {
    lovrFree(buffer);
    return NULL;
  }

  buffer->gpu = view.buffer;
  buffer->base = view.offset;
  buffer->block = view.block;
  atomic_fetch_add(&buffer->block->ref, 1);

  if (data) {
    if (view.pointer) {
      *data = view.pointer;
    } else {
      BufferView staging = getBuffer(GPU_BUFFER_UPLOAD, size, 4);
      if (!staging.buffer) return lovrFree(buffer), NULL;
      gpu_copy_buffers(state.stream, staging.buffer, buffer->gpu, staging.offset, buffer->base, size);
      buffer->sync.writePhase = GPU_PHASE_COPY;
      buffer->sync.pendingWrite = GPU_CACHE_TRANSFER_WRITE;
      buffer->sync.lastTransferWrite = state.tick;
      *data = staging.pointer;
    }
  }

  buffer->sync.barrier = &state.barrier;

  return buffer;
}

void lovrBufferDestroy(void* ref) {
  Buffer* buffer = ref;
  BufferAllocator* allocator = &state.bufferAllocators[GPU_BUFFER_STATIC];
  if (buffer->block != allocator->current && atomic_fetch_sub(&buffer->block->ref, 1) == 1) {
    freeBlock(allocator, buffer->block);
  }
  lovrFree(buffer);
}

const BufferInfo* lovrBufferGetInfo(Buffer* buffer) {
  return &buffer->info;
}

void* lovrBufferGetData(Buffer* buffer, uint32_t offset, uint32_t extent) {
  if (!beginFrame()) return NULL;

  if (extent == ~0u) extent = buffer->info.size - offset;
  lovrCheck(offset + extent <= buffer->info.size, "Buffer read range goes past the end of the Buffer");

  gpu_barrier barrier = syncTransfer(&buffer->sync, GPU_PHASE_COPY, GPU_CACHE_TRANSFER_READ);
  gpu_sync(state.stream, &barrier, 1);

  BufferView view = getBuffer(GPU_BUFFER_DOWNLOAD, extent, 4);
  if (!view.buffer) return NULL;

  gpu_copy_buffers(state.stream, buffer->gpu, view.buffer, buffer->base + offset, view.offset, extent);

  if (!lovrGraphicsSubmit(NULL, 0) || !lovrGraphicsWait()) {
    return NULL;
  }

  return view.pointer;
}

void* lovrBufferSetData(Buffer* buffer, uint32_t offset, uint32_t extent) {
  if (!beginFrame()) return NULL;

  if (extent == ~0u) extent = buffer->info.size - offset;
  lovrCheck(offset + extent <= buffer->info.size, "Attempt to write past the end of the Buffer");

  gpu_barrier barrier = syncTransfer(&buffer->sync, GPU_PHASE_COPY, GPU_CACHE_TRANSFER_WRITE);
  gpu_sync(state.stream, &barrier, 1);

  BufferView view = getBuffer(GPU_BUFFER_UPLOAD, extent, 4);
  if (!view.buffer) return NULL;

  gpu_copy_buffers(state.stream, view.buffer, buffer->gpu, view.offset, buffer->base + offset, extent);
  return view.pointer;
}

bool lovrBufferCopy(Buffer* src, Buffer* dst, uint32_t srcOffset, uint32_t dstOffset, uint32_t extent) {
  if (!beginFrame()) return false;

  lovrCheck(srcOffset + extent <= src->info.size, "Buffer copy range goes past the end of the source Buffer");
  lovrCheck(dstOffset + extent <= dst->info.size, "Buffer copy range goes past the end of the destination Buffer");
  lovrCheck(src != dst || (srcOffset >= dstOffset + extent || dstOffset >= srcOffset + extent), "Copying part of a Buffer to itself requires non-overlapping copy regions");

  gpu_barrier barriers[2];
  barriers[0] = syncTransfer(&src->sync, GPU_PHASE_COPY, GPU_CACHE_TRANSFER_READ);
  barriers[1] = syncTransfer(&dst->sync, GPU_PHASE_COPY, GPU_CACHE_TRANSFER_WRITE);
  gpu_sync(state.stream, barriers, 2);

  gpu_copy_buffers(state.stream, src->gpu, dst->gpu, src->base + srcOffset, dst->base + dstOffset, extent);
  return true;
}

bool lovrBufferClear(Buffer* buffer, uint32_t offset, uint32_t extent, uint32_t value) {
  if (extent == 0) return true;
  if (extent == ~0u) extent = buffer->info.size - offset;
  lovrCheck(offset % 4 == 0, "Buffer clear offset must be a multiple of 4");
  lovrCheck(extent % 4 == 0, "Buffer clear extent must be a multiple of 4");
  lovrCheck(offset + extent <= buffer->info.size, "Buffer clear range goes past the end of the Buffer");

  if (!beginFrame()) return false;

  gpu_barrier barrier = syncTransfer(&buffer->sync, GPU_PHASE_CLEAR, GPU_CACHE_TRANSFER_WRITE);
  gpu_sync(state.stream, &barrier, 1);

  gpu_clear_buffer(state.stream, buffer->gpu, buffer->base + offset, extent, value);
  return true;
}

// Texture

bool lovrGraphicsGetWindowTexture(Texture** texture) {
  if (!state.window && os_window_is_open()) {
    uint32_t width, height;
    os_window_get_size(&width, &height);

    float density = os_window_get_pixel_density();
    width *= density;
    height *= density;

    state.window = lovrCalloc(sizeof(Texture));
    state.window->ref = 1;
    state.window->gpu = NULL;
    state.window->sampleView = NULL;
    state.window->renderView = NULL;
    state.window->info = (TextureInfo) {
      .type = TEXTURE_2D,
      .format = GPU_FORMAT_SURFACE,
      .width = width,
      .height = height,
      .layers = 1,
      .mipmaps = 1,
      .usage = TEXTURE_RENDER,
      .srgb = true
    };

    bool vsync = state.config.vsync;
#ifndef LOVR_DISABLE_HEADSET
    if (lovrHeadsetInterface && lovrHeadsetInterface->driverType != DRIVER_SIMULATOR) {
      vsync = false;
    }
#endif

    gpu_surface_info info = {
      .width = width,
      .height = height,
      .vsync = vsync,
#if defined(_WIN32)
      .win32.window = os_get_win32_window(),
      .win32.instance = os_get_win32_instance()
#elif defined(__APPLE__)
      .macos.layer = os_get_ca_metal_layer()
#elif defined(__linux__) && !defined(__ANDROID__)
      .xcb.connection = os_get_xcb_connection(),
      .xcb.window = os_get_xcb_window()
#endif
    };

    if (!gpu_surface_init(&info)) {
      lovrFree(state.window);
      return lovrSetError("Failed to create window surface: %s", gpu_get_error());
    }

    os_on_resize(onResize);

    state.depthFormat = state.config.stencil ? FORMAT_D32FS8 : FORMAT_D32F;
    if (!lovrGraphicsGetFormatSupport(state.depthFormat, TEXTURE_FEATURE_RENDER)) {
      state.depthFormat = state.config.stencil ? FORMAT_D24S8 : FORMAT_D24;
    }
  }

  if (state.window && !state.window->gpu) {
    if (!beginFrame()) {
      return false;
    }

    if (state.resized) {
      lovrAssert(gpu_surface_resize(state.window->info.width, state.window->info.height), "Failed to resize window: %s", gpu_get_error());
      state.resized = false;
    }

    lovrAssert(gpu_surface_acquire(&state.window->gpu), "Failed to get window texture: %s", gpu_get_error());
    state.window->renderView = state.window->gpu;

    // Window texture may be unavailable during a resize
    if (!state.window->gpu) {
      *texture = NULL;
      return true;
    }
  }

  *texture = state.window;
  return true;
}

Texture* lovrTextureCreate(const TextureInfo* info) {
  uint32_t limits[] = {
    [TEXTURE_2D] = state.limits.textureSize2D,
    [TEXTURE_3D] = state.limits.textureSize3D,
    [TEXTURE_CUBE] = state.limits.textureSizeCube,
    [TEXTURE_ARRAY] = state.limits.textureSize2D
  };

  uint32_t limit = limits[info->type];
  uint32_t mipmapCap = log2(MAX(MAX(info->width, info->height), (info->type == TEXTURE_3D ? info->layers : 1))) + 1;
  uint32_t mipmaps = CLAMP(info->mipmaps, 1, mipmapCap);
  bool srgb = supportsSRGB(info->format) && info->srgb;
  uint8_t supports = state.features.formats[info->format][srgb];
  uint8_t linearSupports = state.features.formats[info->format][false];

  lovrCheck(info->width > 0, "Texture width must be greater than zero");
  lovrCheck(info->height > 0, "Texture height must be greater than zero");
  lovrCheck(info->layers > 0, "Texture layer count must be greater than zero");
  lovrCheck(info->width <= limit, "Texture %s exceeds the limit for this texture type (%d)", "width", limit);
  lovrCheck(info->height <= limit, "Texture %s exceeds the limit for this texture type (%d)", "height", limit);
  lovrCheck(info->layers <= limit || info->type != TEXTURE_3D, "Texture %s exceeds the limit for this texture type (%d)", "layer count", limit);
  lovrCheck(info->layers <= state.limits.textureLayers || info->type == TEXTURE_3D, "Texture %s exceeds the limit for this texture type (%d)", "layer count", limit);
  lovrCheck(info->layers == 1 || info->type != TEXTURE_2D, "2D textures must have a layer count of 1");
  lovrCheck(info->layers % 6 == 0 || info->type != TEXTURE_CUBE, "Cubemap layer count must be a multiple of 6");
  lovrCheck(info->width == info->height || info->type != TEXTURE_CUBE, "Cubemaps must be square");
  lovrCheck(measureTexture(info->format, info->width, info->height, info->layers) < 1 << 30, "Memory for a Texture can not exceed 1GB"); // TODO mip?
  lovrCheck(~info->usage & TEXTURE_SAMPLE || (supports & GPU_FEATURE_SAMPLE), "GPU does not support the 'sample' flag for this texture format/encoding");
  lovrCheck(~info->usage & TEXTURE_RENDER || (supports & GPU_FEATURE_RENDER), "GPU does not support the 'render' flag for this texture format/encoding");
  lovrCheck(~info->usage & TEXTURE_STORAGE || (linearSupports & GPU_FEATURE_STORAGE), "GPU does not support the 'storage' flag for this texture format");
  lovrCheck(~info->usage & TEXTURE_RENDER || info->width <= state.limits.renderSize[0], "Texture has 'render' flag but its size exceeds the renderSize limit");
  lovrCheck(~info->usage & TEXTURE_RENDER || info->height <= state.limits.renderSize[1], "Texture has 'render' flag but its size exceeds the renderSize limit");
  lovrCheck(~info->usage & TEXTURE_RENDER || info->type != TEXTURE_3D || !isDepthFormat(info->format), "3D depth textures can not have the 'render' flag");
  lovrCheck((info->format < FORMAT_BC1 || info->format > FORMAT_BC7) || state.features.textureBC, "%s textures are not supported on this GPU", "BC");
  lovrCheck(info->format < FORMAT_ASTC_4x4 || state.features.textureASTC, "%s textures are not supported on this GPU", "ASTC");

  Texture* texture = lovrCalloc(sizeof(Texture) + gpu_sizeof_texture());
  texture->ref = 1;
  texture->gpu = (gpu_texture*) (texture + 1);
  texture->root = texture;
  texture->info = *info;
  texture->info.mipmaps = mipmaps;
  texture->info.srgb = srgb;

  if (info->label) {
    size_t size = strlen(info->label) + 1;
    char* label = lovrMalloc(size);
    memcpy(label, info->label, size);
    texture->info.label = label;
  }

  uint32_t levelCount = 0;
  uint32_t levelOffsets[16];
  uint32_t levelSizes[16];
  BufferView view = { 0 };

  if (!beginFrame()) {
    lovrTextureDestroy(texture);
    return NULL;
  }

  if (info->imageCount > 0) {
    levelCount = lovrImageGetLevelCount(info->images[0]);

    if (info->type == TEXTURE_3D && levelCount > 1) {
      lovrSetError("Images used to initialize 3D textures can not have mipmaps");
      lovrTextureDestroy(texture);
      return NULL;
    }

    uint32_t total = 0;
    for (uint32_t level = 0; level < levelCount; level++) {
      levelOffsets[level] = total;
      uint32_t width = MAX(info->width >> level, 1);
      uint32_t height = MAX(info->height >> level, 1);
      levelSizes[level] = measureTexture(info->format, width, height, info->layers);
      total += levelSizes[level];
    }

    view = getBuffer(GPU_BUFFER_UPLOAD, total, 64);
    char* data = view.pointer;

    if (!view.buffer) {
      lovrTextureDestroy(texture);
      return NULL;
    }

    for (uint32_t level = 0; level < levelCount; level++) {
      for (uint32_t layer = 0; layer < info->layers; layer++) {
        Image* image = info->imageCount == 1 ? info->images[0] : info->images[layer];
        uint32_t slice = info->imageCount == 1 ? layer : 0;
        size_t size = lovrImageGetLayerSize(image, level);
        if (size != levelSizes[level] / info->layers) lovrUnreachable();
        void* pixels = lovrImageGetLayerData(image, level, slice);
        memcpy(data, pixels, size);
        data += size;
      }
      levelOffsets[level] += view.offset;
    }
  }

  // Render targets with mipmaps get transfer usage for automipmapping
  bool transfer = (info->usage & TEXTURE_TRANSFER) || ((info->usage & TEXTURE_RENDER) && texture->info.mipmaps > 1);

  if (!gpu_texture_init(texture->gpu, &(gpu_texture_info) {
    .type = (gpu_texture_type) info->type,
    .format = (gpu_texture_format) info->format,
    .size = { info->width, info->height, info->layers },
    .mipmaps = texture->info.mipmaps,
    .usage =
      ((info->usage & TEXTURE_SAMPLE) ? GPU_TEXTURE_SAMPLE : 0) |
      ((info->usage & TEXTURE_RENDER) ? GPU_TEXTURE_RENDER : 0) |
      ((info->usage & TEXTURE_STORAGE) ? GPU_TEXTURE_STORAGE : 0) |
      (transfer ? GPU_TEXTURE_COPY_SRC | GPU_TEXTURE_COPY_DST : 0),
    .srgb = srgb,
    .handle = info->handle,
    .label = info->label,
    .upload = {
      .stream = state.stream,
      .buffer = view.buffer,
      .levelCount = levelCount,
      .levelOffsets = levelOffsets,
      .generateMipmaps = levelCount > 0 && levelCount < mipmaps
    }
  })) {
    lovrSetError("Failed to create texture: %s", gpu_get_error());
    lovrTextureDestroy(texture);
    return NULL;
  }

  // Depth-stencil textures use a different depth-only view for sampling, otherwise default view can be used
  if (info->usage & TEXTURE_SAMPLE) {
    if (info->format == FORMAT_D24S8 || info->format == FORMAT_D32FS8) {
      gpu_texture_view_info view = {
        .source = texture->gpu,
        .type = (gpu_texture_type) info->type,
        .usage = GPU_TEXTURE_SAMPLE,
        .srgb = srgb,
        .aspect = GPU_ASPECT_DEPTH
      };

      texture->sampleView = lovrMalloc(gpu_sizeof_texture());
      if (!gpu_texture_init_view(texture->sampleView, &view)) {
        lovrSetError("Failed to create sample texture view: %s", gpu_get_error());
        lovrTextureDestroy(texture);
        return NULL;
      }
    } else {
      texture->sampleView = texture->gpu;
    }
  }

  // Automatically create a renderable view for renderable non-volume textures
  if ((info->usage & TEXTURE_RENDER) && info->type != TEXTURE_3D && info->layers <= state.limits.renderSize[2]) {
    if (info->mipmaps == 1) {
      texture->renderView = texture->gpu;
    } else {
      gpu_texture_view_info view = {
        .source = texture->gpu,
        .type = GPU_TEXTURE_ARRAY,
        .usage = GPU_TEXTURE_RENDER,
        .srgb = srgb,
        .layerCount = info->layers,
        .levelCount = 1
      };

      texture->renderView = lovrMalloc(gpu_sizeof_texture());
      if (!gpu_texture_init_view(texture->renderView, &view)) {
        lovrSetError("Failed to create render texture view: %s", gpu_get_error());
        lovrTextureDestroy(texture);
        return NULL;
      }
    }
  }

  // Make a linear view of sRGB textures for storage bindings
  if (srgb && (info->usage & TEXTURE_STORAGE)) {
    gpu_texture_view_info view = {
      .source = texture->gpu,
      .type = (gpu_texture_type) info->type,
      .usage = GPU_TEXTURE_STORAGE,
      .srgb = false
    };

    texture->storageView = lovrMalloc(gpu_sizeof_texture());
    if (!gpu_texture_init_view(texture->storageView, &view)) {
      lovrSetError("Failed to create storage texture view: %s", gpu_get_error());
      lovrTextureDestroy(texture);
      return NULL;
    }
  } else {
    texture->storageView = texture->gpu;
  }

  // Sample-only textures are exempt from sync tracking to reduce overhead.  Instead, they are
  // manually synchronized with a single barrier after the upload stream.
  if (info->usage == TEXTURE_SAMPLE) {
    state.barrier.prev |= GPU_PHASE_COPY | GPU_PHASE_BLIT;
    state.barrier.next |= GPU_PHASE_SHADER_VERTEX | GPU_PHASE_SHADER_FRAGMENT | GPU_PHASE_SHADER_COMPUTE;
    state.barrier.flush |= GPU_CACHE_TRANSFER_WRITE;
    state.barrier.clear |= GPU_CACHE_TEXTURE;
  } else if (levelCount > 0) {
    texture->sync.writePhase = GPU_PHASE_COPY | GPU_PHASE_BLIT;
    texture->sync.pendingWrite = GPU_CACHE_TRANSFER_WRITE;
    texture->sync.lastTransferWrite = state.tick;
  }

  texture->sync.barrier = &state.barrier;
  return texture;
}

Texture* lovrTextureCreateView(Texture* parent, const TextureViewInfo* info) {
  const TextureInfo* base = &parent->info;
  uint32_t maxLayers = base->type == TEXTURE_3D ? MAX(base->layers >> info->levelIndex, 1) : base->layers;
  uint32_t layers = info->layerCount == ~0u ? base->layers - info->layerIndex : info->layerCount;
  uint32_t levels = info->levelCount == ~0u ? base->mipmaps - info->levelIndex : info->levelCount;

  lovrCheck(info->type != TEXTURE_3D, "Texture views can't be 3D textures");
  lovrCheck(layers > 0, "Texture view must have at least one layer");
  lovrCheck(levels > 0, "Texture view must have at least one mipmap");
  lovrCheck(info->layerIndex + layers <= maxLayers, "Texture view layer range exceeds layer count of parent texture");
  lovrCheck(info->levelIndex + levels <= base->mipmaps, "Texture view mipmap range exceeds mipmap count of parent texture");
  lovrCheck(layers == 1 || info->type != TEXTURE_2D, "2D textures can only have a single layer");
  lovrCheck(levels == 1 || base->type != TEXTURE_3D, "Views of 3D textures can only have a single mipmap level");
  lovrCheck(layers % 6 == 0 || info->type != TEXTURE_CUBE, "Cubemap layer count must be a multiple of 6");

  Texture* texture = lovrCalloc(sizeof(Texture) + gpu_sizeof_texture());
  texture->ref = 1;
  texture->gpu = (gpu_texture*) (texture + 1);
  texture->info = *base;

  if (info->label) {
    size_t size = strlen(info->label) + 1;
    char* label = lovrMalloc(size);
    memcpy(label, info->label, size);
    texture->info.label = label;
  }

  texture->root = parent->root;
  texture->baseLayer = parent->baseLayer + info->layerIndex;
  texture->baseLevel = parent->baseLevel + info->levelIndex;
  texture->info.type = info->type;
  texture->info.width = MAX(base->width >> info->levelIndex, 1);
  texture->info.height = MAX(base->height >> info->levelIndex, 1);
  texture->info.layers = layers;
  texture->info.mipmaps = levels;
  texture->sampler = parent->sampler;
  lovrRetain(texture->sampler);
  lovrRetain(texture->root);

  if (base->usage & (TEXTURE_SAMPLE | TEXTURE_RENDER)) {
    if (!gpu_texture_init_view(texture->gpu, &(gpu_texture_view_info) {
      .source = texture->root->gpu,
      .type = (gpu_texture_type) info->type,
      .usage = base->usage,
      .srgb = base->srgb,
      .layerIndex = texture->baseLayer,
      .layerCount = layers,
      .levelIndex = texture->baseLevel,
      .levelCount = levels,
      .label = info->label
    })) {
      lovrSetError("Failed to create texture view: %s", gpu_get_error());
      lovrTextureDestroy(texture);
      return NULL;
    }
  } else {
    texture->gpu = NULL;
  }

  // Depth-stencil textures use a different depth-only view for sampling, otherwise default view can be used
  if (base->usage & TEXTURE_SAMPLE) {
    if (base->format == FORMAT_D24S8 || base->format == FORMAT_D32FS8) {
      gpu_texture_view_info subview = {
        .source = texture->root->gpu,
        .type = (gpu_texture_type) base->type,
        .usage = GPU_TEXTURE_SAMPLE,
        .aspect = GPU_ASPECT_DEPTH,
        .layerIndex = texture->baseLayer,
        .layerCount = layers,
        .levelIndex = texture->baseLevel,
        .levelCount = levels
      };

      texture->sampleView = lovrMalloc(gpu_sizeof_texture());
      if (!gpu_texture_init_view(texture->sampleView, &subview)) {
        lovrSetError("Failed to create sample texture view: %s", gpu_get_error());
        lovrTextureDestroy(texture);
        return NULL;
      }
    } else {
      texture->sampleView = texture->gpu;
    }
  }

  if ((base->usage & TEXTURE_RENDER) && layers <= state.limits.renderSize[2]) {
    if (levels == 1) {
      texture->renderView = texture->gpu;
    } else {
      gpu_texture_view_info subview = {
        .source = texture->root->gpu,
        .type = GPU_TEXTURE_ARRAY,
        .usage = GPU_TEXTURE_RENDER,
        .layerIndex = texture->baseLayer,
        .layerCount = layers,
        .levelIndex = texture->baseLevel,
        .levelCount = 1
      };

      texture->renderView = lovrMalloc(gpu_sizeof_texture());
      if (!gpu_texture_init_view(texture->renderView, &subview)) {
        lovrSetError("Failed to create render texture view: %s", gpu_get_error());
        lovrTextureDestroy(texture);
        return NULL;
      }
    }
  }

  if ((base->usage & TEXTURE_STORAGE) && base->srgb) {
    gpu_texture_view_info subview = {
      .source = texture->root->gpu,
      .type = (gpu_texture_type) base->type,
      .usage = GPU_TEXTURE_STORAGE,
      .srgb = false,
      .layerIndex = texture->baseLayer,
      .layerCount = layers,
      .levelIndex = texture->baseLevel,
      .levelCount = levels
    };

    texture->storageView = lovrMalloc(gpu_sizeof_texture());
    if (!gpu_texture_init_view(texture->storageView, &subview)) {
      lovrSetError("Failed to create storage texture view: %s", gpu_get_error());
      lovrTextureDestroy(texture);
      return NULL;
    }
  } else {
    texture->storageView = texture->gpu;
  }

  return texture;
}

void lovrTextureDestroy(void* ref) {
  Texture* texture = ref;
  if (texture != state.window) {
    if (texture->root == texture || texture->info.label != texture->root->info.label) {
      lovrFree((char*) texture->info.label);
    }
    flushTransfers();
    lovrRelease(texture->sampler, lovrSamplerDestroy);
    lovrRelease(texture->material, lovrMaterialDestroy);
    if (texture->root != texture) lovrRelease(texture->root, lovrTextureDestroy);
    if (texture->sampleView && texture->sampleView != texture->gpu) gpu_texture_destroy(texture->sampleView), lovrFree(texture->sampleView);
    if (texture->renderView && texture->renderView != texture->gpu) gpu_texture_destroy(texture->renderView), lovrFree(texture->renderView);
    if (texture->storageView && texture->storageView != texture->gpu) gpu_texture_destroy(texture->storageView), lovrFree(texture->storageView);
    if (texture->gpu) gpu_texture_destroy(texture->gpu);
  }
  lovrFree(texture);
}

const TextureInfo* lovrTextureGetInfo(Texture* texture) {
  return &texture->info;
}

Image* lovrTextureGetPixels(Texture* texture, uint32_t offset[4], uint32_t extent[3]) {
  if (!beginFrame()) return NULL;

  if (extent[0] == ~0u) extent[0] = texture->info.width - offset[0];
  if (extent[1] == ~0u) extent[1] = texture->info.height - offset[1];
  lovrCheck(extent[2] == 1, "Currently only a single layer can be read from a Texture");
  lovrCheck(texture->info.usage & TEXTURE_TRANSFER, "Texture must be created with the 'transfer' usage to read from it");
  if (!checkTextureBounds(&texture->info, offset, extent)) return NULL;

  gpu_barrier barrier = syncTransfer(&texture->sync, GPU_PHASE_COPY, GPU_CACHE_TRANSFER_READ);
  gpu_sync(state.stream, &barrier, 1);

  uint32_t rootOffset[4] = { offset[0], offset[1], offset[2] + texture->baseLayer, offset[3] + texture->baseLevel };

  BufferView view = getBuffer(GPU_BUFFER_DOWNLOAD, measureTexture(texture->info.format, extent[0], extent[1], 1), 64);
  if (!view.buffer) return NULL;

  gpu_copy_texture_buffer(state.stream, texture->root->gpu, view.buffer, rootOffset, view.offset, extent);

  if (!lovrGraphicsSubmit(NULL, 0) || !lovrGraphicsWait()) {
    return NULL;
  }

  Image* image = lovrImageCreateRaw(extent[0], extent[1], texture->info.format, texture->info.srgb);

  if (image) {
    void* data = lovrImageGetLayerData(image, offset[3], offset[2]);
    memcpy(data, view.pointer, view.extent);
  }

  return image;
}

bool lovrTextureSetPixels(Texture* texture, Image* image, uint32_t srcOffset[4], uint32_t dstOffset[4], uint32_t extent[3]) {
  if (!beginFrame()) return false;

  TextureFormat format = texture->info.format;
  if (extent[0] == ~0u) extent[0] = MIN(texture->info.width - dstOffset[0], lovrImageGetWidth(image, srcOffset[3]) - srcOffset[0]);
  if (extent[1] == ~0u) extent[1] = MIN(texture->info.height - dstOffset[1], lovrImageGetHeight(image, srcOffset[3]) - srcOffset[1]);
  if (extent[2] == ~0u) extent[2] = MIN(texture->info.layers - dstOffset[2], lovrImageGetLayerCount(image) - srcOffset[2]);
  lovrCheck(texture->info.usage & TEXTURE_TRANSFER, "Texture must be created with the 'transfer' usage to copy to it");
  lovrCheck(lovrImageGetFormat(image) == format, "Image and Texture formats must match");
  lovrCheck(srcOffset[0] + extent[0] <= lovrImageGetWidth(image, srcOffset[3]), "Image copy region exceeds its %s", "width");
  lovrCheck(srcOffset[1] + extent[1] <= lovrImageGetHeight(image, srcOffset[3]), "Image copy region exceeds its %s", "height");
  lovrCheck(srcOffset[2] + extent[2] <= lovrImageGetLayerCount(image), "Image copy region exceeds its %s", "layer count");
  lovrCheck(srcOffset[3] < lovrImageGetLevelCount(image), "Image copy region exceeds its %s", "mipmap count");
  if (!checkTextureBounds(&texture->info, dstOffset, extent)) return false;

  uint32_t rowSize = measureTexture(format, extent[0], 1, 1);
  uint32_t totalSize = measureTexture(format, extent[0], extent[1], 1) * extent[2];
  uint32_t layerOffset = measureTexture(format, extent[0], srcOffset[1], 1);
  layerOffset += measureTexture(format, srcOffset[0], 1, 1);
  uint32_t pitch = measureTexture(format, lovrImageGetWidth(image, srcOffset[3]), 1, 1);
  BufferView view = getBuffer(GPU_BUFFER_UPLOAD, totalSize, 64);
  if (!view.buffer) return false;

  char* dst = view.pointer;
  for (uint32_t z = 0; z < extent[2]; z++) {
    const char* src = (char*) lovrImageGetLayerData(image, srcOffset[3], z) + layerOffset;
    for (uint32_t y = 0; y < extent[1]; y++) {
      memcpy(dst, src, rowSize);
      dst += rowSize;
      src += pitch;
    }
  }

  gpu_barrier barrier = syncTransfer(&texture->root->sync, GPU_PHASE_COPY, GPU_CACHE_TRANSFER_WRITE);
  gpu_sync(state.stream, &barrier, 1);

  uint32_t rootOffset[4] = { dstOffset[0], dstOffset[1], dstOffset[2] + texture->baseLayer, dstOffset[3] + texture->baseLevel };
  gpu_copy_buffer_texture(state.stream, view.buffer, texture->root->gpu, view.offset, rootOffset, extent);
  return true;
}

bool lovrTextureCopy(Texture* src, Texture* dst, uint32_t srcOffset[4], uint32_t dstOffset[4], uint32_t extent[3]) {
  if (!beginFrame()) return false;

  if (extent[0] == ~0u) extent[0] = MIN(src->info.width - srcOffset[0], dst->info.width - dstOffset[0]);
  if (extent[1] == ~0u) extent[1] = MIN(src->info.height - srcOffset[1], dst->info.height - dstOffset[0]);
  if (extent[2] == ~0u) extent[2] = MIN(src->info.layers - srcOffset[2], dst->info.layers - dstOffset[0]);
  lovrCheck(src->info.usage & TEXTURE_TRANSFER, "Texture must be created with the 'transfer' usage to copy %s it", "from");
  lovrCheck(dst->info.usage & TEXTURE_TRANSFER, "Texture must be created with the 'transfer' usage to copy %s it", "to");
  lovrCheck(src->info.format == dst->info.format, "Copying between Textures requires them to have the same format");
  if (!checkTextureBounds(&src->info, srcOffset, extent)) return false;
  if (!checkTextureBounds(&dst->info, dstOffset, extent)) return false;

  gpu_barrier barriers[2];
  barriers[0] = syncTransfer(&src->root->sync, GPU_PHASE_COPY, GPU_CACHE_TRANSFER_READ);
  barriers[1] = syncTransfer(&dst->root->sync, GPU_PHASE_COPY, GPU_CACHE_TRANSFER_WRITE);
  gpu_sync(state.stream, barriers, 2);

  uint32_t srcRootOffset[4] = { srcOffset[0], srcOffset[1], srcOffset[2] + src->baseLayer, srcOffset[3] + src->baseLevel };
  uint32_t dstRootOffset[4] = { dstOffset[0], dstOffset[1], dstOffset[2] + dst->baseLayer, dstOffset[3] + dst->baseLevel };
  gpu_copy_textures(state.stream, src->root->gpu, dst->root->gpu, srcRootOffset, dstRootOffset, extent);
  return true;
}

bool lovrTextureBlit(Texture* src, Texture* dst, uint32_t srcOffset[4], uint32_t dstOffset[4], uint32_t srcExtent[3], uint32_t dstExtent[3], FilterMode filter) {
  if (!beginFrame()) return false;

  if (srcExtent[0] == ~0u) srcExtent[0] = src->info.width - srcOffset[0];
  if (srcExtent[1] == ~0u) srcExtent[1] = src->info.height - srcOffset[1];
  if (srcExtent[2] == ~0u) srcExtent[2] = src->info.layers - srcOffset[2];
  if (dstExtent[0] == ~0u) dstExtent[0] = dst->info.width - dstOffset[0];
  if (dstExtent[1] == ~0u) dstExtent[1] = dst->info.height - dstOffset[1];
  if (dstExtent[2] == ~0u) dstExtent[2] = dst->info.layers - dstOffset[2];
  uint32_t supports = state.features.formats[src->info.format][src->info.srgb];
  lovrCheck(src->info.usage & TEXTURE_TRANSFER, "Texture must be created with the 'transfer' usage to blit %s it", "from");
  lovrCheck(dst->info.usage & TEXTURE_TRANSFER, "Texture must be created with the 'transfer' usage to blit %s it", "to");
  lovrCheck(supports & GPU_FEATURE_BLIT, "This GPU does not support blitting this texture format/encoding");
  lovrCheck(src->info.format == dst->info.format && src->info.srgb == dst->info.srgb, "Texture formats must match to blit between them");
  lovrCheck(((src->info.type == TEXTURE_3D) ^ (dst->info.type == TEXTURE_3D)) == false, "3D textures can only be blitted with other 3D textures");
  lovrCheck(src->info.type == TEXTURE_3D || srcExtent[2] == dstExtent[2], "When blitting between non-3D textures, blit layer counts must match");
  if (!checkTextureBounds(&src->info, srcOffset, srcExtent)) return false;
  if (!checkTextureBounds(&dst->info, dstOffset, dstExtent)) return false;

  gpu_barrier barriers[2];
  barriers[0] = syncTransfer(&src->root->sync, GPU_PHASE_BLIT, GPU_CACHE_TRANSFER_READ);
  barriers[1] = syncTransfer(&dst->root->sync, GPU_PHASE_BLIT, GPU_CACHE_TRANSFER_WRITE);
  gpu_sync(state.stream, barriers, 2);

  uint32_t srcRootOffset[4] = { srcOffset[0], srcOffset[1], srcOffset[2] + src->baseLayer, srcOffset[3] + src->baseLevel };
  uint32_t dstRootOffset[4] = { dstOffset[0], dstOffset[1], dstOffset[2] + dst->baseLayer, dstOffset[3] + dst->baseLevel };
  gpu_blit(state.stream, src->root->gpu, dst->root->gpu, srcRootOffset, dstRootOffset, srcExtent, dstExtent, (gpu_filter) filter);
  return true;
}

bool lovrTextureClear(Texture* texture, float value[4], uint32_t layer, uint32_t layerCount, uint32_t level, uint32_t levelCount) {
  if (!beginFrame()) return false;

  if (layerCount == ~0u) layerCount = texture->info.layers - layer;
  if (levelCount == ~0u) levelCount = texture->info.mipmaps - level;
  lovrCheck(texture->info.usage & TEXTURE_TRANSFER, "Texture must be created with 'transfer' usage to clear it");
  lovrCheck(texture->info.type == TEXTURE_3D || layer + layerCount <= texture->info.layers, "Texture clear range exceeds texture layer count");
  lovrCheck(level + levelCount <= texture->info.mipmaps, "Texture clear range exceeds texture mipmap count");

  gpu_barrier barrier = syncTransfer(&texture->root->sync, GPU_PHASE_CLEAR, GPU_CACHE_TRANSFER_WRITE);
  gpu_sync(state.stream, &barrier, 1);

  gpu_clear_texture(state.stream, texture->root->gpu, value, texture->baseLayer + layer, layerCount, texture->baseLevel + level, levelCount);
  return true;
}

bool lovrTextureGenerateMipmaps(Texture* texture, uint32_t base, uint32_t count) {
  if (!beginFrame()) return false;

  if (count == ~0u) count = texture->info.mipmaps - (base + 1);
  uint32_t supports = state.features.formats[texture->info.format][texture->info.srgb];
  lovrCheck(texture->info.usage & TEXTURE_TRANSFER, "Texture must be created with the 'transfer' usage to mipmap it");
  lovrCheck(supports & GPU_FEATURE_BLIT, "This GPU does not support mipmapping this texture format/encoding");
  lovrCheck(base + count < texture->info.mipmaps, "Trying to generate too many mipmaps");

  gpu_barrier barrier = syncTransfer(&texture->root->sync, GPU_PHASE_BLIT, GPU_CACHE_TRANSFER_READ | GPU_CACHE_TRANSFER_WRITE);
  gpu_sync(state.stream, &barrier, 1);

  mipmapTexture(state.stream, texture, texture->baseLevel + base, count);
  return true;
}

Sampler* lovrTextureGetSampler(Texture* texture) {
  return texture->sampler;
}

void lovrTextureSetSampler(Texture* texture, Sampler* sampler) {
  lovrRelease(texture->sampler, lovrSamplerDestroy);
  texture->sampler = sampler;
  lovrRetain(sampler);
}

Material* lovrTextureToMaterial(Texture* texture) {
  if (!texture->material) {
    texture->material = lovrMaterialCreate(&(MaterialInfo) {
      .data.color = { 1.f, 1.f, 1.f, 1.f },
      .data.uvScale = { 1.f, 1.f },
      .texture = texture
    });

    if (!texture->material) {
      return NULL;
    }

    // Since the Material refcounts the Texture, this creates a cycle.  Release the texture to make
    // sure this is a weak relationship (the automaterial does not keep the texture refcounted).
    lovrRelease(texture, lovrTextureDestroy);
    texture->material->info.texture = NULL;
  }

  return texture->material;
}

// Sampler

Sampler* lovrGraphicsGetDefaultSampler(FilterMode mode) {
  return state.defaultSamplers[mode];
}

Sampler* lovrSamplerCreate(const SamplerInfo* info) {
  lovrCheck(info->range[1] < 0.f || info->range[1] >= info->range[0], "Invalid Sampler mipmap range");
  lovrCheck(info->anisotropy <= state.limits.anisotropy, "Sampler anisotropy (%f) exceeds anisotropy limit (%f)", info->anisotropy, state.limits.anisotropy);

  Sampler* sampler = lovrCalloc(sizeof(Sampler) + gpu_sizeof_sampler());
  sampler->ref = 1;
  sampler->gpu = (gpu_sampler*) (sampler + 1);
  sampler->info = *info;

  gpu_sampler_info gpu = {
    .min = (gpu_filter) info->min,
    .mag = (gpu_filter) info->mag,
    .mip = (gpu_filter) info->mip,
    .wrap[0] = (gpu_wrap) info->wrap[0],
    .wrap[1] = (gpu_wrap) info->wrap[1],
    .wrap[2] = (gpu_wrap) info->wrap[2],
    .compare = (gpu_compare_mode) info->compare,
    .anisotropy = MIN(info->anisotropy, state.limits.anisotropy),
    .lodClamp = { info->range[0], info->range[1] }
  };

  if (!gpu_sampler_init(sampler->gpu, &gpu)) {
    lovrSetError("Failed to create sampler: %s", gpu_get_error());
    lovrFree(sampler);
    return NULL;
  }

  return sampler;
}

void lovrSamplerDestroy(void* ref) {
  Sampler* sampler = ref;
  gpu_sampler_destroy(sampler->gpu);
  lovrFree(sampler);
}

const SamplerInfo* lovrSamplerGetInfo(Sampler* sampler) {
  return &sampler->info;
}

// Shader

#ifdef LOVR_USE_GLSLANG
static glsl_include_result_t* includer(void* cb, const char* path, const char* includer, size_t depth) {
  if (!strcmp(path, includer)) {
    return NULL;
  }
  glsl_include_result_t* result = tempAlloc(&state.allocator, sizeof(*result));
  result->header_name = path;
  result->header_data = ((ShaderIncluder*) cb)(path, &result->header_length);
  if (!result->header_data) return NULL;
  return result;
}
#endif

bool lovrGraphicsCompileShader(ShaderSource* stages, ShaderSource* outputs, uint32_t stageCount, ShaderIncluder* io, bool raw) {
#ifdef LOVR_USE_GLSLANG
  const glslang_stage_t stageMap[] = {
    [STAGE_VERTEX] = GLSLANG_STAGE_VERTEX,
    [STAGE_FRAGMENT] = GLSLANG_STAGE_FRAGMENT,
    [STAGE_COMPUTE] = GLSLANG_STAGE_COMPUTE
  };

  const char* stageNames[] = {
    [STAGE_VERTEX] = "vertex",
    [STAGE_FRAGMENT] = "fragment",
    [STAGE_COMPUTE] = "compute"
  };

  const char* prefix = ""
    "#version 460\n"
    "#extension GL_EXT_multiview : require\n"
    "#extension GL_EXT_samplerless_texture_functions : require\n"
    "#extension GL_GOOGLE_include_directive : require\n";

  glslang_program_t* program = NULL;
  glslang_shader_t* shaders[2] = { 0 };

  if (stageCount > COUNTOF(shaders)) {
    lovrUnreachable();
  }

  for (uint32_t i = 0; i < stageCount; i++) {
    ShaderSource* source = &stages[i];

    // It's okay to pass precompiled SPIR-V here, and it will be returned unchanged.  However, it's
    // dangerous to mix SPIR-V and GLSL because then glslang won't perform cross-stage linking,
    // which means that e.g. the default uniform block might be different for each stage.  This
    // isn't a problem when using the default shaders since they don't use uniforms.
    uint32_t magic = 0x07230203;
    if (source->size % 4 == 0 && source->size >= 4 && !memcmp(source->code, &magic, 4)) {
      outputs[i] = stages[i];
      continue;
    } else if (!program) {
      program = glslang_program_create();
    }

    const char* strings[] = {
      prefix,
      (const char*) etc_shaders_lovr_glsl,
      "#line 1\n",
      source->code
    };

    size_t lengths[] = {
      strlen(strings[0]),
      etc_shaders_lovr_glsl_len,
      strlen(strings[2]),
      source->size
    };

    char* code = NULL;

    if (raw) {
      code = tempAlloc(&state.allocator, source->size + 1);
      memcpy(code, source->code, source->size);
      code[source->size] = '\0';
    } else {
      size_t totalLength = 0;
      for (size_t i = 0; i < COUNTOF(strings); i++) {
        totalLength += lengths[i];
      }

      size_t cursor = 0;
      code = tempAlloc(&state.allocator, totalLength + 1);
      for (size_t i = 0; i < COUNTOF(strings); i++) {
        memcpy(code + cursor, strings[i], lengths[i]);
        cursor += lengths[i];
      }
      code[cursor] = '\0';
    }

    const glslang_resource_t* resource = glslang_default_resource();

    glslang_input_t input = {
      .language = GLSLANG_SOURCE_GLSL,
      .stage = stageMap[source->stage],
      .client = GLSLANG_CLIENT_VULKAN,
      .client_version = GLSLANG_TARGET_VULKAN_1_1,
      .target_language = GLSLANG_TARGET_SPV,
      .target_language_version = GLSLANG_TARGET_SPV_1_3,
      .code = code,
      .default_version = 460,
      .default_profile = GLSLANG_NO_PROFILE,
      .forward_compatible = true,
      .resource = resource,
      .callbacks.include_local = includer,
      .callbacks_ctx = (void*) io
    };

    shaders[i] = glslang_shader_create(&input);

    int options = 0;
    options |= GLSLANG_SHADER_AUTO_MAP_BINDINGS;
    options |= GLSLANG_SHADER_AUTO_MAP_LOCATIONS;
    options |= GLSLANG_SHADER_VULKAN_RULES_RELAXED;

    glslang_shader_set_options(shaders[i], options);

    if (!glslang_shader_preprocess(shaders[i], &input)) {
      lovrSetError("Could not preprocess %s shader:\n%s", stageNames[source->stage], glslang_shader_get_info_log(shaders[i]));
      glslang_shader_delete(shaders[i]);
      glslang_program_delete(program);
      return false;
    }

    if (!glslang_shader_parse(shaders[i], &input)) {
      lovrSetError("Could not parse %s shader:\n%s", stageNames[source->stage], glslang_shader_get_info_log(shaders[i]));
      glslang_shader_delete(shaders[i]);
      glslang_program_delete(program);
      return false;
    }

    glslang_program_add_shader(program, shaders[i]);
  }

  // We might not need to do anything if all the inputs were already SPIR-V
  if (!program) {
    return true;
  }

  if (!glslang_program_link(program, 0)) {
    lovrSetError("Could not link shader:\n%s", glslang_program_get_info_log(program));
    glslang_program_delete(program);
    return false;
  }

  if (!glslang_program_map_io(program)) {
    lovrSetError("Could not map shader IO:\n%s", glslang_program_get_info_log(program));
    glslang_program_delete(program);
    return false;
  }

  glslang_spv_options_t spvOptions = { 0 };

  if (state.config.debug && state.features.shaderDebug) {
    spvOptions.generate_debug_info = true;
    spvOptions.emit_nonsemantic_shader_debug_info = true;
    spvOptions.emit_nonsemantic_shader_debug_source = true;
  }

  for (uint32_t i = 0; i < stageCount; i++) {
    if (!shaders[i]) continue;

    ShaderSource* source = &stages[i];

    if (state.config.debug && state.features.shaderDebug) {
      glslang_program_add_source_text(program, stageMap[source->stage], source->code, source->size);
    }

    glslang_program_SPIRV_generate_with_options(program, stageMap[source->stage], &spvOptions);

    void* words = glslang_program_SPIRV_get_ptr(program);
    size_t size = glslang_program_SPIRV_get_size(program) * 4;

    void* data = lovrMalloc(size);
    memcpy(data, words, size);

    outputs[i].stage = source->stage;
    outputs[i].code = data;
    outputs[i].size = size;

    glslang_shader_delete(shaders[i]);
  }

  glslang_program_delete(program);
  return true;
#else
  return lovrSetError("Could not compile shader: No shader compiler available");
#endif
}

static bool lovrShaderInit(Shader* shader) {

  // Shaders store the full list of their flags so clones can override them, but they are reordered
  // to put overridden (active) ones first, so a contiguous list can be used to create pipelines
  for (uint32_t i = 0; i < shader->info.flagCount; i++) {
    ShaderFlag* flag = &shader->info.flags[i];
    uint32_t hash = flag->name ? (uint32_t) hash64(flag->name, strlen(flag->name)) : 0;

    for (uint32_t j = 0; j < shader->flagCount; j++) {
      if (hash ? (hash != shader->flagLookup[j]) : (flag->id != shader->flags[j].id)) continue;

      uint32_t index = shader->overrideCount++;

      if (index != j) {
        gpu_shader_flag temp = shader->flags[index];
        shader->flags[index] = shader->flags[j];
        shader->flags[j] = temp;

        uint32_t tempHash = shader->flagLookup[index];
        shader->flagLookup[index] = shader->flagLookup[j];
        shader->flagLookup[j] = tempHash;
      }

      shader->flags[index].value = flag->value;
    }
  }

  if (shader->info.type == SHADER_COMPUTE) {
    gpu_compute_pipeline_info pipelineInfo = {
      .shader = shader->gpu,
      .flags = shader->flags,
      .flagCount = shader->overrideCount
    };

    lovrAssert(state.pipelineCount < MAX_PIPELINES, "Too many pipelines!");
    shader->computePipeline = getPipeline(state.pipelineCount++);
    lovrAssert(os_vm_commit(state.pipelines, state.pipelineCount * gpu_sizeof_pipeline()), "Out of pipeline memory");
    lovrAssert(gpu_pipeline_init_compute(shader->computePipeline, &pipelineInfo), "Failed to create compute shader pipeline: %s", gpu_get_error());
  }

  return true;
}

ShaderSource lovrGraphicsGetDefaultShaderSource(DefaultShader type, ShaderStage stage) {
  const ShaderSource sources[][3] = {
    [SHADER_UNLIT] = {
      [STAGE_VERTEX] = { STAGE_VERTEX, lovr_shader_unlit_vert, sizeof(lovr_shader_unlit_vert) },
      [STAGE_FRAGMENT] = { STAGE_FRAGMENT, lovr_shader_unlit_frag, sizeof(lovr_shader_unlit_frag) }
    },
    [SHADER_NORMAL] = {
      [STAGE_VERTEX] = { STAGE_VERTEX, lovr_shader_unlit_vert, sizeof(lovr_shader_unlit_vert) },
      [STAGE_FRAGMENT] = { STAGE_FRAGMENT, lovr_shader_normal_frag, sizeof(lovr_shader_normal_frag) }
    },
    [SHADER_FONT] = {
      [STAGE_VERTEX] = { STAGE_VERTEX, lovr_shader_unlit_vert, sizeof(lovr_shader_unlit_vert) },
      [STAGE_FRAGMENT] = { STAGE_FRAGMENT, lovr_shader_font_frag, sizeof(lovr_shader_font_frag) }
    },
    [SHADER_CUBEMAP] = {
      [STAGE_VERTEX] = { STAGE_VERTEX, lovr_shader_cubemap_vert, sizeof(lovr_shader_cubemap_vert) },
      [STAGE_FRAGMENT] = { STAGE_FRAGMENT, lovr_shader_cubemap_frag, sizeof(lovr_shader_cubemap_frag) }
    },
    [SHADER_EQUIRECT] = {
      [STAGE_VERTEX] = { STAGE_VERTEX, lovr_shader_cubemap_vert, sizeof(lovr_shader_cubemap_vert) },
      [STAGE_FRAGMENT] = { STAGE_FRAGMENT, lovr_shader_equirect_frag, sizeof(lovr_shader_equirect_frag) }
    },
    [SHADER_FILL_2D] = {
      [STAGE_VERTEX] = { STAGE_VERTEX, lovr_shader_fill_vert, sizeof(lovr_shader_fill_vert) },
      [STAGE_FRAGMENT] = { STAGE_FRAGMENT, lovr_shader_unlit_frag, sizeof(lovr_shader_unlit_frag) }
    },
    [SHADER_FILL_ARRAY] = {
      [STAGE_VERTEX] = { STAGE_VERTEX, lovr_shader_fill_vert, sizeof(lovr_shader_fill_vert) },
      [STAGE_FRAGMENT] = { STAGE_FRAGMENT, lovr_shader_fill_array_frag, sizeof(lovr_shader_fill_array_frag) }
    },
    [SHADER_ANIMATOR] = {
      [STAGE_COMPUTE] = { STAGE_COMPUTE, lovr_shader_animator_comp, sizeof(lovr_shader_animator_comp) }
    },
    [SHADER_BLENDER] = {
      [STAGE_COMPUTE] = { STAGE_COMPUTE, lovr_shader_blender_comp, sizeof(lovr_shader_blender_comp) }
    },
    [SHADER_TALLY_MERGE] = {
      [STAGE_COMPUTE] = { STAGE_COMPUTE, lovr_shader_tallymerge_comp, sizeof(lovr_shader_tallymerge_comp) }
    }
  };

  return sources[type][stage];
}

Shader* lovrGraphicsGetDefaultShader(DefaultShader type) {
  if (state.defaultShaders[type]) {
    return state.defaultShaders[type];
  }

  switch (type) {
    case SHADER_ANIMATOR:
    case SHADER_BLENDER:
    case SHADER_TALLY_MERGE:
      return state.defaultShaders[type] = lovrShaderCreate(&(ShaderInfo) {
        .type = SHADER_COMPUTE,
        .stages = (ShaderSource[1]) {
          lovrGraphicsGetDefaultShaderSource(type, STAGE_COMPUTE)
        },
        .stageCount = 1,
        .flags = &(ShaderFlag) { NULL, 0, state.device.subgroupSize },
        .flagCount = 1,
        .isDefault = true
      });
    default:
      return state.defaultShaders[type] = lovrShaderCreate(&(ShaderInfo) {
        .type = SHADER_GRAPHICS,
        .stages = (ShaderSource[2]) {
          lovrGraphicsGetDefaultShaderSource(type, STAGE_VERTEX),
          lovrGraphicsGetDefaultShaderSource(type, STAGE_FRAGMENT)
        },
        .stageCount = 2,
        .isDefault = true
      });
  }
}

Shader* lovrShaderCreate(const ShaderInfo* info) {
  size_t stack = tempPush(&state.allocator);

  Shader* shader = lovrCalloc(sizeof(Shader) + gpu_sizeof_shader());
  shader->ref = 1;
  shader->gpu = (gpu_shader*) (shader + 1);
  shader->info = *info;

  if (info->label) {
    size_t size = strlen(info->label) + 1;
    char* label = lovrMalloc(size);
    memcpy(label, info->label, size);
    shader->info.label = label;
  }

  // Validate stage combinations
  for (uint32_t i = 0; i < info->stageCount; i++) {
    shader->stageMask |= (1 << info->stages[i].stage);
  }

  if (info->type == SHADER_GRAPHICS) {
    lovrCheckGoto(fail, shader->stageMask == (FLAG_VERTEX | FLAG_FRAGMENT), "Graphics shaders must have a vertex and a pixel stage");
  } else if (info->type == SHADER_COMPUTE) {
    lovrCheckGoto(fail, shader->stageMask == FLAG_COMPUTE, "Compute shaders can only have a compute stage");
  }

  // Copy the source to temp memory (we perform edits on the SPIR-V and the input might be readonly)
  void* source[2];
  for (uint32_t i = 0; i < info->stageCount; i++) {
    source[i] = tempAlloc(&state.allocator, info->stages[i].size);
    memcpy(source[i], info->stages[i].code, info->stages[i].size);
  }

  // Parse SPIR-V
  spv_result result;
  spv_info spv[2] = { 0 };
  uint32_t maxResources = 0;
  uint32_t maxSpecConstants = 0;
  uint32_t maxFields = 0;
  size_t maxChars = 0;
  for (uint32_t i = 0; i < info->stageCount; i++) {
    result = spv_parse(source[i], info->stages[i].size, &spv[i]);
    lovrAssertGoto(fail, result == SPV_OK, "Failed to load Shader: %s", spv_result_to_string(result));
    lovrAssertGoto(fail, spv[i].version <= 0x00010300, "Invalid SPIR-V version (up to 1.3 is supported)");

    spv[i].features = tempAlloc(&state.allocator, spv[i].featureCount * sizeof(uint32_t));
    spv[i].specConstants = tempAlloc(&state.allocator, spv[i].specConstantCount * sizeof(spv_spec_constant));
    spv[i].attributes = tempAlloc(&state.allocator, spv[i].attributeCount * sizeof(spv_attribute));
    spv[i].resources = tempAlloc(&state.allocator, spv[i].resourceCount * sizeof(spv_resource));
    spv[i].fields = tempAlloc(&state.allocator, spv[i].fieldCount * sizeof(spv_field));
    if (spv[i].fields) memset(spv[i].fields, 0, spv[i].fieldCount * sizeof(spv_field));

    result = spv_parse(source[i], info->stages[i].size, &spv[i]);
    lovrAssertGoto(fail, result == SPV_OK, "Failed to load Shader: %s\n", spv_result_to_string(result));

    if (!checkShaderFeatures(spv[i].features, spv[i].featureCount)) {
      goto fail;
    }

    maxResources += spv[i].resourceCount;
    maxSpecConstants += spv[i].specConstantCount;
    maxFields += spv[i].fieldCount;

    for (uint32_t j = 0; j < spv[i].fieldCount; j++) {
      spv_field* field = &spv[i].fields[j];
      maxChars += field->name ? strlen(field->name) + 1 : 0;
    }
  }

  // Allocate memory
  shader->resources = lovrMalloc(maxResources * sizeof(ShaderResource));
  shader->fields = lovrMalloc(maxFields * sizeof(DataField));
  shader->names = lovrMalloc(maxChars);
  shader->flags = lovrMalloc(maxSpecConstants * sizeof(gpu_shader_flag));
  shader->flagLookup = lovrMalloc(maxSpecConstants * sizeof(uint32_t));

  // Workgroup size
  if (info->type == SHADER_COMPUTE) {
    uint32_t* workgroupSize = spv[0].workgroupSize;
    uint32_t totalWorkgroupSize = workgroupSize[0] * workgroupSize[1] * workgroupSize[2];
    lovrCheckGoto(fail, workgroupSize[0] <= state.limits.workgroupSize[0], "Shader workgroup size exceeds the 'workgroupSize' limit");
    lovrCheckGoto(fail, workgroupSize[1] <= state.limits.workgroupSize[1], "Shader workgroup size exceeds the 'workgroupSize' limit");
    lovrCheckGoto(fail, workgroupSize[2] <= state.limits.workgroupSize[2], "Shader workgroup size exceeds the 'workgroupSize' limit");
    lovrCheckGoto(fail, totalWorkgroupSize <= state.limits.totalWorkgroupSize, "Shader workgroup size exceeds the 'totalWorkgroupSize' limit");
    memcpy(shader->workgroupSize, workgroupSize, 3 * sizeof(uint32_t));
  }

  // Vertex attributes
  if (info->type == SHADER_GRAPHICS && spv[0].attributeCount > 0) {
    shader->attributeCount = spv[0].attributeCount;
    shader->attributes = lovrMalloc(shader->attributeCount * sizeof(ShaderAttribute));
    for (uint32_t i = 0; i < shader->attributeCount; i++) {
      shader->attributes[i].location = spv[0].attributes[i].location;
      shader->attributes[i].hash = (uint32_t) hash64(spv[0].attributes[i].name, strlen(spv[0].attributes[i].name));
      shader->hasCustomAttributes |= shader->attributes[i].location < 10;
    }
  }

  uint32_t resourceSet = info->type == SHADER_COMPUTE ? 0 : 2;
  uint32_t uniformSet = info->type == SHADER_COMPUTE ? 1 : 3;

  // Resources
  for (uint32_t s = 0, lastResourceCount = 0; s < info->stageCount; s++, lastResourceCount = shader->resourceCount) {
    ShaderStage stage = info->stages[s].stage;
    for (uint32_t i = 0; i < spv[s].resourceCount; i++) {
      spv_resource* resource = &spv[s].resources[i];

      // It's safe to cast away const because we are operating on a copy of the input
      uint32_t* set = (uint32_t*) resource->set;
      uint32_t* binding = (uint32_t*) resource->binding;

      // glslang outputs gl_DefaultUniformBlock, there's also the Constants macro which defines a DefaultUniformBlock UBO
      if (!strcmp(resource->name, "gl_DefaultUniformBlock") || !strcmp(resource->name, "DefaultUniformBlock")) {
        spv_field* block = resource->bufferFields;
        shader->uniformSize = block->elementSize;
        shader->uniformCount = block->fieldCount;
        shader->uniforms = shader->fields + ((s == 1 ? spv[0].fieldCount : 0) + (block->fields - spv[s].fields));
        *set = uniformSet;
        *binding = 0;
        continue;
      }

      // Skip builtin resources
      if (info->type == SHADER_GRAPHICS) {
        if (*set == 0) {
          // Unused user resources are given binding #0 for some reason, we still want to keep those
          if (*binding == 0 && !strcmp(resource->name, "Globals")) {
            continue;
          } else if (*binding > 0 && *binding <= LAST_BUILTIN_BINDING) {
            continue;
          }
        } else if (*set == 1) {
          continue;
        }
      }

      if (info->raw && *set != resourceSet) {
        continue;
      }

      static const gpu_slot_type types[] = {
        [SPV_UNIFORM_BUFFER] = GPU_SLOT_UNIFORM_BUFFER,
        [SPV_STORAGE_BUFFER] = GPU_SLOT_STORAGE_BUFFER,
        [SPV_COMBINED_TEXTURE_SAMPLER] = GPU_SLOT_TEXTURE_WITH_SAMPLER,
        [SPV_SAMPLED_TEXTURE] = GPU_SLOT_SAMPLED_TEXTURE,
        [SPV_STORAGE_TEXTURE] = GPU_SLOT_STORAGE_TEXTURE,
        [SPV_SAMPLER] = GPU_SLOT_SAMPLER
      };

      gpu_phase phases[] = {
        [STAGE_VERTEX] = GPU_PHASE_SHADER_VERTEX,
        [STAGE_FRAGMENT] = GPU_PHASE_SHADER_FRAGMENT,
        [STAGE_COMPUTE] = GPU_PHASE_SHADER_COMPUTE
      };

      gpu_slot_type type = types[resource->type];
      gpu_phase phase = phases[stage];

      // Merge resources between shader stages, by name
      bool merged = false;
      uint32_t hash = (uint32_t) hash64(resource->name, strlen(resource->name));
      for (uint32_t j = 0; j < lastResourceCount; j++) {
        ShaderResource* other = &shader->resources[j];
        if (other->hash == hash) {
          lovrCheckGoto(fail, other->type == type, "Shader variable '%s' is declared in multiple shader stages with different types", resource->name);
          *set = resourceSet;
          *binding = shader->resources[j].binding;
          shader->resources[j].phase |= phase;
          merged = true;
          break;
        }
      }

      if (merged) {
        continue;
      }

      uint32_t index = shader->resourceCount++;

      lovrCheckGoto(fail, index < MAX_SHADER_RESOURCES, "Shader resource count exceeds resourcesPerShader limit (%d)", MAX_SHADER_RESOURCES);
      lovrCheckGoto(fail, resource->type != SPV_UNIFORM_TEXEL_BUFFER, "Shader variable '%s' is a%s, which is not supported%s", resource->name, " uniform texel buffer", "");
      lovrCheckGoto(fail, resource->type != SPV_STORAGE_TEXEL_BUFFER, "Shader variable '%s' is a%s, which is not supported%s", resource->name, " storage texel buffer", "");
      lovrCheckGoto(fail, resource->type != SPV_INPUT_ATTACHMENT, "Shader variable '%s' is a%s, which is not supported%s", resource->name, "n input attachment", "");
      lovrCheckGoto(fail, resource->arraySize == 0, "Arrays of resources in shaders are not currently supported");

      // Move resources into user set and give them auto-incremented binding numbers
      // Default shaders refer to resources with explicit binding numbers, so leave those alone
      if (!info->isDefault && !info->raw) {
        *set = resourceSet;
        *binding = index;
      }

      bool buffer = resource->type == SPV_UNIFORM_BUFFER || resource->type == SPV_STORAGE_BUFFER;
      bool texture = resource->type == SPV_SAMPLED_TEXTURE || resource->type == SPV_STORAGE_TEXTURE || resource->type == SPV_COMBINED_TEXTURE_SAMPLER;
      bool sampler = resource->type == SPV_SAMPLER;
      bool storage = resource->type == SPV_STORAGE_BUFFER || resource->type == SPV_STORAGE_TEXTURE;

      shader->bufferMask |= (buffer << *binding);
      shader->textureMask |= (texture << *binding);
      shader->samplerMask |= (sampler << *binding);
      shader->storageMask |= (storage << *binding);

      gpu_cache cache;

      if (storage) {
        cache = info->type == SHADER_COMPUTE ? GPU_CACHE_STORAGE_WRITE : GPU_CACHE_STORAGE_READ;
      } else {
        cache = texture ? GPU_CACHE_TEXTURE : GPU_CACHE_UNIFORM;
      }

      shader->resources[index] = (ShaderResource) {
        .hash = hash,
        .binding = *binding,
        .type = type,
        .phase = phase,
        .cache = cache
      };

      if (buffer && resource->bufferFields) {
        spv_field* field = &resource->bufferFields[0];
        shader->resources[index].fieldCount = field->totalFieldCount + 1;
        shader->resources[index].format = shader->fields + ((s == 1 ? spv[0].fieldCount : 0) + (field - spv[s].fields));
      }
    }
  }

  // Fields
  char* name = shader->names;
  for (uint32_t s = 0; s < info->stageCount; s++) {
    for (uint32_t i = 0; i < spv[s].fieldCount; i++) {
      static const DataType dataTypes[] = {
        [SPV_B32] = TYPE_U32,
        [SPV_I32] = TYPE_I32,
        [SPV_I32x2] = TYPE_I32x2,
        [SPV_I32x3] = TYPE_I32x3,
        [SPV_I32x4] = TYPE_I32x4,
        [SPV_U32] = TYPE_U32,
        [SPV_U32x2] = TYPE_U32x2,
        [SPV_U32x3] = TYPE_U32x3,
        [SPV_U32x4] = TYPE_U32x4,
        [SPV_F32] = TYPE_F32,
        [SPV_F32x2] = TYPE_F32x2,
        [SPV_F32x3] = TYPE_F32x3,
        [SPV_F32x4] = TYPE_F32x4,
        [SPV_MAT2x2] = TYPE_MAT2,
        [SPV_MAT2x3] = ~0u,
        [SPV_MAT2x4] = ~0u,
        [SPV_MAT3x2] = ~0u,
        [SPV_MAT3x3] = TYPE_MAT3,
        [SPV_MAT3x4] = ~0u,
        [SPV_MAT4x2] = ~0u,
        [SPV_MAT4x3] = ~0u,
        [SPV_MAT4x4] = TYPE_MAT4,
        [SPV_STRUCT] = ~0u
      };

      spv_field* field = &spv[s].fields[i];

      uint32_t base = s == 1 ? spv[0].fieldCount : 0;

      shader->fields[base + i] = (DataField) {
        .type = dataTypes[field->type],
        .offset = field->offset,
        .length = field->arrayLength,
        .stride = field->arrayLength > 0 ? field->arrayStride : field->elementSize, // Use stride as element size for non-arrays
        .fieldCount = field->fieldCount,
        .fields = field->fields ? shader->fields + base + (field->fields - spv[s].fields) : NULL
      };

      if (field->name) {
        size_t length = strlen(field->name);
        memcpy(name, field->name, length);
        shader->fields[base + i].hash = (uint32_t) hash64(name, length);
        shader->fields[base + i].name = name;
        name[length] = '\0';
        name += length + 1;
      }
    }
  }

  // Specialization constants
  for (uint32_t s = 0; s < info->stageCount; s++) {
    for (uint32_t i = 0; i < spv[s].specConstantCount; i++) {
      spv_spec_constant* constant = &spv[s].specConstants[i];

      bool append = true;

      if (s > 0) {
        for (uint32_t j = 0; j < spv[0].specConstantCount; j++) {
          spv_spec_constant* other = &spv[0].specConstants[j];
          if (other->id == constant->id) {
            lovrCheckGoto(fail, other->type == constant->type, "Shader flag (%d) does not use a consistent type", constant->id);
            lovrCheckGoto(fail, !strcmp(constant->name, other->name), "Shader flag (%d) does not use a consistent name", constant->id);
            append = false;
            break;
          }
        }
      }

      if (!append) {
        continue;
      }

      static const gpu_flag_type flagTypes[] = {
        [SPV_B32] = GPU_FLAG_B32,
        [SPV_I32] = GPU_FLAG_I32,
        [SPV_U32] = GPU_FLAG_U32,
        [SPV_F32] = GPU_FLAG_F32
      };

      uint32_t index = shader->flagCount++;

      // Flag names can start with flag_ which will be ignored for matching purposes
      if (constant->name) {
        size_t length = strlen(constant->name);
        size_t offset = length > 5 && !memcmp(constant->name, "flag_", 5) ? 5 : 0;
        shader->flagLookup[index] = (uint32_t) hash64(constant->name + offset, length - offset);
      } else {
        shader->flagLookup[index] = 0;
      }

      shader->flags[index] = (gpu_shader_flag) {
        .id = constant->id,
        .type = flagTypes[constant->type]
      };
    }
  }

  // Layout
  gpu_slot* slots = tempAlloc(&state.allocator, shader->resourceCount * sizeof(gpu_slot));
  for (uint32_t i = 0; i < shader->resourceCount; i++) {
    ShaderResource* resource = &shader->resources[i];
    slots[i] = (gpu_slot) {
      .number = resource->binding,
      .type = resource->type,
      .stages =
        ((resource->phase & GPU_PHASE_SHADER_VERTEX) ? GPU_STAGE_VERTEX : 0) |
        ((resource->phase & GPU_PHASE_SHADER_FRAGMENT) ? GPU_STAGE_FRAGMENT : 0) |
        ((resource->phase & GPU_PHASE_SHADER_COMPUTE) ? GPU_STAGE_COMPUTE : 0)
    };
  }

  shader->layout = getLayout(slots, shader->resourceCount);

  if (!shader->layout) {
    goto fail;
  }

  gpu_shader_info gpu = {
    .stageCount = info->stageCount,
    .stages = tempAlloc(&state.allocator, info->stageCount * sizeof(gpu_shader_source)),
    .label = info->label
  };

  for (uint32_t i = 0; i < info->stageCount; i++) {
    const uint32_t stageMap[] = {
      [STAGE_VERTEX] = GPU_STAGE_VERTEX,
      [STAGE_FRAGMENT] = GPU_STAGE_FRAGMENT,
      [STAGE_COMPUTE] = GPU_STAGE_COMPUTE
    };

    gpu.stages[i] = (gpu_shader_source) {
      .stage = stageMap[info->stages[i].stage],
      .code = source[i],
      .length = info->stages[i].size
    };
  }

  for (uint32_t i = 0; i < info->stageCount; i++) {
    if (spv[i].pushConstants) {
      gpu.pushConstantSize = MAX(gpu.pushConstantSize, spv[i].pushConstants->elementSize);
    }
  }

  shader->pushConstantSize = gpu.pushConstantSize;

  if (info->type == SHADER_GRAPHICS) {
    gpu.layouts[0] = state.builtinLayout->gpu;
    gpu.layouts[1] = state.materialLayout->gpu;
    gpu.layouts[2] = shader->layout->gpu;
    gpu.layouts[3] = shader->uniformSize > 0 ? state.uniformLayout->gpu : NULL;
  } else {
    gpu.layouts[0] = shader->layout->gpu;
    gpu.layouts[1] = shader->uniformSize > 0 ? state.uniformLayout->gpu : NULL;
  }

  lovrAssertGoto(fail, gpu_shader_init(shader->gpu, &gpu), "Failed to create shader: %s", gpu_get_error());

  if (!lovrShaderInit(shader)) {
    goto fail;
  }

  tempPop(&state.allocator, stack);
  return shader;
fail:
  tempPop(&state.allocator, stack);
  lovrShaderDestroy(shader);
  return NULL;
}

Shader* lovrShaderClone(Shader* parent, ShaderFlag* flags, uint32_t count) {
  Shader* shader = lovrCalloc(sizeof(Shader) + gpu_sizeof_shader());
  shader->ref = 1;
  shader->parent = parent;
  shader->gpu = parent->gpu;
  shader->info = parent->info;
  shader->info.flags = flags;
  shader->info.flagCount = count;
  shader->layout = parent->layout;
  shader->stageMask = parent->stageMask;
  shader->bufferMask = parent->bufferMask;
  shader->textureMask = parent->textureMask;
  shader->samplerMask = parent->samplerMask;
  shader->storageMask = parent->storageMask;
  shader->uniformSize = parent->uniformSize;
  shader->uniformCount = parent->uniformCount;
  shader->resourceCount = parent->resourceCount;
  shader->flagCount = parent->flagCount;
  shader->attributes = parent->attributes;
  shader->resources = parent->resources;
  shader->uniforms = parent->uniforms;
  shader->fields = parent->fields;
  shader->names = parent->names;
  shader->flags = lovrMalloc(shader->flagCount * sizeof(gpu_shader_flag));
  shader->flagLookup = lovrMalloc(shader->flagCount * sizeof(uint32_t));
  memcpy(shader->flags, parent->flags, shader->flagCount * sizeof(gpu_shader_flag));
  memcpy(shader->flagLookup, parent->flagLookup, shader->flagCount * sizeof(uint32_t));
  if (!lovrShaderInit(shader)) return NULL;
  lovrRetain(parent);
  return shader;
}

void lovrShaderDestroy(void* ref) {
  Shader* shader = ref;
  if (shader->parent) {
    lovrRelease(shader->parent, lovrShaderDestroy);
  } else {
    gpu_shader_destroy(shader->gpu);
    lovrFree(shader->attributes);
    lovrFree(shader->resources);
    lovrFree(shader->fields);
    lovrFree(shader->names);
    lovrFree((char*) shader->info.label);
  }
  lovrFree(shader->flags);
  lovrFree(shader->flagLookup);
  lovrFree(shader);
}

const ShaderInfo* lovrShaderGetInfo(Shader* shader) {
  return &shader->info;
}

bool lovrShaderHasStage(Shader* shader, ShaderStage stage) {
  return shader->stageMask & (1 << stage);
}

bool lovrShaderHasAttribute(Shader* shader, const char* name, uint32_t location) {
  if (name) {
    uint32_t hash = (uint32_t) hash64(name, strlen(name));
    for (uint32_t i = 0; i < shader->attributeCount; i++) {
      if (shader->attributes[i].hash == hash) {
        return true;
      }
    }
  } else {
    for (uint32_t i = 0; i < shader->attributeCount; i++) {
      if (shader->attributes[i].location == location) {
        return true;
      }
    }
  }

  return false;
}

bool lovrShaderHasVariable(Shader* shader, const char* name) {
  uint32_t hash = (uint32_t) hash64(name, strlen(name));

  for (uint32_t i = 0; i < shader->uniformCount; i++) {
    if (shader->uniforms[i].hash == hash) {
      return true;
    }
  }

  for (uint32_t i = 0; i < shader->resourceCount; i++) {
    if (shader->resources[i].hash == hash) {
      return true;
    }
  }

  return false;
}

void lovrShaderGetWorkgroupSize(Shader* shader, uint32_t size[3]) {
  memcpy(size, shader->workgroupSize, 3 * sizeof(uint32_t));
}

const DataField* lovrShaderGetBufferFormat(Shader* shader, const char* name, uint32_t* fieldCount) {
  uint32_t hash = (uint32_t) hash64(name, strlen(name));
  ShaderResource* resource = shader->resources;

  for (uint32_t i = 0; i < shader->resourceCount; i++, resource++) {
    if (resource->hash == hash && (shader->bufferMask & (1u << resource->binding))) {
      *fieldCount = resource->fieldCount;
      return resource->format;
    }
  }

  return NULL;
}

// Material

Material* lovrMaterialCreate(const MaterialInfo* info) {
  Texture* textures[] = {
    info->texture,
    info->glowTexture,
    info->metalnessTexture,
    info->roughnessTexture,
    info->clearcoatTexture,
    info->occlusionTexture,
    info->normalTexture
  };

  for (uint32_t i = 0; i < COUNTOF(textures); i++) {
    if (!textures[i]) continue;
    lovrCheck(i == 0 || textures[i]->info.type == TEXTURE_2D, "Material textures must be 2D");
    lovrCheck(textures[i]->info.usage & TEXTURE_SAMPLE, "Textures must be created with the 'sample' usage to use them in Materials");
  }

  MaterialBlock* block = state.materialBlocks.length > 0 ? &state.materialBlocks.data[state.materialBlock] : NULL;
  const uint32_t MATERIALS_PER_BLOCK = 256;

  if (!block || block->head == ~0u || !gpu_is_complete(block->list[block->head].tick)) {
    bool found = false;

    for (size_t i = 0; i < state.materialBlocks.length; i++) {
      block = &state.materialBlocks.data[i];
      if (block->head != ~0u && gpu_is_complete(block->list[block->head].tick)) {
        state.materialBlock = i;
        found = true;
        break;
      }
    }

    if (!found) {
      arr_expand(&state.materialBlocks, 1);
      lovrAssert(state.materialBlocks.length < UINT16_MAX, "Out of memory");
      uint32_t blockIndex = state.materialBlocks.length;
      block = &state.materialBlocks.data[blockIndex];
      block->list = lovrMalloc(MATERIALS_PER_BLOCK * sizeof(Material));
      block->bundlePool = lovrMalloc(gpu_sizeof_bundle_pool());
      block->bundles = lovrMalloc(MATERIALS_PER_BLOCK * gpu_sizeof_bundle());

      for (uint32_t i = 0; i < MATERIALS_PER_BLOCK; i++) {
        block->list[i].next = i + 1;
        block->list[i].tick = 0;
        block->list[i].block = (uint16_t) blockIndex;
        block->list[i].index = i;
        block->list[i].bundle = (gpu_bundle*) ((char*) block->bundles + i * gpu_sizeof_bundle());
        block->list[i].hasWritableTexture = false;
      }
      block->list[MATERIALS_PER_BLOCK - 1].next = ~0u;
      block->tail = MATERIALS_PER_BLOCK - 1;
      block->head = 0;

      gpu_bundle_pool_info poolInfo = {
        .bundles = block->bundles,
        .layout = state.materialLayout->gpu,
        .count = MATERIALS_PER_BLOCK
      };

      if (!gpu_bundle_pool_init(block->bundlePool, &poolInfo)) {
        lovrFree(block->list);
        lovrFree(block->bundlePool);
        lovrFree(block->bundles);
        return NULL;
      }

      size_t align = state.limits.uniformBufferAlign;
      uint32_t bufferSize = MATERIALS_PER_BLOCK * (uint32_t) ALIGN(sizeof(MaterialData), align);
      block->view = getBuffer(GPU_BUFFER_STATIC, bufferSize, align);

      if (!block->view.buffer) {
        lovrFree(block->list);
        lovrFree(block->bundlePool);
        lovrFree(block->bundles);
        gpu_bundle_pool_destroy(block->bundlePool);
        return NULL;
      }

      atomic_fetch_add(&block->view.block->ref, 1);
      state.materialBlock = blockIndex;
      state.materialBlocks.length++;
    }
  }

  Material* material = &block->list[block->head];
  material->ref = 1;
  material->info = *info;

  MaterialData* data;
  uint32_t stride = ALIGN(sizeof(MaterialData), state.limits.uniformBufferAlign);

  if (block->view.pointer) {
    data = (MaterialData*) ((char*) block->view.pointer + material->index * stride);
  } else {
    if (!beginFrame()) {
      return NULL;
    }

    BufferView staging = getBuffer(GPU_BUFFER_UPLOAD, sizeof(MaterialData), 4);
    if (!staging.buffer) return NULL;

    gpu_copy_buffers(state.stream, staging.buffer, block->view.buffer, staging.offset, block->view.offset + stride * material->index, sizeof(MaterialData));
    state.barrier.prev |= GPU_PHASE_COPY;
    state.barrier.next |= GPU_PHASE_SHADER_VERTEX | GPU_PHASE_SHADER_FRAGMENT;
    state.barrier.flush |= GPU_CACHE_TRANSFER_WRITE;
    state.barrier.clear |= GPU_CACHE_UNIFORM;
    data = staging.pointer;
  }

  memcpy(data, info, sizeof(MaterialData));

  gpu_buffer_binding buffer = {
    .object = block->view.buffer,
    .offset = block->view.offset + material->index * stride,
    .extent = stride
  };

  gpu_binding bindings[8] = {
    { 0, GPU_SLOT_UNIFORM_BUFFER, .buffer = buffer }
  };

  for (uint32_t i = 0; i < COUNTOF(textures); i++) {
    Texture* texture = textures[i] ? textures[i] : state.defaultTexture;
    bindings[i + 1] = (gpu_binding) { i + 1, GPU_SLOT_SAMPLED_TEXTURE, .texture.object = texture->sampleView };
    material->hasWritableTexture |= texture->info.usage != TEXTURE_SAMPLE;
    lovrRetain(textures[i]);
  }

  gpu_bundle_info bundleInfo = {
    .layout = state.materialLayout->gpu,
    .bindings = bindings,
    .count = COUNTOF(bindings)
  };

  gpu_bundle_write(&material->bundle, &bundleInfo, 1);

  block->head = material->next;
  material->next = ~0u;
  return material;
}

void lovrMaterialDestroy(void* ref) {
  Material* material = ref;
  MaterialBlock* block = &state.materialBlocks.data[material->block];
  material->tick = state.tick;
  block->tail = material->index;
  if (block->head == ~0u) block->head = block->tail;
  lovrRelease(material->info.texture, lovrTextureDestroy);
  lovrRelease(material->info.glowTexture, lovrTextureDestroy);
  lovrRelease(material->info.metalnessTexture, lovrTextureDestroy);
  lovrRelease(material->info.roughnessTexture, lovrTextureDestroy);
  lovrRelease(material->info.clearcoatTexture, lovrTextureDestroy);
  lovrRelease(material->info.occlusionTexture, lovrTextureDestroy);
  lovrRelease(material->info.normalTexture, lovrTextureDestroy);
}

const MaterialInfo* lovrMaterialGetInfo(Material* material) {
  return &material->info;
}

// Font

Font* lovrGraphicsGetDefaultFont(void) {
  if (!state.defaultFont) {
    Rasterizer* rasterizer = lovrRasterizerCreate(NULL, 32, NULL);
    if (!rasterizer) return NULL;
    state.defaultFont = lovrFontCreate(&(FontInfo) {
      .rasterizer = rasterizer,
      .spread = 4.
    });
    lovrRelease(rasterizer, lovrRasterizerDestroy);
  }

  return state.defaultFont;
}

Font* lovrFontCreate(const FontInfo* info) {
  Font* font = lovrCalloc(sizeof(Font));
  font->ref = 1;
  font->info = *info;
  lovrRetain(info->rasterizer);
  arr_init(&font->glyphs);
  map_init(&font->glyphLookup, 36);

  font->pixelDensity = lovrRasterizerGetLeading(info->rasterizer);
  font->lineSpacing = 1.f;
  font->padding = lovrRasterizerGetType(info->rasterizer) == RASTERIZER_TTF ? (uint32_t) ceil(info->spread / 2.) : 0;

  Image* image = lovrRasterizerGetAtlas(info->rasterizer);

  if (image) {
    font->atlasWidth = lovrImageGetWidth(image, 0);
    font->atlasHeight = lovrImageGetHeight(image, 0);

    font->atlas = lovrTextureCreate(&(TextureInfo) {
      .type = TEXTURE_2D,
      .format = lovrImageGetFormat(image),
      .width = font->atlasWidth,
      .height = font->atlasHeight,
      .layers = 1,
      .mipmaps = 1,
      .usage = TEXTURE_SAMPLE,
      .srgb = true,
      .imageCount = 1,
      .images = &image,
      .label = "Font Atlas"
    });

    if (!font->atlas) {
      lovrFontDestroy(font);
      return NULL;
    }

    font->material = lovrMaterialCreate(&(MaterialInfo) {
      .data.color = { 1.f, 1.f, 1.f, 1.f },
      .data.uvScale = { 1.f, 1.f },
      .texture = font->atlas
    });

    if (!font->material) {
      lovrFontDestroy(font);
      return NULL;
    }

    size_t glyphCount = lovrRasterizerGetGlyphCount(info->rasterizer);

    for (size_t i = 0; i < glyphCount; i++) {
      arr_expand(&font->glyphs, 1);
      Glyph* glyph = &font->glyphs.data[font->glyphs.length++];
      uint32_t codepoint = lovrRasterizerGetAtlasGlyph(info->rasterizer, i, &glyph->x, &glyph->y);
      map_set(&font->glyphLookup, hash64(&codepoint, 4), font->glyphs.length - 1);

      lovrRasterizerGetGlyphBoundingBox(info->rasterizer, codepoint, glyph->box);

      float width = glyph->box[2] - glyph->box[0];
      float height = glyph->box[3] - glyph->box[1];
      glyph->uv[0] = (uint16_t) ((float) glyph->x / font->atlasWidth * 65535.f + .5f);
      glyph->uv[1] = (uint16_t) ((float) glyph->y / font->atlasHeight * 65535.f + .5f);
      glyph->uv[2] = (uint16_t) ((float) (glyph->x + width) / font->atlasWidth * 65535.f + .5f);
      glyph->uv[3] = (uint16_t) ((float) (glyph->y + height) / font->atlasHeight * 65535.f + .5f);
      glyph->advance = lovrRasterizerGetAdvance(font->info.rasterizer, codepoint);
    }
  } else {
    // Initial atlas size must be big enough to hold any of the glyphs
    float box[4];
    font->atlasWidth = 1;
    font->atlasHeight = 1;
    lovrRasterizerGetBoundingBox(info->rasterizer, box);
    uint32_t maxWidth = (uint32_t) ceilf(box[2] - box[0]) + 2 * font->padding;
    uint32_t maxHeight = (uint32_t) ceilf(box[3] - box[1]) + 2 * font->padding;
    while (font->atlasWidth < 2 * maxWidth || font->atlasHeight < 2 * maxHeight) {
      font->atlasWidth <<= 1;
      font->atlasHeight <<= 1;
    }
  }

  return font;
}

void lovrFontDestroy(void* ref) {
  Font* font = ref;
  lovrRelease(font->info.rasterizer, lovrRasterizerDestroy);
  lovrRelease(font->material, lovrMaterialDestroy);
  lovrRelease(font->atlas, lovrTextureDestroy);
  arr_free(&font->glyphs);
  map_free(&font->glyphLookup);
  lovrFree(font);
}

const FontInfo* lovrFontGetInfo(Font* font) {
  return &font->info;
}

float lovrFontGetPixelDensity(Font* font) {
  return font->pixelDensity;
}

void lovrFontSetPixelDensity(Font* font, float pixelDensity) {
  font->pixelDensity = pixelDensity;
}

float lovrFontGetLineSpacing(Font* font) {
  return font->lineSpacing;
}

void lovrFontSetLineSpacing(Font* font, float spacing) {
  font->lineSpacing = spacing;
}

static Glyph* lovrFontGetGlyph(Font* font, uint32_t codepoint, bool* resized) {
  uint64_t hash = hash64(&codepoint, 4);
  uint64_t index = map_get(&font->glyphLookup, hash);

  if (index != MAP_NIL) {
    if (resized) *resized = false;
    return &font->glyphs.data[index];
  }

  arr_expand(&font->glyphs, 1);
  Glyph* glyph = &font->glyphs.data[font->glyphs.length];
  glyph->advance = lovrRasterizerGetAdvance(font->info.rasterizer, codepoint);

  if (lovrRasterizerIsGlyphEmpty(font->info.rasterizer, codepoint)) {
    memset(glyph->box, 0, sizeof(glyph->box));
    if (resized) *resized = false;
    map_set(&font->glyphLookup, hash, font->glyphs.length++);
    return glyph;
  }

  lovrRasterizerGetGlyphBoundingBox(font->info.rasterizer, codepoint, glyph->box);

  float width = glyph->box[2] - glyph->box[0];
  float height = glyph->box[3] - glyph->box[1];
  uint32_t pixelWidth = 2 * font->padding + (uint32_t) ceilf(width);
  uint32_t pixelHeight = 2 * font->padding + (uint32_t) ceilf(height);

  if (!beginFrame()) {
    return NULL;
  }

  BufferView bufferView = getBuffer(GPU_BUFFER_UPLOAD, pixelWidth * pixelHeight * 4 * sizeof(uint8_t), 64);

  if (!bufferView.buffer) {
    return NULL;
  }

  bool wrap = font->atlasX + pixelWidth > font->atlasWidth;
  bool resize = font->atlasY + (wrap ? font->rowHeight : 0) + pixelHeight > font->atlasHeight;

  if (!font->atlas || resize) {
    uint32_t newWidth = font->atlasWidth << (font->atlasWidth == font->atlasHeight);
    uint32_t newHeight = font->atlasHeight << (font->atlasWidth != font->atlasHeight);
    lovrCheck(newWidth <= 65536, "Font atlas is way too big!");

    Texture* atlas = lovrTextureCreate(&(TextureInfo) {
      .type = TEXTURE_2D,
      .format = FORMAT_RGBA8,
      .width = newWidth,
      .height = newHeight,
      .layers = 1,
      .mipmaps = 1,
      .usage = TEXTURE_SAMPLE | TEXTURE_TRANSFER,
      .label = "Font Atlas"
    });

    if (!atlas) {
      return NULL;
    }

    Material* material = lovrMaterialCreate(&(MaterialInfo) {
      .data.color = { 1.f, 1.f, 1.f, 1.f },
      .data.uvScale = { 1.f, 1.f },
      .data.sdfRange = { font->info.spread / newWidth, font->info.spread / newHeight },
      .texture = atlas
    });

    if (!material) {
      lovrTextureDestroy(atlas);
      return NULL;
    }

    float clear[4] = { 0.f, 0.f, 0.f, 0.f };
    gpu_clear_texture(state.stream, atlas->gpu, clear, 0, ~0u, 0, ~0u);

    // This barrier serves 2 purposes:
    // - Ensure new atlas clear is finished/flushed before copying to it
    // - Ensure any unsynchronized pending uploads to old atlas finish before copying to new atlas
    gpu_barrier barrier;
    barrier.prev = GPU_PHASE_COPY | GPU_PHASE_CLEAR;
    barrier.next = GPU_PHASE_COPY;
    barrier.flush = GPU_CACHE_TRANSFER_WRITE;
    barrier.clear = GPU_CACHE_TRANSFER_READ | GPU_CACHE_TRANSFER_WRITE;
    gpu_sync(state.stream, &barrier, 1);

    if (font->atlas) {
      uint32_t srcOffset[4] = { 0, 0, 0, 0 };
      uint32_t dstOffset[4] = { 0, 0, 0, 0 };
      uint32_t extent[3] = { font->atlasWidth, font->atlasHeight, 1 };
      gpu_copy_textures(state.stream, font->atlas->gpu, atlas->gpu, srcOffset, dstOffset, extent);
    }

    lovrRelease(font->material, lovrMaterialDestroy);
    lovrRelease(font->atlas, lovrTextureDestroy);
    font->material = material;
    font->atlas = atlas;
    font->atlasWidth = newWidth;
    font->atlasHeight = newHeight;
    font->rowHeight = 0;

    // Adjust the cursor after a successful resize, but only if this isn't the first glyph
    if (font->atlasX > 0 || font->atlasY > 0) {
      if (newWidth == newHeight) {
        font->atlasX = 0;
        font->atlasY = newHeight / 2;
      } else {
        font->atlasX = newWidth / 2;
        font->atlasY = 0;
      }
    }

    // Recompute all glyph uvs after atlas resize
    for (size_t i = 0; i < font->glyphs.length; i++) {
      Glyph* g = &font->glyphs.data[i];
      if (g->box[2] - g->box[0] > 0.f) {
        g->uv[0] = (uint16_t) ((float) g->x / newWidth * 65535.f + .5f);
        g->uv[1] = (uint16_t) ((float) g->y / newHeight * 65535.f + .5f);
        g->uv[2] = (uint16_t) ((float) (g->x + g->box[2] - g->box[0]) / newWidth * 65535.f + .5f);
        g->uv[3] = (uint16_t) ((float) (g->y + g->box[3] - g->box[1]) / newHeight * 65535.f + .5f);
      }
    }

    if (resized) *resized = true;
    wrap = false;
  }

  if (wrap) {
    font->atlasX = font->atlasWidth == font->atlasHeight ? 0 : font->atlasWidth >> 1;
    font->atlasY += font->rowHeight;
  }

  glyph->x = font->atlasX + font->padding;
  glyph->y = font->atlasY + font->padding;
  glyph->uv[0] = (uint16_t) ((float) glyph->x / font->atlasWidth * 65535.f + .5f);
  glyph->uv[1] = (uint16_t) ((float) glyph->y / font->atlasHeight * 65535.f + .5f);
  glyph->uv[2] = (uint16_t) ((float) (glyph->x + width) / font->atlasWidth * 65535.f + .5f);
  glyph->uv[3] = (uint16_t) ((float) (glyph->y + height) / font->atlasHeight * 65535.f + .5f);

  font->atlasX += pixelWidth;
  font->rowHeight = MAX(font->rowHeight, pixelHeight);

  size_t stack = tempPush(&state.allocator);
  float* pixels = tempAlloc(&state.allocator, pixelWidth * pixelHeight * 4 * sizeof(float));
  lovrRasterizerGetPixels(font->info.rasterizer, codepoint, pixels, pixelWidth, pixelHeight, font->info.spread);
  float* src = pixels;
  uint8_t* dst = bufferView.pointer;
  for (uint32_t y = 0; y < pixelHeight; y++) {
    for (uint32_t x = 0; x < pixelWidth; x++) {
      for (uint32_t c = 0; c < 4; c++) {
        float f = *src++; // CLAMP would evaluate this multiple times
        *dst++ = (uint8_t) (CLAMP(f, 0.f, 1.f) * 255.f + .5f);
      }
    }
  }
  uint32_t dstOffset[4] = { glyph->x - font->padding, glyph->y - font->padding, 0, 0 };
  uint32_t extent[3] = { pixelWidth, pixelHeight, 1 };
  gpu_copy_buffer_texture(state.stream, bufferView.buffer, font->atlas->gpu, bufferView.offset, dstOffset, extent);
  tempPop(&state.allocator, stack);

  state.barrier.prev |= GPU_PHASE_COPY;
  state.barrier.next |= GPU_PHASE_SHADER_FRAGMENT;
  state.barrier.flush |= GPU_CACHE_TRANSFER_WRITE;
  state.barrier.clear |= GPU_CACHE_TEXTURE;
  map_set(&font->glyphLookup, hash, font->glyphs.length++);
  return glyph;
}

float lovrFontGetWidth(Font* font, ColoredString* strings, uint32_t count) {
  float x = 0.f;
  float maxWidth = 0.f;
  float space = lovrRasterizerGetAdvance(font->info.rasterizer, ' ');

  for (uint32_t i = 0; i < count; i++) {
    size_t bytes;
    uint32_t codepoint;
    uint32_t previous = '\0';
    const char* str = strings[i].string;
    const char* end = strings[i].string + strings[i].length;
    while ((bytes = utf8_decode(str, end, &codepoint)) > 0) {
      if (codepoint == ' ' || codepoint == '\t') {
        x += codepoint == '\t' ? space * 4.f : space;
        previous = '\0';
        str += bytes;
        continue;
      } else if (codepoint == '\n') {
        maxWidth = MAX(maxWidth, x);
        x = 0.f;
        previous = '\0';
        str += bytes;
        continue;
      } else if (codepoint == '\r') {
        str += bytes;
        continue;
      }

      if (previous) x += lovrRasterizerGetKerning(font->info.rasterizer, previous, codepoint);
      previous = codepoint;

      x += lovrRasterizerGetAdvance(font->info.rasterizer, codepoint);
      str += bytes;
    }
  }

  return MAX(maxWidth, x) / font->pixelDensity;
}

void lovrFontGetLines(Font* font, ColoredString* strings, uint32_t count, float wrap, void (*callback)(void* context, const char* string, size_t length), void* context) {
  if (count == 0) return;

  size_t totalLength = 0;
  for (uint32_t i = 0; i < count; i++) {
    totalLength += strings[i].length;
  }

  size_t stack = tempPush(&state.allocator);
  char* string = tempAlloc(&state.allocator, totalLength + 1);
  string[totalLength] = '\0';

  size_t cursor = 0;
  for (uint32_t i = 0; i < count; cursor += strings[i].length, i++) {
    memcpy(string + cursor, strings[i].string, strings[i].length);
  }

  float x = 0.f;
  float nextWordStartX = 0.f;
  wrap *= font->pixelDensity;

  size_t bytes;
  uint32_t codepoint;
  uint32_t previous = '\0';
  const char* lineStart = string;
  const char* wordStart = string;
  const char* end = string + totalLength;
  float space = lovrRasterizerGetAdvance(font->info.rasterizer, ' ');
  while ((bytes = utf8_decode(string, end, &codepoint)) > 0) {
    if (codepoint == ' ' || codepoint == '\t') {
      x += codepoint == '\t' ? space * 4.f : space;
      nextWordStartX = x;
      previous = '\0';
      string += bytes;
      wordStart = string;
      continue;
    } else if (codepoint == '\n') {
      size_t length = string - lineStart;
      while (string[length] == ' ' || string[length] == '\t') length--;
      callback(context, lineStart, length);
      nextWordStartX = 0.f;
      x = 0.f;
      previous = '\0';
      string += bytes;
      lineStart = string;
      wordStart = string;
      continue;
    } else if (codepoint == '\r') {
      string += bytes;
      continue;
    }

    float advance = lovrRasterizerGetAdvance(font->info.rasterizer, codepoint);

    // Keming
    if (previous) x += lovrRasterizerGetKerning(font->info.rasterizer, previous, codepoint);
    previous = codepoint;

    // Wrap
    if (wordStart != lineStart && x + advance > wrap) {
      size_t length = wordStart - lineStart;
      while (string[length] == ' ' || string[length] == '\t') length--;
      callback(context, lineStart, length);
      lineStart = wordStart;
      x -= nextWordStartX;
      nextWordStartX = 0.f;
      previous = '\0';
    }

    // Advance
    x += advance;
    string += bytes;
  }

  if (end - lineStart > 0) {
    callback(context, lineStart, end - lineStart);
  }

  tempPop(&state.allocator, stack);
}

static void aline(GlyphVertex* vertices, uint32_t head, uint32_t tail, float width, HorizontalAlign align) {
  if (align == ALIGN_LEFT) return;
  float shift = align / 2.f * width;
  for (uint32_t i = head; i < tail; i++) {
    vertices[i].position.x -= shift;
  }
}

bool lovrFontGetVertices(Font* font, ColoredString* strings, uint32_t count, float wrap, HorizontalAlign halign, VerticalAlign valign, GlyphVertex* vertices, uint32_t* glyphCount, uint32_t* lineCount, Material** material, bool flip) {
  uint32_t vertexCount = 0;
  uint32_t lineStart = 0;
  uint32_t wordStart = 0;
  *glyphCount = 0;
  *lineCount = 1;

  float x = 0.f;
  float y = 0.f;
  float wordStartX = 0.f;
  float prevWordEndX = 0.f;
  float leading = lovrRasterizerGetLeading(font->info.rasterizer) * font->lineSpacing;
  float space = lovrRasterizerGetAdvance(font->info.rasterizer, ' ');

  for (uint32_t i = 0; i < count; i++) {
    size_t bytes;
    uint32_t codepoint;
    uint32_t previous = '\0';
    const char* str = strings[i].string;
    const char* end = strings[i].string + strings[i].length;
    float rf = lovrMathGammaToLinear(strings[i].color[0]);
    float gf = lovrMathGammaToLinear(strings[i].color[1]);
    float bf = lovrMathGammaToLinear(strings[i].color[2]);
    uint8_t r = (uint8_t) (CLAMP(rf, 0.f, 1.f) * 255.f);
    uint8_t g = (uint8_t) (CLAMP(gf, 0.f, 1.f) * 255.f);
    uint8_t b = (uint8_t) (CLAMP(bf, 0.f, 1.f) * 255.f);
    uint8_t a = (uint8_t) (CLAMP(strings[i].color[3], 0.f, 1.f) * 255.f);

    while ((bytes = utf8_decode(str, end, &codepoint)) > 0) {
      if (codepoint == ' ' || codepoint == '\t') {
        if (previous) prevWordEndX = x;
        wordStart = vertexCount;
        x += codepoint == '\t' ? space * 4.f : space;
        wordStartX = x;
        previous = '\0';
        str += bytes;
        continue;
      } else if (codepoint == '\n') {
        aline(vertices, lineStart, vertexCount, x, halign);
        lineStart = vertexCount;
        wordStart = vertexCount;
        x = 0.f;
        y -= leading;
        wordStartX = 0.f;
        prevWordEndX = 0.f;
        (*lineCount)++;
        previous = '\0';
        str += bytes;
        continue;
      } else if (codepoint == '\r') {
        str += bytes;
        continue;
      }

      bool resized = false;
      Glyph* glyph = lovrFontGetGlyph(font, codepoint, &resized);

      if (!glyph) {
        return false;
      }

      if (resized) {
        return lovrFontGetVertices(font, strings, count, wrap, halign, valign, vertices, glyphCount, lineCount, material, flip);
      }

      // Keming
      if (previous) x += lovrRasterizerGetKerning(font->info.rasterizer, previous, codepoint);
      previous = codepoint;

      // Wrap
      if (wrap > 0.f && x + glyph->advance > wrap && wordStart != lineStart) {
        float dx = wordStartX;
        float dy = leading;

        // Shift the vertices of the overflowing word down a line and back to the beginning
        for (uint32_t v = wordStart; v < vertexCount; v++) {
          vertices[v].position.x -= dx;
          vertices[v].position.y += flip ? dy : -dy;
        }

        aline(vertices, lineStart, wordStart, prevWordEndX, halign);
        lineStart = wordStart;
        wordStartX = 0.f;
        (*lineCount)++;
        x -= dx;
        y -= dy;
      }

      // Vertices
      float* bb = glyph->box;
      uint16_t* uv = glyph->uv;
      if (flip) {
        vertices[vertexCount++] = (GlyphVertex) { { x + bb[0], -(y + bb[1]) }, { uv[0], uv[3] }, { r, g, b, a } };
        vertices[vertexCount++] = (GlyphVertex) { { x + bb[2], -(y + bb[1]) }, { uv[2], uv[3] }, { r, g, b, a } };
        vertices[vertexCount++] = (GlyphVertex) { { x + bb[0], -(y + bb[3]) }, { uv[0], uv[1] }, { r, g, b, a } };
        vertices[vertexCount++] = (GlyphVertex) { { x + bb[2], -(y + bb[3]) }, { uv[2], uv[1] }, { r, g, b, a } };
      } else {
        vertices[vertexCount++] = (GlyphVertex) { { x + bb[0], y + bb[3] }, { uv[0], uv[1] }, { r, g, b, a } };
        vertices[vertexCount++] = (GlyphVertex) { { x + bb[2], y + bb[3] }, { uv[2], uv[1] }, { r, g, b, a } };
        vertices[vertexCount++] = (GlyphVertex) { { x + bb[0], y + bb[1] }, { uv[0], uv[3] }, { r, g, b, a } };
        vertices[vertexCount++] = (GlyphVertex) { { x + bb[2], y + bb[1] }, { uv[2], uv[3] }, { r, g, b, a } };
      }
      (*glyphCount)++;

      // Advance
      x += glyph->advance;
      str += bytes;
    }
  }

  // Align last line
  aline(vertices, lineStart, vertexCount, x, halign);

  *material = font->material;
  return true;
}

// Mesh

Mesh* lovrMeshCreate(const MeshInfo* info, void** vertices) {
  Buffer* buffer = info->vertexBuffer;
  DataField* format = buffer ? buffer->info.format : info->vertexFormat;

  lovrCheck(format, "Mesh vertex buffer must have format information");
  lovrCheck(format->length > 0, "Mesh must have at least one vertex");
  lovrCheck(!buffer || buffer->supportsMesh, "Mesh vertex buffer has invalid format (can not contain nested structs/arrays, or matrix/index types)");
  lovrCheck(!buffer || info->storage == MESH_GPU, "Mesh storage must be 'gpu' when created from a Buffer");
  lovrCheck(format->stride <= state.limits.vertexBufferStride, "Mesh vertex buffer stride exceeds the vertexBufferStride limit of this GPU");
  lovrCheck(format->fieldCount <= state.limits.vertexAttributes, "Mesh attribute count exceeds the vertexAttributes limit of this GPU");

  for (uint32_t i = 0; i < MAX(format->fieldCount, 1); i++) {
    const DataField* attribute = format->fieldCount > 0 ? &format->fields[i] : format;
    lovrCheck(attribute->offset < 256, "Max Mesh attribute offset is 255"); // Limited by u8 gpu_attribute offset
  }

  Mesh* mesh = lovrCalloc(sizeof(Mesh));
  mesh->ref = 1;
  mesh->storage = info->storage;
  mesh->mode = DRAW_TRIANGLES;

  if (buffer) {
    mesh->vertexBuffer = buffer;
    lovrRetain(buffer);
  } else {
    BufferInfo bufferInfo = { .format = info->vertexFormat };
    buffer = lovrBufferCreate(&bufferInfo, info->storage == MESH_GPU ? vertices : NULL);
    mesh->vertexBuffer = buffer;

    if (!buffer) {
      lovrSetError("Failed to create Mesh vertex buffer: %s", lovrGetError());
      lovrMeshDestroy(mesh);
      return NULL;
    }

    if (!vertices && !lovrBufferClear(buffer, 0, ~0u, 0)) {
      lovrSetError("Failed to clear Mesh vertex buffer: %s", lovrGetError());
      lovrMeshDestroy(mesh);
      return NULL;
    }

    if (mesh->storage == MESH_CPU) {
      mesh->vertices = vertices ? lovrMalloc(buffer->info.size) : lovrCalloc(buffer->info.size);

      if (vertices) {
        *vertices = mesh->vertices;
        mesh->dirtyVertices[0] = 0;
        mesh->dirtyVertices[1] = format->length;
      } else {
        mesh->dirtyVertices[0] = ~0u;
        mesh->dirtyVertices[1] = 0;
      }
    }
  }

  return mesh;
}

void lovrMeshDestroy(void* ref) {
  Mesh* mesh = ref;
  lovrRelease(mesh->vertexBuffer, lovrBufferDestroy);
  lovrRelease(mesh->indexBuffer, lovrBufferDestroy);
  lovrRelease(mesh->material, lovrMaterialDestroy);
  lovrFree(mesh->vertices);
  lovrFree(mesh->indices);
  lovrFree(mesh);
}

const DataField* lovrMeshGetVertexFormat(Mesh* mesh) {
  return mesh->vertexBuffer->info.format;
}

const DataField* lovrMeshGetIndexFormat(Mesh* mesh) {
  return mesh->indexCount > 0 || !mesh->indexBuffer ? mesh->indexBuffer->info.format : NULL;
}

Buffer* lovrMeshGetVertexBuffer(Mesh* mesh) {
  return mesh->storage == MESH_CPU ? NULL : mesh->vertexBuffer;
}

Buffer* lovrMeshGetIndexBuffer(Mesh* mesh) {
  return mesh->storage == MESH_CPU ? NULL : mesh->indexBuffer;
}

bool lovrMeshSetIndexBuffer(Mesh* mesh, Buffer* buffer) {
  lovrCheck(mesh->storage == MESH_GPU, "Mesh can only use a Buffer for indices if it was created with 'gpu' storage mode");

  DataField* format = buffer->info.format;
  lovrCheck(format, "Mesh index buffer must have been created with a format");
  lovrCheck(format->length > 0, "Mesh index buffer length can not be zero");

  DataType type = format->type;
  if (format->fieldCount > 0 || (type != TYPE_U16 && type != TYPE_U32 && type != TYPE_INDEX16 && type != TYPE_INDEX32)) {
    return lovrSetError("Mesh index buffer must use the u16, u32, index16, or index32 type");
  } else {
    uint32_t stride = (type == TYPE_U16 || type == TYPE_INDEX16) ? 2 : 4;
    lovrCheck(format->stride == stride && format->offset == 0, "Mesh index buffer must be tightly packed");
  }

  lovrRelease(mesh->indexBuffer, lovrBufferDestroy);
  mesh->indexBuffer = buffer;
  mesh->indexCount = format->length;
  lovrRetain(buffer);
  return true;
}

void* lovrMeshGetVertices(Mesh* mesh, uint32_t index, uint32_t count) {
  const DataField* format = lovrMeshGetVertexFormat(mesh);
  if (count == ~0u) count = format->length - index;
  lovrCheck(index < format->length && count <= format->length - index, "Mesh vertex range [%d,%d] overflows mesh capacity", index + 1, index + 1 + count - 1);

  if (mesh->storage == MESH_CPU) {
    return (char*) mesh->vertices + index * format->stride;
  } else {
    return lovrBufferGetData(mesh->vertexBuffer, index * format->stride, count * format->stride);
  }
}

void* lovrMeshSetVertices(Mesh* mesh, uint32_t index, uint32_t count) {
  const DataField* format = lovrMeshGetVertexFormat(mesh);
  if (count == ~0u) count = format->length - index;
  lovrCheck(index < format->length && count <= format->length - index, "Mesh vertex range [%d,%d] overflows mesh capacity", index + 1, index + 1 + count - 1);

  if (mesh->storage == MESH_CPU) {
    mesh->dirtyVertices[0] = MIN(mesh->dirtyVertices[0], index);
    mesh->dirtyVertices[1] = MAX(mesh->dirtyVertices[1], index + count);
    return (char*) mesh->vertices + index * format->stride;
  } else {
    return lovrBufferSetData(mesh->vertexBuffer, index * format->stride, count * format->stride);
  }
}

bool lovrMeshGetIndices(Mesh* mesh, void** indices, uint32_t* count, DataType* type) {
  if (mesh->indexCount == 0 || !mesh->indexBuffer) {
    *indices = NULL;
    return true;
  }

  *count = mesh->indexCount;
  *type = mesh->indexBuffer->info.format->type;

  if (mesh->storage == MESH_CPU) {
    *indices = mesh->indices;
    return true;
  } else {
    *indices = lovrBufferGetData(mesh->indexBuffer, 0, mesh->indexCount * mesh->indexBuffer->info.format->stride);
    return *indices != NULL;
  }
}

void* lovrMeshSetIndices(Mesh* mesh, uint32_t count, DataType type) {
  const DataField* format = mesh->indexBuffer ? mesh->indexBuffer->info.format : NULL;

  if (!mesh->indexBuffer || count > format->length || type != format->type) {
    uint32_t stride = (type == TYPE_U16 || type == TYPE_INDEX16) ? 2 : 4;
    DataField format = { .length = count, .stride = stride, .type = type };
    BufferInfo info = { .format = &format };
    if (mesh->storage == MESH_CPU) {
      Buffer* buffer = lovrBufferCreate(&info, NULL);
      if (!buffer) return NULL;
      lovrRelease(mesh->indexBuffer, lovrBufferDestroy);
      mesh->indexBuffer = buffer;
      mesh->indices = lovrRealloc(mesh->indices, count * stride);
      mesh->indexCount = count;
      mesh->dirtyIndices = true;
      return mesh->indices;
    } else {
      void* data = NULL;
      Buffer* buffer = lovrBufferCreate(&info, &data);
      if (!buffer) return NULL;
      lovrRelease(mesh->indexBuffer, lovrBufferDestroy);
      mesh->indexBuffer = buffer;
      mesh->indexCount = count;
      return data;
    }
  } else if (mesh->storage == MESH_CPU) {
    mesh->indexCount = count;
    mesh->dirtyIndices = true;
    return mesh->indices;
  } else {
    void* data = lovrBufferSetData(mesh->indexBuffer, 0, count * format->stride);
    if (data) mesh->indexCount = count;
    return data;
  }
}

static float* lovrMeshGetPositions(Mesh* mesh) {
  if (mesh->storage == MESH_GPU) return NULL;
  const DataField* format = lovrMeshGetVertexFormat(mesh);
  uint32_t positionHash = (uint32_t) hash64("VertexPosition", strlen("VertexPosition"));
  for (uint32_t i = 0; i < MAX(format->fieldCount, 1); i++) {
    const DataField* attribute = format->fieldCount > 0 ? &format->fields[i] : format;
    if (attribute->type == TYPE_F32x3 && attribute->hash == positionHash) {
      return (float*) ((char*) mesh->vertices + attribute->offset);
    }
  }
  return NULL;
}

bool lovrMeshGetTriangles(Mesh* mesh, float** vertices, uint32_t** indices, uint32_t* vertexCount, uint32_t* indexCount) {
  float* position = lovrMeshGetPositions(mesh);
  const DataField* format = lovrMeshGetVertexFormat(mesh);
  lovrCheck(mesh->storage == MESH_CPU, "Mesh storage mode must be 'cpu'");
  lovrCheck(mesh->mode == DRAW_TRIANGLES, "Mesh draw mode must be 'triangles'");
  lovrCheck(position, "Mesh has no VertexPosition attribute with vec3 type");
  lovrCheck(mesh->indexCount > 0 || format->length % 3 == 0, "Mesh vertex count must be divisible by 3");

  *vertices = lovrMalloc(format->length * 3 * sizeof(float));
  *vertexCount = format->length;

  for (uint32_t i = 0; i < format->length; i++) {
    vec3_init((*vertices) + 3 * i, position);
    position = (float*) ((char*) position + format->stride);
  }

  if (mesh->indexCount > 0) {
    *indexCount = mesh->indexCount;
    *indices = lovrMalloc(*indexCount * sizeof(uint32_t));
    if (mesh->indexBuffer->info.format->type == TYPE_U16 || mesh->indexBuffer->info.format->type == TYPE_INDEX16) {
      for (uint32_t i = 0; i < mesh->indexCount; i++) {
        *indices[i] = (uint32_t) ((uint16_t*) mesh->indices)[i];
      }
    } else {
      memcpy(*indices, mesh->indices, mesh->indexCount * sizeof(uint32_t));
    }
  } else {
    *indexCount = format->length;
    *indices = lovrMalloc(*indexCount * sizeof(uint32_t));
    for (uint32_t i = 0; i < format->length; i++) {
      (*indices)[i] = i;
    }
  }

  return true;
}

bool lovrMeshGetBoundingBox(Mesh* mesh, float box[6]) {
  box[0] = mesh->bounds[0] - mesh->bounds[3];
  box[1] = mesh->bounds[0] + mesh->bounds[3];
  box[2] = mesh->bounds[1] - mesh->bounds[4];
  box[3] = mesh->bounds[1] + mesh->bounds[4];
  box[4] = mesh->bounds[2] - mesh->bounds[5];
  box[5] = mesh->bounds[2] + mesh->bounds[5];
  return mesh->hasBounds;
}

void lovrMeshSetBoundingBox(Mesh* mesh, float box[6]) {
  if (box) {
    mesh->bounds[0] = (box[0] + box[1]) / 2.f;
    mesh->bounds[1] = (box[2] + box[3]) / 2.f;
    mesh->bounds[2] = (box[4] + box[5]) / 2.f;
    mesh->bounds[3] = (box[1] - box[0]) / 2.f;
    mesh->bounds[4] = (box[3] - box[2]) / 2.f;
    mesh->bounds[5] = (box[5] - box[4]) / 2.f;
    mesh->hasBounds = true;
  } else {
    mesh->hasBounds = false;
  }
}

bool lovrMeshComputeBoundingBox(Mesh* mesh) {
  const DataField* format = lovrMeshGetVertexFormat(mesh);
  float* position = lovrMeshGetPositions(mesh);

  if (!position) {
    return false;
  }

  float box[6] = { FLT_MAX, FLT_MIN, FLT_MAX, FLT_MIN, FLT_MAX, FLT_MIN };

  for (uint32_t i = 0; i < format->length; i++, position = (float*) ((char*) position + format->stride)) {
    box[0] = MIN(box[0], position[0]);
    box[1] = MAX(box[1], position[0]);
    box[2] = MIN(box[2], position[1]);
    box[3] = MAX(box[3], position[1]);
    box[4] = MIN(box[4], position[2]);
    box[5] = MAX(box[5], position[2]);
  }

  lovrMeshSetBoundingBox(mesh, box);
  return true;
}

DrawMode lovrMeshGetDrawMode(Mesh* mesh) {
  return mesh->mode;
}

void lovrMeshSetDrawMode(Mesh* mesh, DrawMode mode) {
  mesh->mode = mode;
}

void lovrMeshGetDrawRange(Mesh* mesh, uint32_t* start, uint32_t* count, uint32_t* offset) {
  *start = mesh->drawStart;
  *count = mesh->drawCount;
  *offset = mesh->baseVertex;
}

bool lovrMeshSetDrawRange(Mesh* mesh, uint32_t start, uint32_t count, uint32_t offset) {
  uint32_t vertexCount = mesh->vertexBuffer->info.format->length;
  uint32_t extent = mesh->indexCount > 0 ? mesh->indexCount : vertexCount;
  lovrCheck(start < extent && count <= extent - start, "Invalid draw range [%d,%d]", start + 1, start + 1 + count);
  lovrCheck(offset < vertexCount, "Mesh vertex offset must be less than the vertex count");
  mesh->drawStart = start;
  mesh->drawCount = count;
  mesh->baseVertex = offset;
  return true;
}

Material* lovrMeshGetMaterial(Mesh* mesh) {
  return mesh->material;
}

void lovrMeshSetMaterial(Mesh* mesh, Material* material) {
  lovrRelease(mesh->material, lovrMaterialDestroy);
  mesh->material = material;
  lovrRetain(material);
}

static bool lovrMeshFlush(Mesh* mesh) {
  if (mesh->storage == MESH_GPU) {
    return true;
  }

  if (mesh->dirtyVertices[1] > mesh->dirtyVertices[0]) {
    uint32_t stride = mesh->vertexBuffer->info.format->stride;
    uint32_t offset = mesh->dirtyVertices[0] * stride;
    uint32_t extent = (mesh->dirtyVertices[1] - mesh->dirtyVertices[0]) * stride;
    void* data = lovrBufferSetData(mesh->vertexBuffer, offset, extent);
    if (!data) return false;
    memcpy(data, (char*) mesh->vertices + offset, extent);
    mesh->dirtyVertices[0] = ~0u;
    mesh->dirtyVertices[1] = 0;
  }

  if (mesh->dirtyIndices) {
    uint32_t stride = mesh->indexBuffer->info.format->stride;
    void* data = lovrBufferSetData(mesh->indexBuffer, 0, mesh->indexCount * stride);
    if (!data) return false;
    memcpy(data, mesh->indices, mesh->indexCount * stride);
    mesh->dirtyIndices = false;
  }

  return true;
}

// Model

Model* lovrModelCreate(const ModelInfo* info) {
  ModelData* data = info->data;
  Model* model = lovrCalloc(sizeof(Model));
  model->ref = 1;
  model->info = *info;
  lovrRetain(info->data);

  size_t stack = 0;

  for (uint32_t i = 0; i < data->skinCount; i++) {
    lovrCheckGoto(fail, data->skins[i].jointCount <= 256, "Currently, the max number of joints per skin is 256");
  }

  // Materials and Textures
  if (info->materials) {
    model->textures = lovrCalloc(data->imageCount * sizeof(Texture*));
    model->materials = lovrMalloc(data->materialCount * sizeof(Material*));
    for (uint32_t i = 0; i < data->materialCount; i++) {
      MaterialInfo material;
      ModelMaterial* properties = &data->materials[i];
      memcpy(&material.data, properties, sizeof(MaterialData));

      struct { uint32_t index; Texture** texture; } textures[] = {
        { properties->texture, &material.texture },
        { properties->glowTexture, &material.glowTexture },
        { properties->metalnessTexture, &material.metalnessTexture },
        { properties->roughnessTexture, &material.roughnessTexture },
        { properties->clearcoatTexture, &material.clearcoatTexture },
        { properties->occlusionTexture, &material.occlusionTexture },
        { properties->normalTexture, &material.normalTexture }
      };

      for (uint32_t t = 0; t < COUNTOF(textures); t++) {
        uint32_t index = textures[t].index;
        Texture** texture = textures[t].texture;

        if (index == ~0u) {
          *texture = NULL;
        } else {
          if (!model->textures[index]) {
            model->textures[index] = lovrTextureCreate(&(TextureInfo) {
              .type = TEXTURE_2D,
              .usage = TEXTURE_SAMPLE,
              .format = lovrImageGetFormat(data->images[index]),
              .width = lovrImageGetWidth(data->images[index], 0),
              .height = lovrImageGetHeight(data->images[index], 0),
              .layers = 1,
              .mipmaps = info->mipmaps || lovrImageGetLevelCount(data->images[index]) > 1 ? ~0u : 1,
              .srgb = texture == &material.texture || texture == &material.glowTexture,
              .images = &data->images[index],
              .imageCount = 1
            });

            if (!model->textures[index]) goto fail;
          }

          *texture = model->textures[index];
        }
      }

      model->materials[i] = lovrMaterialCreate(&material);
      lovrAssertGoto(fail, model->materials[i], "Failed to create model material: %s", lovrGetError());
    }
  }

  // Buffers
  char* vertexData = NULL;
  char* indexData = NULL;
  char* blendData = NULL;
  char* skinData = NULL;

  BufferInfo vertexBufferInfo = {
    .format = (DataField[]) {
      { .length = data->vertexCount, .stride = sizeof(ModelVertex), .fieldCount = 5 },
      { .name = "VertexPosition", .type = TYPE_F32x3, .offset = offsetof(ModelVertex, position) },
      { .name = "VertexNormal", .type = TYPE_SN10x3, .offset = offsetof(ModelVertex, normal) },
      { .name = "VertexUV", .type = TYPE_F32x2, .offset = offsetof(ModelVertex, uv) },
      { .name = "VertexColor", .type = TYPE_UN8x4, .offset = offsetof(ModelVertex, color) },
      { .name = "VertexTangent", .type = TYPE_SN10x3, .offset = offsetof(ModelVertex, tangent) }
    }
  };

  if (data->vertexCount > 0) {
    model->vertexBuffer = lovrBufferCreate(&vertexBufferInfo, (void**) &vertexData);
    lovrAssertGoto(fail, model->vertexBuffer, "Failed to create model vertex buffer: %s", lovrGetError());
  }

  if (data->blendShapeVertexCount > 0) {
    model->blendBuffer = lovrBufferCreate(&(BufferInfo) {
      .format = (DataField[]) {
        { .length = data->blendShapeVertexCount, .stride = sizeof(BlendVertex), .fieldCount = 3 },
        { .type = TYPE_F32x3, .offset = offsetof(BlendVertex, position) },
        { .type = TYPE_F32x3, .offset = offsetof(BlendVertex, normal) },
        { .type = TYPE_F32x3, .offset = offsetof(BlendVertex, tangent) }
      }
    }, (void**) &blendData);

    lovrAssertGoto(fail, model->blendBuffer, "Failed to create model blend shape buffer: %s", lovrGetError());
  }

  if (data->skinnedVertexCount > 0) {
    model->skinBuffer = lovrBufferCreate(&(BufferInfo) {
      .format = (DataField[]) {
        { .length = data->skinnedVertexCount, .stride = 8, .fieldCount = 2 },
        { .type = TYPE_UN8x4, .offset = 0 },
        { .type = TYPE_U8x4, .offset = 4 }
      }
    }, (void**) &skinData);

    lovrAssertGoto(fail, model->skinBuffer, "Failed to create model skinning buffer: %s", lovrGetError());
  }

  // Dynamic vertices are ones that are blended or skinned.  They need a copy of the original vertex
  if (data->dynamicVertexCount > 0) {
    vertexBufferInfo.format->length = data->dynamicVertexCount;
    model->rawVertexBuffer = lovrBufferCreate(&vertexBufferInfo, NULL);
    lovrAssertGoto(fail, model->rawVertexBuffer, "Failed to create model raw vertex buffer: %s", lovrGetError());

    if (!beginFrame()) goto fail;

    // The vertex buffer may already have a pending copy if its memory was not host-visible, need to
    // wait for that to complete before copying to the raw vertex buffer
    gpu_barrier barrier = syncTransfer(&model->vertexBuffer->sync, GPU_PHASE_COPY, GPU_CACHE_TRANSFER_READ);
    gpu_sync(state.stream, &barrier, 1);

    Buffer* src = model->vertexBuffer;
    Buffer* dst = model->rawVertexBuffer;
    gpu_copy_buffers(state.stream, src->gpu, dst->gpu, src->base, dst->base, data->dynamicVertexCount * sizeof(ModelVertex));

    gpu_sync(state.stream, &(gpu_barrier) {
      .prev = GPU_PHASE_COPY,
      .next = GPU_PHASE_SHADER_COMPUTE,
      .flush = GPU_CACHE_TRANSFER_WRITE,
      .clear = GPU_CACHE_STORAGE_READ | GPU_CACHE_STORAGE_WRITE
    }, 1);
  }

  DataType indexType = data->indexType == U32 ? TYPE_INDEX32 : TYPE_INDEX16;
  uint32_t indexSize = data->indexType == U32 ? 4 : 2;

  if (data->indexCount > 0) {
    model->indexBuffer = lovrBufferCreate(&(BufferInfo) {
      .format = (DataField[]) {
        { .length = data->indexCount, .stride = indexSize, .type = indexType }
      }
    }, (void**) &indexData);

    lovrAssertGoto(fail, model->indexBuffer, "Failed to create model index buffer: %s", lovrGetError());
  }

  // Primitives are sorted to ensure animated/blended vertices are close together:
  // - Skinned primitives come first
  //   - Within a skin, the primitives with blend shapes come first
  //   - Followed by the primitives without blend shapes
  // - Primitives with blend shapes (but no skinning) are next
  // - Then the remaining "non-dynamic" primitives follow
  // Within each section, primitives are still sorted by their index
  // All of a node's primitives will remain together, since skin/blend shapes are per-node

  stack = tempPush(&state.allocator);
  uint64_t* primitiveOrder = tempAlloc(&state.allocator, data->primitiveCount * sizeof(uint64_t));
  uint32_t* baseVertex = tempAlloc(&state.allocator, data->primitiveCount * sizeof(uint32_t));

  // The sort key only has 31 bits for the skin
  lovrCheckGoto(fail, data->skinCount < (1u << 31), "Too many skins!");

  for (uint32_t i = 0; i < data->primitiveCount; i++) {
    primitiveOrder[i] = 0;
    primitiveOrder[i] |= (uint64_t) data->primitives[i].skin << 33;
    primitiveOrder[i] |= (uint64_t) (data->primitives[i].blendShapeCount == 0) << 32;
    primitiveOrder[i] |= i;
  }

  qsort(primitiveOrder, data->primitiveCount, sizeof(uint64_t), u64cmp);

  // Draws
  model->draws = lovrCalloc(data->primitiveCount * sizeof(DrawInfo));
  model->boundingBoxes = lovrMalloc(data->primitiveCount * 6 * sizeof(float));
  for (uint32_t i = 0, vertexCursor = 0, indexCursor = 0; i < data->primitiveCount; i++) {
    ModelPrimitive* primitive = &data->primitives[primitiveOrder[i] & ~0u];
    ModelAttribute* position = primitive->attributes[ATTR_POSITION];
    DrawInfo* draw = &model->draws[primitiveOrder[i] & ~0u];

    switch (primitive->mode) {
      case DRAW_POINT_LIST: draw->mode = DRAW_POINTS; break;
      case DRAW_LINE_LIST: draw->mode = DRAW_LINES; break;
      case DRAW_TRIANGLE_LIST: draw->mode = DRAW_TRIANGLES; break;
      default: lovrSetError("Model uses an unsupported draw mode (lineloop, linestrip, strip, fan)"); goto fail;
    }

    draw->material = !info->materials || primitive->material == ~0u ? NULL: model->materials[primitive->material];
    draw->vertex.buffer = model->vertexBuffer;

    if (primitive->indices) {
      draw->index.buffer = model->indexBuffer;
      draw->start = indexCursor;
      draw->count = primitive->indices->count;
      draw->baseVertex = vertexCursor;
      indexCursor += draw->count;
    } else {
      draw->start = vertexCursor;
      draw->count = position->count;
    }

    draw->bounds = model->boundingBoxes + i * 6;
    draw->bounds[0] = (position->min[0] + position->max[0]) / 2.f;
    draw->bounds[1] = (position->min[1] + position->max[1]) / 2.f;
    draw->bounds[2] = (position->min[2] + position->max[2]) / 2.f;
    draw->bounds[3] = (position->max[0] - position->min[0]) / 2.f;
    draw->bounds[4] = (position->max[1] - position->min[1]) / 2.f;
    draw->bounds[5] = (position->max[2] - position->min[2]) / 2.f;

    baseVertex[primitiveOrder[i] & ~0u] = vertexCursor;
    vertexCursor += position->count;
  }

  // Vertices
  for (uint32_t i = 0; i < data->primitiveCount; i++) {
    ModelPrimitive* primitive = &data->primitives[primitiveOrder[i] & ~0u];
    ModelAttribute** attributes = primitive->attributes;
    uint32_t count = attributes[ATTR_POSITION]->count;
    size_t stride = sizeof(ModelVertex);

    lovrModelDataCopyAttribute(data, attributes[ATTR_POSITION], vertexData + 0, F32, 3, false, count, stride, 0);
    lovrModelDataCopyAttribute(data, attributes[ATTR_NORMAL], vertexData + 12, SN10x3, 1, false, count, stride, 0);
    lovrModelDataCopyAttribute(data, attributes[ATTR_UV], vertexData + 16, F32, 2, false, count, stride, 0);
    lovrModelDataCopyAttribute(data, attributes[ATTR_COLOR], vertexData + 24, U8, 4, true, count, stride, 255);
    lovrModelDataCopyAttribute(data, attributes[ATTR_TANGENT], vertexData + 28, SN10x3, 1, false, count, stride, 0);
    vertexData += count * stride;

    if (data->skinnedVertexCount > 0 && primitive->skin != ~0u) {
      lovrModelDataCopyAttribute(data, attributes[ATTR_JOINTS], skinData + 0, U8, 4, false, count, 8, 0);
      lovrModelDataCopyAttribute(data, attributes[ATTR_WEIGHTS], skinData + 4, U8, 4, true, count, 8, 0);
      skinData += count * 8;
    }

    if (primitive->indices) {
      char* indices = data->buffers[primitive->indices->buffer].data + primitive->indices->offset;
      memcpy(indexData, indices, primitive->indices->count * indexSize);
      indexData += primitive->indices->count * indexSize;
    }
  }

  // Blend shapes
  if (data->blendShapeCount > 0) {
    for (uint32_t i = 0; i < data->blendShapeCount; i++) {
      if (i == 0 || data->blendShapes[i - 1].node != data->blendShapes[i].node) {
        model->blendGroupCount++;
      }
    }

    model->blendGroups = lovrMalloc(model->blendGroupCount * sizeof(BlendGroup));
    model->blendShapeWeights = lovrMalloc(data->blendShapeCount * sizeof(float));

    BlendGroup* group = model->blendGroups;

    for (uint32_t i = 0; i < data->blendShapeCount; i++) {
      ModelBlendShape* blendShape = &data->blendShapes[i];
      ModelNode* node = &data->nodes[blendShape->node];
      uint32_t groupVertexCount = 0;

      for (uint32_t p = 0; p < node->primitiveCount; p++) {
        ModelPrimitive* primitive = &data->primitives[node->primitiveIndex + p];
        uint32_t vertexCount = primitive->attributes[ATTR_POSITION]->count;
        size_t stride = sizeof(BlendVertex);

        ModelBlendData* blendAttributes = &primitive->blendShapes[i - node->blendShapeIndex];
        lovrModelDataCopyAttribute(data, blendAttributes->positions, blendData + offsetof(BlendVertex, position), F32, 3, false, vertexCount, stride, 0);
        lovrModelDataCopyAttribute(data, blendAttributes->normals, blendData + offsetof(BlendVertex, normal), F32, 3, false, vertexCount, stride, 0);
        lovrModelDataCopyAttribute(data, blendAttributes->tangents, blendData + offsetof(BlendVertex, tangent), F32, 3, false, vertexCount, stride, 0);
        blendData += vertexCount * stride;
        groupVertexCount += vertexCount;
      }

      if (i == 0 || blendShape[-1].node != blendShape[0].node) {
        group->index = node->blendShapeIndex;
        group->count = node->blendShapeCount;
        group->vertexIndex = baseVertex[node->primitiveIndex];
        group->vertexCount = groupVertexCount;
        group++;
      }
    }

    lovrModelResetBlendShapes(model);
  }

  // Transforms
  model->localTransforms = lovrMalloc(sizeof(NodeTransform) * data->nodeCount);
  model->globalTransforms = lovrMalloc(16 * sizeof(float) * data->nodeCount);
  lovrModelResetNodeTransforms(model);

  tempPop(&state.allocator, stack);

  return model;
fail:
  if (stack) tempPop(&state.allocator, stack);
  lovrModelDestroy(model);
  return NULL;
}

Model* lovrModelClone(Model* parent) {
  ModelData* data = parent->info.data;
  Model* model = lovrCalloc(sizeof(Model));
  model->ref = 1;
  model->parent = parent;
  model->info = parent->info;
  lovrRetain(parent);

  model->textures = parent->textures;
  model->materials = parent->materials;

  model->rawVertexBuffer = parent->rawVertexBuffer;
  model->indexBuffer = parent->indexBuffer;
  model->blendBuffer = parent->blendBuffer;
  model->skinBuffer = parent->skinBuffer;

  model->blendGroups = parent->blendGroups;
  model->blendGroupCount = parent->blendGroupCount;

  if (parent->vertexBuffer) {
    model->vertexBuffer = lovrBufferCreate(&parent->vertexBuffer->info, NULL);

    if (!model->vertexBuffer || !beginFrame()) {
      lovrModelDestroy(model);
      return NULL;
    }

    gpu_barrier barrier = syncTransfer(&parent->vertexBuffer->sync, GPU_PHASE_COPY, GPU_CACHE_TRANSFER_READ);
    gpu_sync(state.stream, &barrier, 1);

    Buffer* src = parent->vertexBuffer;
    Buffer* dst = model->vertexBuffer;
    gpu_copy_buffers(state.stream, src->gpu, dst->gpu, src->base, dst->base, parent->vertexBuffer->info.size);

    gpu_sync(state.stream, &(gpu_barrier) {
      .prev = GPU_PHASE_COPY,
      .next = GPU_PHASE_SHADER_COMPUTE,
      .flush = GPU_CACHE_TRANSFER_WRITE,
      .clear = GPU_CACHE_STORAGE_READ | GPU_CACHE_STORAGE_WRITE
    }, 1);
  }

  model->draws = lovrMalloc(data->primitiveCount * sizeof(DrawInfo));

  for (uint32_t i = 0; i < data->primitiveCount; i++) {
    model->draws[i] = parent->draws[i];
    model->draws[i].vertex.buffer = model->vertexBuffer;
  }

  model->blendShapeWeights = lovrMalloc(data->blendShapeCount * sizeof(float));
  lovrModelResetBlendShapes(model);

  model->localTransforms = lovrMalloc(sizeof(NodeTransform) * data->nodeCount);
  model->globalTransforms = lovrMalloc(16 * sizeof(float) * data->nodeCount);
  lovrModelResetNodeTransforms(model);

  return model;
}

void lovrModelDestroy(void* ref) {
  Model* model = ref;
  if (model->parent) {
    lovrRelease(model->parent, lovrModelDestroy);
    lovrRelease(model->vertexBuffer, lovrBufferDestroy);
    lovrFree(model->localTransforms);
    lovrFree(model->globalTransforms);
    lovrFree(model->blendShapeWeights);
    lovrFree(model->meshes);
    lovrFree(model->draws);
    lovrFree(model);
    return;
  }
  ModelData* data = model->info.data;
  if (model->info.materials) {
    for (uint32_t i = 0; i < data->materialCount; i++) {
      lovrRelease(model->materials[i], lovrMaterialDestroy);
    }
    for (uint32_t i = 0; i < data->imageCount; i++) {
      lovrRelease(model->textures[i], lovrTextureDestroy);
    }
    lovrFree(model->materials);
    lovrFree(model->textures);
  }
  lovrRelease(model->rawVertexBuffer, lovrBufferDestroy);
  lovrRelease(model->vertexBuffer, lovrBufferDestroy);
  lovrRelease(model->indexBuffer, lovrBufferDestroy);
  lovrRelease(model->blendBuffer, lovrBufferDestroy);
  lovrRelease(model->skinBuffer, lovrBufferDestroy);
  lovrRelease(model->info.data, lovrModelDataDestroy);
  lovrFree(model->localTransforms);
  lovrFree(model->globalTransforms);
  lovrFree(model->boundingBoxes);
  lovrFree(model->blendShapeWeights);
  lovrFree(model->blendGroups);
  lovrFree(model->meshes);
  lovrFree(model->draws);
  lovrFree(model);
}

const ModelInfo* lovrModelGetInfo(Model* model) {
  return &model->info;
}

void lovrModelResetNodeTransforms(Model* model) {
  ModelData* data = model->info.data;
  for (uint32_t i = 0; i < data->nodeCount; i++) {
    NodeTransform* transform = &model->localTransforms[i];
    if (data->nodes[i].hasMatrix) {
      mat4_getPosition(data->nodes[i].transform.matrix, transform->position);
      mat4_getOrientation(data->nodes[i].transform.matrix, transform->rotation);
      mat4_getScale(data->nodes[i].transform.matrix, transform->scale);
    } else {
      vec3_init(transform->position, data->nodes[i].transform.translation);
      quat_init(transform->rotation, data->nodes[i].transform.rotation);
      vec3_init(transform->scale, data->nodes[i].transform.scale);
    }
  }
  model->transformsDirty = true;
}

void lovrModelResetBlendShapes(Model* model) {
  ModelData* data = model->info.data;
  for (uint32_t i = 0; i < data->blendShapeCount; i++) {
    model->blendShapeWeights[i] = data->blendShapes[i].weight;
  }
  model->blendShapesDirty = true;
}

bool lovrModelAnimate(Model* model, uint32_t animationIndex, float time, float alpha) {
  if (alpha <= 0.f) return true;

  ModelData* data = model->info.data;
  lovrCheck(animationIndex < data->animationCount, "Invalid animation index '%d' (Model has %d animation%s)", animationIndex + 1, data->animationCount, data->animationCount == 1 ? "" : "s");
  ModelAnimation* animation = &data->animations[animationIndex];
  time = fmodf(time, animation->duration);

  size_t stack = tempPush(&state.allocator);

  for (uint32_t i = 0; i < animation->channelCount; i++) {
    ModelAnimationChannel* channel = &animation->channels[i];
    uint32_t node = channel->nodeIndex;

    uint32_t keyframe = 0;
    while (keyframe < channel->keyframeCount && channel->times[keyframe] < time) {
      keyframe++;
    }

    size_t n;
    switch (channel->property) {
      case PROP_TRANSLATION: n = 3; break;
      case PROP_SCALE: n = 3; break;
      case PROP_ROTATION: n = 4; break;
      case PROP_WEIGHTS: n = data->nodes[node].blendShapeCount; break;
    }

    float* property = tempAlloc(&state.allocator, n * sizeof(float));

    // Handle the first/last keyframe case (no interpolation)
    if (keyframe == 0 || keyframe >= channel->keyframeCount) {
      size_t index = MIN(keyframe, channel->keyframeCount - 1);

      // For cubic interpolation, each keyframe has 3 parts, and the actual data is in the middle
      if (channel->smoothing == SMOOTH_CUBIC) {
        index = 3 * index + 1;
      }

      memcpy(property, channel->data + index * n, n * sizeof(float));
    } else {
      float t1 = channel->times[keyframe - 1];
      float t2 = channel->times[keyframe];
      float z = (time - t1) / (t2 - t1);

      switch (channel->smoothing) {
        case SMOOTH_STEP:
          memcpy(property, channel->data + (z >= .5f ? keyframe : keyframe - 1) * n, n * sizeof(float));
          break;
        case SMOOTH_LINEAR:
          memcpy(property, channel->data + (keyframe - 1) * n, n * sizeof(float));
          if (channel->property == PROP_ROTATION) {
            quat_slerp(property, channel->data + keyframe * n, z);
          } else {
            float* target = channel->data + keyframe * n;
            for (uint32_t i = 0; i < n; i++) {
              property[i] += (target[i] - property[i]) * z;
            }
          }
          break;
        case SMOOTH_CUBIC: {
          size_t stride = 3 * n;
          float* p0 = channel->data + (keyframe - 1) * stride + 1 * n;
          float* m0 = channel->data + (keyframe - 1) * stride + 2 * n;
          float* p1 = channel->data + (keyframe - 0) * stride + 1 * n;
          float* m1 = channel->data + (keyframe - 0) * stride + 0 * n;
          float dt = t2 - t1;
          float z2 = z * z;
          float z3 = z2 * z;
          float a = 2.f * z3 - 3.f * z2 + 1.f;
          float b = 2.f * z3 - 3.f * z2 + 1.f;
          float c = -2.f * z3 + 3.f * z2;
          float d = (z3 * -z2) * dt;
          for (size_t j = 0; j < n; j++) {
            property[j] = a * p0[j] + b * m0[j] + c * p1[j] + d * m1[j];
          }
          break;
        }
        default: break;
      }
    }

    if (channel->property == PROP_WEIGHTS) {
      model->blendShapesDirty = true;
    } else {
      model->transformsDirty = true;
    }

    float* dst;
    switch (channel->property) {
      case PROP_TRANSLATION: dst = model->localTransforms[node].position; break;
      case PROP_SCALE: dst = model->localTransforms[node].scale; break;
      case PROP_ROTATION: dst = model->localTransforms[node].rotation; break;
      case PROP_WEIGHTS: dst = &model->blendShapeWeights[data->nodes[node].blendShapeIndex]; break;
    }

    if (alpha >= 1.f) {
      memcpy(dst, property, n * sizeof(float));
    } else {
      for (uint32_t i = 0; i < n; i++) {
        dst[i] += (property[i] - dst[i]) * alpha;
      }
    }
  }

  tempPop(&state.allocator, stack);
  return true;
}

float lovrModelGetBlendShapeWeight(Model* model, uint32_t index) {
  return model->blendShapeWeights[index];
}

void lovrModelSetBlendShapeWeight(Model* model, uint32_t index, float weight) {
  model->blendShapeWeights[index] = weight;
  model->blendShapesDirty = true;
}

void lovrModelGetNodeTransform(Model* model, uint32_t node, float position[3], float scale[3], float rotation[4], OriginType origin) {
  if (origin == ORIGIN_PARENT) {
    vec3_init(position, model->localTransforms[node].position);
    vec3_init(scale, model->localTransforms[node].scale);
    quat_init(rotation, model->localTransforms[node].rotation);
  } else {
    if (model->transformsDirty) {
      updateModelTransforms(model, model->info.data->rootNode, (float[]) MAT4_IDENTITY);
      model->transformsDirty = false;
    }
    mat4_getPosition(model->globalTransforms + 16 * node, position);
    mat4_getScale(model->globalTransforms + 16 * node, scale);
    mat4_getOrientation(model->globalTransforms + 16 * node, rotation);
  }
}

void lovrModelSetNodeTransform(Model* model, uint32_t node, float position[3], float scale[3], float rotation[4], float alpha) {
  if (alpha <= 0.f) return;

  NodeTransform* transform = &model->localTransforms[node];

  if (alpha >= 1.f) {
    if (position) vec3_init(transform->position, position);
    if (scale) vec3_init(transform->scale, scale);
    if (rotation) quat_init(transform->rotation, rotation);
  } else {
    if (position) vec3_lerp(transform->position, position, alpha);
    if (scale) vec3_lerp(transform->scale, scale, alpha);
    if (rotation) quat_slerp(transform->rotation, rotation, alpha);
  }

  model->transformsDirty = true;
}

Buffer* lovrModelGetVertexBuffer(Model* model) {
  return model->rawVertexBuffer;
}

Buffer* lovrModelGetIndexBuffer(Model* model) {
  return model->indexBuffer;
}

Mesh* lovrModelGetMesh(Model* model, uint32_t index) {
  ModelData* data = model->info.data;
  lovrCheck(index < data->primitiveCount, "Invalid mesh index '%d' (Model has %d mesh%s)", index + 1, data->primitiveCount, data->primitiveCount == 1 ? "" : "es");

  if (!model->meshes) {
    model->meshes = lovrCalloc(data->primitiveCount * sizeof(Mesh*));
  }

  if (!model->meshes[index]) {
    DrawInfo* draw = &model->draws[index];
    MeshInfo info = { .vertexBuffer = model->vertexBuffer, .storage = MESH_GPU };
    Mesh* mesh = lovrMeshCreate(&info, NULL);
    if (!mesh) return NULL;
    if (draw->index.buffer) lovrMeshSetIndexBuffer(mesh, model->indexBuffer);
    lovrMeshSetDrawMode(mesh, draw->mode);
    lovrMeshSetDrawRange(mesh, draw->start, draw->count, draw->baseVertex);
    lovrMeshSetMaterial(mesh, draw->material);
    memcpy(mesh->bounds, draw->bounds, sizeof(mesh->bounds));
    mesh->hasBounds = true;
    model->meshes[index] = mesh;
  }

  return model->meshes[index];
}

Texture* lovrModelGetTexture(Model* model, uint32_t index) {
  ModelData* data = model->info.data;
  lovrCheck(index < data->imageCount, "Invalid texture index '%d' (Model has %d texture%s)", index + 1, data->imageCount, data->imageCount == 1 ? "" : "s");
  return model->textures[index];
}

Material* lovrModelGetMaterial(Model* model, uint32_t index) {
  ModelData* data = model->info.data;
  lovrCheck(index < data->materialCount, "Invalid material index '%d' (Model has %d material%s)", index + 1, data->materialCount, data->materialCount == 1 ? "" : "s");
  return model->materials[index];
}

static bool lovrModelAnimateVertices(Model* model) {
  ModelData* data = model->info.data;

  bool blend = model->blendGroupCount > 0;
  bool skin = data->skinCount > 0;

  if (!beginFrame()) return false;

  if ((!blend && !skin) || (!model->transformsDirty && !model->blendShapesDirty) || model->lastVertexAnimation == state.tick) {
    return true;
  }

  if (model->transformsDirty) {
    updateModelTransforms(model, model->info.data->rootNode, (float[]) MAT4_IDENTITY);
    model->transformsDirty = false;
  }

  gpu_compute_begin(state.stream);

  if (blend) {
    Shader* shader = lovrGraphicsGetDefaultShader(SHADER_BLENDER);
    uint32_t vertexCount = data->dynamicVertexCount;
    uint32_t blendBufferCursor = 0;
    uint32_t chunkSize = 64;

    if (!shader) return false;

    gpu_binding bindings[] = {
      { 0, GPU_SLOT_STORAGE_BUFFER, .buffer = { model->rawVertexBuffer->gpu, model->rawVertexBuffer->base, vertexCount * sizeof(ModelVertex) } },
      { 1, GPU_SLOT_STORAGE_BUFFER, .buffer = { model->vertexBuffer->gpu, model->vertexBuffer->base, vertexCount * sizeof(ModelVertex) } },
      { 2, GPU_SLOT_STORAGE_BUFFER, .buffer = { model->blendBuffer->gpu, model->blendBuffer->base, model->blendBuffer->info.size } },
      { 3, GPU_SLOT_UNIFORM_BUFFER, .buffer = { NULL, 0, chunkSize * sizeof(float) } }
    };

    gpu_bind_pipeline(state.stream, shader->computePipeline, GPU_PIPELINE_COMPUTE);

    for (uint32_t i = 0; i < model->blendGroupCount; i++) {
      BlendGroup* group = &model->blendGroups[i];

      for (uint32_t j = 0; j < group->count; j += chunkSize) {
        uint32_t count = MIN(group->count - j, chunkSize);
        bool inplace = j > 0;

        BufferView view = getBuffer(GPU_BUFFER_STREAM, chunkSize * sizeof(float), state.limits.uniformBufferAlign);
        if (!view.buffer) return false;
        memcpy(view.pointer, model->blendShapeWeights + group->index + j, count * sizeof(float));
        bindings[3].buffer = (gpu_buffer_binding) { view.buffer, view.offset, view.extent };

        gpu_bundle* bundle = getBundle(shader->layout, bindings, COUNTOF(bindings));
        if (!bundle) return false;
        uint32_t constants[] = { group->vertexIndex, group->vertexCount, count, blendBufferCursor, inplace };
        uint32_t subgroupSize = state.device.subgroupSize;

        gpu_bind_bundles(state.stream, shader->gpu, &bundle, 0, 1, NULL, 0);
        gpu_push_constants(state.stream, shader->gpu, constants, sizeof(constants));
        gpu_compute(state.stream, (group->vertexCount + subgroupSize - 1) / subgroupSize, 1, 1);

        if (j + count < group->count) {
          gpu_sync(state.stream, &(gpu_barrier) {
            .prev = GPU_PHASE_SHADER_COMPUTE,
            .next = GPU_PHASE_SHADER_COMPUTE,
            .flush = GPU_CACHE_STORAGE_WRITE,
            .clear = GPU_CACHE_STORAGE_READ
          }, 1);
        }

        blendBufferCursor += group->vertexCount * count;
      }
    }

    model->blendShapesDirty = false;

    if (skin) {
      gpu_sync(state.stream, &(gpu_barrier) {
        .prev = GPU_PHASE_SHADER_COMPUTE,
        .next = GPU_PHASE_SHADER_COMPUTE,
        .flush = GPU_CACHE_STORAGE_WRITE,
        .clear = GPU_CACHE_STORAGE_READ | GPU_CACHE_STORAGE_WRITE
      }, 1);
    }
  }

  if (skin) {
    Shader* shader = lovrGraphicsGetDefaultShader(SHADER_ANIMATOR);
    if (!shader) return false;

    gpu_binding bindings[] = {
      { 0, GPU_SLOT_STORAGE_BUFFER, .buffer = { model->rawVertexBuffer->gpu, model->rawVertexBuffer->base, data->skinnedVertexCount * sizeof(ModelVertex) } },
      { 1, GPU_SLOT_STORAGE_BUFFER, .buffer = { model->vertexBuffer->gpu, model->vertexBuffer->base, data->skinnedVertexCount * sizeof(ModelVertex) } },
      { 2, GPU_SLOT_STORAGE_BUFFER, .buffer = { model->skinBuffer->gpu, model->skinBuffer->base, data->skinnedVertexCount * 8 } },
      { 3, GPU_SLOT_UNIFORM_BUFFER, .buffer = { NULL, 0, 0 } } // Filled in for each skin
    };

    gpu_bind_pipeline(state.stream, shader->computePipeline, GPU_PIPELINE_COMPUTE);

    for (uint32_t i = 0, baseVertex = 0; i < data->skinCount; i++) {
      ModelSkin* skin = &data->skins[i];

      uint32_t align = state.limits.uniformBufferAlign;
      BufferView view = getBuffer(GPU_BUFFER_STREAM, skin->jointCount * 16 * sizeof(float), align);
      if (!view.buffer) return false;
      bindings[3].buffer = (gpu_buffer_binding) { view.buffer, view.offset, view.extent };

      float transform[16];
      float* joints = view.pointer;
      for (uint32_t j = 0; j < skin->jointCount; j++) {
        mat4_init(transform, model->globalTransforms + 16 * skin->joints[j]);
        mat4_mul(transform, skin->inverseBindMatrices + 16 * j);
        memcpy(joints, transform, sizeof(transform));
        joints += 16;
      }

      gpu_bundle* bundle = getBundle(shader->layout, bindings, COUNTOF(bindings));
      if (!bundle) return false;
      gpu_bind_bundles(state.stream, shader->gpu, &bundle, 0, 1, NULL, 0);

      // Animate in 2 passes: first animate vertices with blend shapes, then animate the rest
      // We do this because the source buffer is different for blended vs. unblended vertices:
      // - Unblended vertices should animate the original vertices from rawVertexBuffer
      // - Blended vertices should animate the post-blended vertices in-place
      for (uint32_t j = 0; j < 2; j++) {
        uint32_t subgroupSize = state.device.subgroupSize; 
        uint32_t verticesRemaining = j == 0 ? skin->blendedVertexCount : (skin->vertexCount - skin->blendedVertexCount);
        uint32_t maxVerticesPerDispatch = (uint32_t) MIN((uint64_t) state.limits.workgroupCount[0] * subgroupSize, (uint64_t) verticesRemaining);

        while (verticesRemaining > 0) {
          uint32_t vertexCount = MIN(verticesRemaining, maxVerticesPerDispatch);
          gpu_push_constants(state.stream, shader->gpu, (uint32_t[3]) { baseVertex, vertexCount, j == 0 }, 12);
          gpu_compute(state.stream, (vertexCount + subgroupSize - 1) / subgroupSize, 1, 1);
          verticesRemaining -= vertexCount;
          baseVertex += vertexCount;
        }
      }
    }
  }

  gpu_compute_end(state.stream);

  state.barrier.prev |= GPU_PHASE_SHADER_COMPUTE;
  state.barrier.next |= GPU_PHASE_INPUT_VERTEX;
  state.barrier.flush |= GPU_CACHE_STORAGE_WRITE;
  state.barrier.clear |= GPU_CACHE_VERTEX;
  model->lastVertexAnimation = state.tick;
  return true;
}

// Readback

static Readback* lovrReadbackCreate(ReadbackType type) {
  Readback* readback = lovrCalloc(sizeof(Readback));
  readback->ref = 1;
  readback->tick = state.tick;
  readback->type = type;
  if (!state.oldestReadback) state.oldestReadback = readback;
  if (state.newestReadback) state.newestReadback->next = readback;
  state.newestReadback = readback;
  lovrRetain(readback);
  return readback;
}

Readback* lovrReadbackCreateBuffer(Buffer* buffer, uint32_t offset, uint32_t extent) {
  if (!beginFrame()) return NULL;
  if (extent == ~0u) extent = buffer->info.size - offset;
  lovrCheck(offset + extent <= buffer->info.size, "Tried to read past the end of the Buffer");
  lovrCheck(!buffer->info.format || offset % buffer->info.format->stride == 0, "Readback offset must be a multiple of Buffer's stride");
  lovrCheck(!buffer->info.format || extent % buffer->info.format->stride == 0, "Readback size must be a multiple of Buffer's stride");
  BufferView view = getBuffer(GPU_BUFFER_DOWNLOAD, extent, 4);
  if (!view.buffer) return NULL;
  Readback* readback = lovrReadbackCreate(READBACK_BUFFER);
  readback->buffer = buffer;
  void* data = lovrMalloc(extent);
  readback->blob = lovrBlobCreate(data, extent, "Readback");
  readback->view = view;
  lovrRetain(buffer);
  gpu_barrier barrier = syncTransfer(&buffer->sync, GPU_PHASE_COPY, GPU_CACHE_TRANSFER_READ);
  gpu_sync(state.stream, &barrier, 1);
  gpu_copy_buffers(state.stream, buffer->gpu, readback->view.buffer, buffer->base + offset, readback->view.offset, extent);
  return readback;
}

Readback* lovrReadbackCreateTexture(Texture* texture, uint32_t offset[4], uint32_t extent[3]) {
  if (!beginFrame()) return NULL;
  if (extent[0] == ~0u) extent[0] = texture->info.width - offset[0];
  if (extent[1] == ~0u) extent[1] = texture->info.height - offset[1];
  lovrCheck(extent[2] == 1, "Currently, only one layer can be read from a Texture");
  lovrCheck(texture->root == texture, "Can not read from a Texture view");
  lovrCheck(texture->info.usage & TEXTURE_TRANSFER, "Texture must be created with the 'transfer' usage to read from it");
  checkTextureBounds(&texture->info, offset, extent);
  BufferView view = getBuffer(GPU_BUFFER_DOWNLOAD, measureTexture(texture->info.format, extent[0], extent[1], 1), 64);
  if (!view.buffer) return NULL;
  Image* image = lovrImageCreateRaw(extent[0], extent[1], texture->info.format, texture->info.srgb);
  lovrAssert(image, "Failed to create image: %s", lovrGetError());
  Readback* readback = lovrReadbackCreate(READBACK_TEXTURE);
  readback->texture = texture;
  readback->image = image;
  readback->view = view;
  lovrRetain(texture);
  gpu_barrier barrier = syncTransfer(&texture->sync, GPU_PHASE_COPY, GPU_CACHE_TRANSFER_READ);
  gpu_sync(state.stream, &barrier, 1);
  gpu_copy_texture_buffer(state.stream, texture->gpu, readback->view.buffer, offset, readback->view.offset, extent);
  return readback;
}

static Readback* lovrReadbackCreateTimestamp(TimingInfo* times, uint32_t count, BufferView buffer) {
  Readback* readback = lovrReadbackCreate(READBACK_TIMESTAMP);
  readback->view = buffer;
  readback->times = times;
  readback->count = count;
  return readback;
}

void lovrReadbackDestroy(void* ref) {
  Readback* readback = ref;
  switch (readback->type) {
    case READBACK_BUFFER:
      lovrRelease(readback->buffer, lovrBufferDestroy);
      lovrRelease(readback->blob, lovrBlobDestroy);
      break;
    case READBACK_TEXTURE:
      lovrRelease(readback->texture, lovrTextureDestroy);
      lovrRelease(readback->image, lovrImageDestroy);
      break;
    case READBACK_TIMESTAMP:
      for (uint32_t i = 0; i < readback->count; i++) {
        lovrRelease(readback->times[i].pass, lovrPassDestroy);
      }
      lovrFree(readback->times);
      break;
    default: break;
  }
  lovrFree(readback);
}

bool lovrReadbackIsComplete(Readback* readback) {
  return gpu_is_complete(readback->tick);
}

bool lovrReadbackWait(Readback* readback, bool* waited) {
  if (lovrReadbackIsComplete(readback)) {
    *waited = false;
    return true;
  }

  if (readback->tick == state.tick && state.active) {
    if (!lovrGraphicsSubmit(NULL, 0)) {
      return false;
    }
  }

  if (!beginFrame()) {
    return false;
  }

  if (!gpu_wait_tick(readback->tick, waited)) {
    return lovrSetError("Failed to wait for readback: %s", gpu_get_error());
  }

  if (*waited) {
    processReadbacks();
  }

  return true;
}

void* lovrReadbackGetData(Readback* readback, DataField** format, uint32_t* count) {
  if (!lovrReadbackIsComplete(readback)) return NULL;

  if (readback->type == READBACK_BUFFER && readback->buffer->info.format) {
    *format = readback->buffer->info.format;
    *count = (uint32_t) (readback->blob->size / readback->buffer->info.format->stride);
    return readback->blob->data;
  }

  return NULL;
}

Blob* lovrReadbackGetBlob(Readback* readback) {
  return lovrReadbackIsComplete(readback) ? readback->blob : NULL;
}

Image* lovrReadbackGetImage(Readback* readback) {
  return lovrReadbackIsComplete(readback) ? readback->image : NULL;
}

// Pass

static void* lovrPassAllocate(Pass* pass, size_t size) {
  return tempAlloc(&pass->allocator, size);
}

static BufferView lovrPassGetBuffer(Pass* pass, uint32_t size, size_t align) {
  return allocateBuffer(&pass->buffers, GPU_BUFFER_STREAM, size, align);
}

static void lovrPassRelease(Pass* pass) {
  // Chain all of the Pass's full buffers onto the end of the global freelist
  if (pass->buffers.freelist) {
    BufferBlock** list = &state.bufferAllocators[GPU_BUFFER_STREAM].freelist;
    while (*list) list = (BufferBlock**) &(*list)->next;
    *list = pass->buffers.freelist;
    pass->buffers.freelist = NULL;
  }

  if (pass->pipeline) {
    for (uint32_t i = 0; i <= pass->pipelineIndex; i++) {
      lovrRelease(pass->pipeline->material, lovrMaterialDestroy);
      lovrRelease(pass->pipeline->shader, lovrShaderDestroy);
      lovrRelease(pass->pipeline->font, lovrFontDestroy);
      pass->pipeline--;
    }

    pass->pipelineIndex = 0;
  }

  lovrRelease(pass->sampler, lovrSamplerDestroy);

  for (uint32_t i = 0; i < pass->computeCount; i++) {
    lovrRelease(pass->computes[i].shader, lovrShaderDestroy);
  }

  for (uint32_t i = 0; i < pass->drawCount; i++) {
    Draw* draw = &pass->draws[i];
    lovrRelease(draw->shader, lovrShaderDestroy);
    lovrRelease(draw->material, lovrMaterialDestroy);
  }

  for (uint32_t i = 0; i < COUNTOF(pass->access); i++) {
    for (AccessBlock* block = pass->access[i]; block != NULL; block = block->next) {
      for (uint32_t j = 0; j < block->count; j++) {
        bool texture = block->textureMask & (1ull << j);
        lovrRelease(block->list[j].object, texture ? lovrTextureDestroy : lovrBufferDestroy);
      }
    }
  }
}

bool lovrGraphicsGetWindowPass(Pass** pass) {
  if (!state.windowPass) {
    state.windowPass = lovrPassCreate("Window");
  }

  Texture* textures[4] = { 0 };
  if (!lovrGraphicsGetWindowTexture(&textures[0])) {
    *pass = NULL;
    return false;
  }

  // The window may become unavailable during a resize
  if (!textures[0]) {
    *pass = NULL;
    return true;
  }

  lovrPassReset(state.windowPass);
  memcpy(state.windowPass->canvas.color[0].clear, state.background, 4 * sizeof(float));
  lovrPassSetCanvas(state.windowPass, textures, NULL, state.depthFormat, state.config.antialias ? 4 : 1);
  *pass = state.windowPass;
  return true;
}

Pass* lovrPassCreate(const char* label) {
  Pass* pass = lovrCalloc(sizeof(Pass));
  pass->ref = 1;

  pass->allocator.limit = 1 << 28;
  pass->allocator.length = 1 << 12;
  pass->allocator.memory = os_vm_init(pass->allocator.limit);
  lovrAssert(pass->allocator.memory, "Out of memory");
  os_vm_commit(pass->allocator.memory, pass->allocator.length);

  if (label) {
    size_t size = strlen(label) + 1;
    pass->label = lovrMalloc(size);
    memcpy(pass->label, label, size);
  }

  lovrPassReset(pass);
  return pass;
}

void lovrPassDestroy(void* ref) {
  Pass* pass = ref;
  lovrPassRelease(pass);
  for (uint32_t i = 0; i < COUNTOF(pass->canvas.color); i++) {
    lovrRelease(pass->canvas.color[i].texture, lovrTextureDestroy);
  }
  lovrRelease(pass->canvas.depth.texture, lovrTextureDestroy);
  lovrRelease(pass->tally.buffer, lovrBufferDestroy);
  if (pass->tally.gpu) {
    gpu_tally_destroy(pass->tally.gpu);
    lovrRelease(pass->tally.tempBuffer, lovrBufferDestroy);
  }
  if (pass->buffers.current) {
    pass->buffers.current->tick = state.tick;
    freeBlock(&state.bufferAllocators[GPU_BUFFER_STREAM], pass->buffers.current);
  }
  os_vm_free(pass->allocator.memory, pass->allocator.limit);
  lovrFree(pass->label);
  lovrFree(pass);
}

void lovrPassReset(Pass* pass) {
  lovrPassRelease(pass);

  pass->allocator.cursor = 0;
  pass->access[ACCESS_RENDER] = NULL;
  pass->access[ACCESS_COMPUTE] = NULL;
  pass->flags = DIRTY_BINDINGS;
  pass->transform = lovrPassAllocate(pass, TRANSFORM_STACK_SIZE * 16 * sizeof(float));
  pass->pipeline = lovrPassAllocate(pass, PIPELINE_STACK_SIZE * sizeof(Pipeline));
  pass->bindings = lovrPassAllocate(pass, 32 * sizeof(gpu_binding));
  pass->uniforms = NULL;
  pass->computeCount = 0;
  pass->computes = NULL;
  pass->drawCount = 0;
  pass->draws = lovrPassAllocate(pass, pass->drawCapacity * sizeof(Draw));

  memset(&pass->geocache, 0, sizeof(pass->geocache));

  pass->tally.active = false;
  pass->tally.count = 0;

  pass->transformIndex = 0;
  mat4_identity(pass->transform);

  pass->pipelineIndex = 0;
  memset(pass->pipeline, 0, sizeof(Pipeline));
  pass->pipeline->mode = DRAW_TRIANGLES;
  pass->pipeline->lastVertexFormat = ~0u;
  pass->pipeline->color[0] = 1.f;
  pass->pipeline->color[1] = 1.f;
  pass->pipeline->color[2] = 1.f;
  pass->pipeline->color[3] = 1.f;
  pass->pipeline->info.pass = pass->gpu;
  pass->pipeline->info.depth.test = GPU_COMPARE_GEQUAL;
  pass->pipeline->info.depth.write = true;
  pass->pipeline->info.stencil.testMask = 0xff;
  pass->pipeline->info.stencil.writeMask = 0xff;

  for (uint32_t i = 0; i < 4; i++) {
    lovrPassSetBlendMode(pass, i, BLEND_ALPHA, BLEND_ALPHA_MULTIPLY);
    pass->pipeline->info.colorMask[i] = 0xf;
  }

  Canvas* canvas = &pass->canvas;

  pass->cameraCount = 0;
  pass->viewportCount = 0;
  pass->scissorCount = 0;

  if (canvas->views > 0) {
    float viewMatrix[16];
    float projection[16];
    mat4_identity(viewMatrix);
    mat4_perspective(projection, 1.2f, (float) canvas->width / canvas->height, .01f, 0.f);
    for (uint32_t i = 0; i < canvas->views; i++) {
      lovrPassSetViewMatrix(pass, i, viewMatrix);
      lovrPassSetProjection(pass, i, projection);
    }
  }

  lovrPassSetViewport(pass, (float[6]) { 0.f, 0.f, (float) canvas->width, (float) canvas->height, 0.f, 1.f });
  lovrPassSetScissor(pass, (uint32_t[4]) { 0, 0, canvas->width, canvas->height });

  pass->sampler = NULL;
}

const PassStats* lovrPassGetStats(Pass* pass) {
  pass->stats.draws = pass->drawCount;
  pass->stats.computes = pass->computeCount;
  pass->stats.cpuMemoryReserved = pass->allocator.length;
  pass->stats.cpuMemoryUsed = pass->allocator.cursor;
  return &pass->stats;
}

const char* lovrPassGetLabel(Pass* pass) {
  return pass->label;
}

void lovrPassGetCanvas(Pass* pass, Texture* textures[4], Texture** depthTexture, uint32_t* depthFormat, uint32_t* samples) {
  for (uint32_t i = 0; i < COUNTOF(pass->canvas.color); i++) {
    textures[i] = pass->canvas.color[i].texture;
  }
  *depthTexture = pass->canvas.depth.texture;
  *depthFormat = pass->canvas.depth.format;
  *samples = pass->canvas.samples;
}

bool lovrPassSetCanvas(Pass* pass, Texture* textures[4], Texture* depthTexture, uint32_t depthFormat, uint32_t samples) {
  Canvas* canvas = &pass->canvas;

  for (uint32_t i = 0; i < canvas->count; i++) {
    lovrRelease(canvas->color[i].texture, lovrTextureDestroy);
    canvas->color[i].texture = NULL;
  }

  lovrRelease(canvas->depth.texture, lovrTextureDestroy);
  canvas->depth.texture = NULL;
  canvas->depth.format = 0;

  canvas->count = 0;
  canvas->width = 0;
  canvas->height = 0;
  canvas->views = 0;
  canvas->samples = 0;
  canvas->resolve = false;

  pass->gpu = NULL;
  lovrPassReset(pass);

  if ((!textures || !textures[0]) && !depthTexture) {
    return true;
  }

  const TextureInfo* t = textures && textures[0] ? &textures[0]->info : &depthTexture->info;

  lovrCheck(t->width <= state.limits.renderSize[0], "Pass canvas width (%d) exceeds the renderSize limit of this GPU (%d)", t->width, state.limits.renderSize[0]);
  lovrCheck(t->height <= state.limits.renderSize[1], "Pass canvas height (%d) exceeds the renderSize limit of this GPU (%d)", t->height, state.limits.renderSize[1]);
  lovrCheck(t->layers <= state.limits.renderSize[2], "Pass canvas layer count (%d) exceeds the renderSize limit of this GPU (%d)", t->layers, state.limits.renderSize[2]);
  lovrCheck(samples == 1 || samples == 4, "Currently MSAA must be 1 or 4");

  uint32_t count = 0;
  for (uint32_t i = 0; i < COUNTOF(canvas->color) && textures && textures[i]; i++, count++) {
    const TextureInfo* texture = &textures[i]->info;
    bool renderable = texture->format == GPU_FORMAT_SURFACE || (state.features.formats[texture->format][texture->srgb] & GPU_FEATURE_RENDER);
    lovrCheck(!isDepthFormat(texture->format), "Unable to use a depth texture as a color target");
    lovrCheck(renderable, "This GPU does not support rendering to the texture format/encoding used by canvas texture #%d", i + 1);
    lovrCheck(texture->usage & TEXTURE_RENDER, "Texture must be created with the 'render' flag to render to it");
    lovrCheck(texture->width == t->width, "Canvas texture sizes must match");
    lovrCheck(texture->height == t->height, "Canvas texture sizes must match");
    lovrCheck(texture->layers == t->layers, "Canvas texture layer counts must match");
  }

  if (depthTexture) {
    const TextureInfo* texture = &depthTexture->info;
    lovrCheck(isDepthFormat(texture->format), "Canvas depth textures must have a depth format");
    lovrCheck(texture->usage & TEXTURE_RENDER, "Texture must be created with the 'render' flag to render to it");
    lovrCheck(texture->width == t->width, "Canvas texture sizes must match");
    lovrCheck(texture->height == t->height, "Canvas texture sizes must match");
    lovrCheck(texture->layers == t->layers, "Canvas texture layer counts must match");
    lovrCheck(samples == 1 || state.features.depthResolve, "This GPU does not support resolving depth textures, MSAA should be set to 1");
  } else if (depthFormat) {
    lovrCheck(isDepthFormat(depthFormat), "Expected depth format for canvas depth (received color format)");
    lovrCheck(state.features.formats[depthFormat][0] & GPU_FEATURE_RENDER, "Canvas depth format is not supported by this GPU");
  }

  canvas->count = count;
  canvas->width = t->width;
  canvas->height = t->height;
  canvas->views = t->layers;
  canvas->samples = samples;
  canvas->resolve = samples > 1;

  for (uint32_t i = 0; i < canvas->count; i++) {
    canvas->color[i].texture = textures[i];
    lovrRetain(textures[i]);
  }
  canvas->depth.texture = depthTexture;
  canvas->depth.format = depthTexture ? depthTexture->info.format : depthFormat;
  lovrRetain(depthTexture);

  pass->gpu = getPass(canvas);

  if (!pass->gpu) {
    return lovrPassSetCanvas(pass, NULL, NULL, 0, 0);
  }

  lovrPassReset(pass);
  return true;
}

void lovrPassGetClear(Pass* pass, LoadAction loads[4], float clears[4][4], LoadAction* depthLoad, float* depthClear) {
  for (uint32_t i = 0; i < pass->canvas.count; i++) {
    loads[i] = pass->canvas.color[i].load;
    if (pass->canvas.color[i].load == LOAD_CLEAR) {
      clears[i][0] = lovrMathLinearToGamma(pass->canvas.color[i].clear[0]);
      clears[i][1] = lovrMathLinearToGamma(pass->canvas.color[i].clear[1]);
      clears[i][2] = lovrMathLinearToGamma(pass->canvas.color[i].clear[2]);
      clears[i][3] = pass->canvas.color[i].clear[3];
    }
  }
  *depthLoad = pass->canvas.depth.load;
  *depthClear = pass->canvas.depth.clear;
}

bool lovrPassSetClear(Pass* pass, LoadAction loads[4], float clears[4][4], LoadAction depthLoad, float depthClear) {
  bool dirty = false;
  for (uint32_t i = 0; i < pass->canvas.count; i++) {
    dirty |= loads[i] != pass->canvas.color[i].load;
    pass->canvas.color[i].load = loads[i];
    if (loads[i] == LOAD_CLEAR) {
      pass->canvas.color[i].clear[0] = lovrMathGammaToLinear(clears[i][0]);
      pass->canvas.color[i].clear[1] = lovrMathGammaToLinear(clears[i][1]);
      pass->canvas.color[i].clear[2] = lovrMathGammaToLinear(clears[i][2]);
      pass->canvas.color[i].clear[3] = clears[i][3];
    } else {
      memset(pass->canvas.color[i].clear, 0, 4 * sizeof(float));
    }
  }
  dirty |= depthLoad != pass->canvas.depth.load;
  pass->canvas.depth.load = depthLoad;
  pass->canvas.depth.clear = depthLoad == LOAD_CLEAR ? depthClear : 0.f;
  if (dirty) return (pass->gpu = getPass(&pass->canvas)) != NULL;
  return true;
}

uint32_t lovrPassGetAttachmentCount(Pass* pass, bool* depth) {
  if (depth) *depth = pass->canvas.depth.texture || pass->canvas.depth.format;
  return pass->canvas.count;
}

uint32_t lovrPassGetWidth(Pass* pass) {
  return pass->canvas.width;
}

uint32_t lovrPassGetHeight(Pass* pass) {
  return pass->canvas.height;
}

uint32_t lovrPassGetViewCount(Pass* pass) {
  return pass->canvas.views;
}

static Camera* getCamera(Pass* pass) {
  if (pass->flags & DIRTY_CAMERA) {
    return pass->cameras + (pass->cameraCount - 1) * pass->canvas.views;
  }

  uint32_t views = pass->canvas.views;
  uint32_t stride = sizeof(Camera) * views;
  uint32_t count = pass->cameraCount;
  Camera* cameras = lovrPassAllocate(pass, (count + 1) * stride);
  Camera* newCamera = cameras + count * views;
  if (pass->cameras) memcpy(cameras, pass->cameras, count * stride);
  memcpy(newCamera, newCamera - views, count > 0 ? stride : 0);
  pass->flags |= DIRTY_CAMERA;
  pass->cameras = cameras;
  pass->cameraCount++;
  return newCamera;
}

bool lovrPassGetViewMatrix(Pass* pass, uint32_t index, float viewMatrix[16]) {
  lovrCheck(index < pass->canvas.views, "Invalid view index '%d'", index + 1);
  mat4_init(viewMatrix, getCamera(pass)[index].viewMatrix);
  return true;
}

bool lovrPassSetViewMatrix(Pass* pass, uint32_t index, float viewMatrix[16]) {
  lovrCheck(index < pass->canvas.views, "Invalid view index '%d'", index + 1);
  mat4_init(getCamera(pass)[index].viewMatrix, viewMatrix);
  return true;
}

bool lovrPassGetProjection(Pass* pass, uint32_t index, float projection[16]) {
  lovrCheck(index < pass->canvas.views, "Invalid view index '%d'", index + 1);
  mat4_init(projection, getCamera(pass)[index].projection);
  return true;
}

bool lovrPassSetProjection(Pass* pass, uint32_t index, float projection[16]) {
  lovrCheck(index < pass->canvas.views, "Invalid view index '%d'", index + 1);
  mat4_init(getCamera(pass)[index].projection, projection);
  return true;
}

bool lovrPassPush(Pass* pass, StackType stack) {
  switch (stack) {
    case STACK_TRANSFORM:
      lovrCheck(++pass->transformIndex < TRANSFORM_STACK_SIZE, "%s stack overflow (more pushes than pops?)", "Transform");
      mat4_init(pass->transform + 16, pass->transform);
      pass->transform += 16;
      return true;
    case STACK_STATE:
      lovrCheck(++pass->pipelineIndex < PIPELINE_STACK_SIZE, "%s stack overflow (more pushes than pops?)", "Pipeline");
      memcpy(pass->pipeline + 1, pass->pipeline, sizeof(Pipeline));
      pass->pipeline++;
      lovrRetain(pass->pipeline->font);
      lovrRetain(pass->pipeline->shader);
      lovrRetain(pass->pipeline->material);
      return true;
    default: return true;
  }
}

bool lovrPassPop(Pass* pass, StackType stack) {
  switch (stack) {
    case STACK_TRANSFORM:
      lovrCheck(--pass->transformIndex < TRANSFORM_STACK_SIZE, "%s stack underflow (more pops than pushes?)", "Transform");
      pass->transform -= 16;
      return true;
    case STACK_STATE:
      lovrRelease(pass->pipeline->font, lovrFontDestroy);
      lovrRelease(pass->pipeline->shader, lovrShaderDestroy);
      lovrRelease(pass->pipeline->material, lovrMaterialDestroy);
      lovrCheck(--pass->pipelineIndex < PIPELINE_STACK_SIZE, "%s stack underflow (more pops than pushes?)", "Pipeline");
      pass->pipeline--;
      pass->pipeline->dirty = true;
      return true;
    default: return true;
  }
}

void lovrPassOrigin(Pass* pass) {
  mat4_identity(pass->transform);
}

void lovrPassTranslate(Pass* pass, vec3 translation) {
  mat4_translate(pass->transform, translation[0], translation[1], translation[2]);
}

void lovrPassRotate(Pass* pass, quat rotation) {
  mat4_rotateQuat(pass->transform, rotation);
}

void lovrPassScale(Pass* pass, vec3 scale) {
  mat4_scale(pass->transform, scale[0], scale[1], scale[2]);
}

void lovrPassTransform(Pass* pass, mat4 transform) {
  mat4_mul(pass->transform, transform);
}

void lovrPassSetAlphaToCoverage(Pass* pass, bool enabled) {
  pass->pipeline->dirty |= enabled != pass->pipeline->info.multisample.alphaToCoverage;
  pass->pipeline->info.multisample.alphaToCoverage = enabled;
}

void lovrPassSetBlendMode(Pass* pass, uint32_t index, BlendMode mode, BlendAlphaMode alphaMode) {
  if (mode == BLEND_NONE) {
    pass->pipeline->dirty |= pass->pipeline->info.blend[index].enabled;
    memset(&pass->pipeline->info.blend[index], 0, sizeof(gpu_blend_state));
    return;
  }

  gpu_blend_state* blend = &pass->pipeline->info.blend[index];

  static const gpu_blend_state table[] = {
    [BLEND_ALPHA] = {
      .color = { GPU_BLEND_SRC_ALPHA, GPU_BLEND_ONE_MINUS_SRC_ALPHA, GPU_BLEND_ADD },
      .alpha = { GPU_BLEND_ONE, GPU_BLEND_ONE_MINUS_SRC_ALPHA, GPU_BLEND_ADD }
    },
    [BLEND_ADD] = {
      .color = { GPU_BLEND_SRC_ALPHA, GPU_BLEND_ONE, GPU_BLEND_ADD },
      .alpha = { GPU_BLEND_ZERO, GPU_BLEND_ONE, GPU_BLEND_ADD }
    },
    [BLEND_SUBTRACT] = {
      .color = { GPU_BLEND_SRC_ALPHA, GPU_BLEND_ONE, GPU_BLEND_RSUB },
      .alpha = { GPU_BLEND_ZERO, GPU_BLEND_ONE, GPU_BLEND_RSUB }
    },
    [BLEND_MULTIPLY] = {
      .color = { GPU_BLEND_DST_COLOR, GPU_BLEND_ZERO, GPU_BLEND_ADD },
      .alpha = { GPU_BLEND_DST_COLOR, GPU_BLEND_ZERO, GPU_BLEND_ADD }
    },
    [BLEND_LIGHTEN] = {
      .color = { GPU_BLEND_SRC_ALPHA, GPU_BLEND_ZERO, GPU_BLEND_MAX },
      .alpha = { GPU_BLEND_ONE, GPU_BLEND_ZERO, GPU_BLEND_MAX }
    },
    [BLEND_DARKEN] = {
      .color = { GPU_BLEND_SRC_ALPHA, GPU_BLEND_ZERO, GPU_BLEND_MIN },
      .alpha = { GPU_BLEND_ONE, GPU_BLEND_ZERO, GPU_BLEND_MIN }
    },
    [BLEND_SCREEN] = {
      .color = { GPU_BLEND_SRC_ALPHA, GPU_BLEND_ONE_MINUS_SRC_COLOR, GPU_BLEND_ADD },
      .alpha = { GPU_BLEND_ONE, GPU_BLEND_ONE_MINUS_SRC_COLOR, GPU_BLEND_ADD }
    },
  };

  *blend = table[mode];
  blend->enabled = true;

  if (alphaMode == BLEND_PREMULTIPLIED && mode != BLEND_MULTIPLY) {
    blend->color.src = GPU_BLEND_ONE;
  }

  pass->pipeline->dirty = true;
}

void lovrPassSetColor(Pass* pass, float color[4]) {
  pass->pipeline->color[0] = lovrMathGammaToLinear(color[0]);
  pass->pipeline->color[1] = lovrMathGammaToLinear(color[1]);
  pass->pipeline->color[2] = lovrMathGammaToLinear(color[2]);
  pass->pipeline->color[3] = color[3];
}

void lovrPassSetColorWrite(Pass* pass, uint32_t index, bool r, bool g, bool b, bool a) {
  uint8_t mask = (r << 0) | (g << 1) | (b << 2) | (a << 3);
  pass->pipeline->dirty |= pass->pipeline->info.colorMask[index] != mask;
  pass->pipeline->info.colorMask[index] = mask;
}

void lovrPassSetDepthTest(Pass* pass, CompareMode test) {
  pass->pipeline->dirty |= pass->pipeline->info.depth.test != (gpu_compare_mode) test;
  pass->pipeline->info.depth.test = (gpu_compare_mode) test;
}

void lovrPassSetDepthWrite(Pass* pass, bool write) {
  pass->pipeline->dirty |= pass->pipeline->info.depth.write != write;
  pass->pipeline->info.depth.write = write;
}

void lovrPassSetDepthOffset(Pass* pass, float offset, float sloped) {
  pass->pipeline->info.rasterizer.depthOffset = offset;
  pass->pipeline->info.rasterizer.depthOffsetSloped = sloped;
  pass->pipeline->dirty = true;
}

void lovrPassSetDepthClamp(Pass* pass, bool clamp) {
  if (state.features.depthClamp) {
    pass->pipeline->dirty |= pass->pipeline->info.rasterizer.depthClamp != clamp;
    pass->pipeline->info.rasterizer.depthClamp = clamp;
  }
}

void lovrPassSetFaceCull(Pass* pass, CullMode mode) {
  pass->pipeline->dirty |= pass->pipeline->info.rasterizer.cullMode != (gpu_cull_mode) mode;
  pass->pipeline->info.rasterizer.cullMode = (gpu_cull_mode) mode;
}

void lovrPassSetFont(Pass* pass, Font* font) {
  if (pass->pipeline->font != font) {
    lovrRetain(font);
    lovrRelease(pass->pipeline->font, lovrFontDestroy);
    pass->pipeline->font = font;
  }
}

void lovrPassSetMaterial(Pass* pass, Material* material) {
  if (!material) material = state.defaultMaterial;

  if (pass->pipeline->material != material) {
    lovrRetain(material);
    lovrRelease(pass->pipeline->material, lovrMaterialDestroy);
    pass->pipeline->material = material;
  }
}

void lovrPassSetMeshMode(Pass* pass, DrawMode mode) {
  pass->pipeline->mode = mode;
}

void lovrPassSetSampler(Pass* pass, Sampler* sampler) {
  if (sampler != pass->sampler) {
    lovrRetain(sampler);
    lovrRelease(pass->sampler, lovrSamplerDestroy);
    pass->sampler = sampler;
  }
}

void lovrPassSetScissor(Pass* pass, uint32_t scissor[4]) {
  if (~pass->flags & DIRTY_SCISSOR) {
    uint32_t* scissors = lovrPassAllocate(pass, (pass->scissorCount + 1) * 4 * sizeof(uint32_t));
    if (pass->scissors) memcpy(scissors, pass->scissors, pass->scissorCount * 4 * sizeof(uint32_t));
    pass->flags |= DIRTY_SCISSOR;
    pass->scissors = scissors;
    pass->scissorCount++;
  }

  uint32_t* s = pass->scissors + (pass->scissorCount - 1) * 4;

  if (scissor) {
    memcpy(s, scissor, 4 * sizeof(uint32_t));
  } else {
    s[0] = 0;
    s[1] = 0;
    s[2] = pass->canvas.width;
    s[3] = pass->canvas.height;
  }
}

void lovrPassSetShader(Pass* pass, Shader* shader) {
  Shader* old = pass->pipeline->shader;

  if (shader == old) {
    return;
  }

  if (shader) {
    gpu_binding bindings[32];

    // Ensure there's a valid binding for every resource in the new shader.  If the old shader had a
    // binding with the same name and type, then use that, otherwise use a "default" resource.
    for (uint32_t i = 0; i < shader->resourceCount; i++) {
      ShaderResource* resource = &shader->resources[i];
      bool useDefault = true;

      if (old) {
        ShaderResource* other = old->resources;
        for (uint32_t j = 0; j < old->resourceCount; j++, other++) {
          if (other->hash == resource->hash && other->type == resource->type) {
            bindings[resource->binding] = pass->bindings[other->binding];
            useDefault = false;
            break;
          }
        }
      }

      if (useDefault) {
        switch (resource->type) {
          case GPU_SLOT_UNIFORM_BUFFER:
          case GPU_SLOT_STORAGE_BUFFER:
            bindings[i].buffer.object = state.defaultBuffer->gpu;
            bindings[i].buffer.offset = state.defaultBuffer->base;
            bindings[i].buffer.extent = state.defaultBuffer->info.size;
            break;
          case GPU_SLOT_TEXTURE_WITH_SAMPLER:
          case GPU_SLOT_SAMPLED_TEXTURE:
          case GPU_SLOT_STORAGE_TEXTURE:
          case GPU_SLOT_SAMPLER:
            bindings[i].texture.object = state.defaultTexture->gpu;
            bindings[i].texture.sampler = state.defaultSamplers[FILTER_LINEAR]->gpu;
            break;
          default: break;
        }
      }
    }

    memcpy(pass->bindings, bindings, shader->resourceCount * sizeof(gpu_binding));
    pass->flags |= DIRTY_BINDINGS;

    // Uniform data is preserved for uniforms with the same name/size (this might be slow...)
    if (shader->uniformCount > 0) {
      void* uniforms = lovrPassAllocate(pass, shader->uniformSize);

      if (old && old->uniformCount > 0) {
        for (uint32_t i = 0; i < shader->uniformCount; i++) {
          DataField* uniform = &shader->uniforms[i];
          DataField* other = old->uniforms;
          for (uint32_t j = 0; j < old->uniformCount; j++, other++) {
            if (uniform->hash == other->hash && uniform->stride == other->stride && uniform->length == other->length) {
              void* src = (char*) pass->uniforms + other->offset;
              void* dst = (char*) uniforms + uniform->offset;
              size_t size = uniform->stride * MAX(uniform->length, 1);
              memcpy(dst, src, size);
              break;
            }
          }
        }
      } else {
        memset(uniforms, 0, shader->uniformSize);
      }

      pass->uniforms = uniforms;
      pass->flags |= DIRTY_UNIFORMS;
    }

    // Custom vertex attributes must be reset: their locations may differ even if the names match
    if (shader->hasCustomAttributes) {
      pass->pipeline->lastVertexBuffer = NULL;
    }

    pass->pipeline->info.shader = shader->gpu;
    pass->pipeline->info.flags = shader->flags;
    pass->pipeline->info.flagCount = shader->overrideCount;
    lovrRetain(shader);
  }

  lovrRelease(old, lovrShaderDestroy);
  pass->pipeline->shader = shader;
  pass->pipeline->dirty = true;
}

bool lovrPassSetStencilTest(Pass* pass, CompareMode test, uint8_t value, uint8_t mask) {
  TextureFormat depthFormat = pass->canvas.depth.texture ? pass->canvas.depth.texture->info.format : pass->canvas.depth.format;
  lovrCheck(depthFormat == FORMAT_D32FS8 || depthFormat == FORMAT_D24S8, "Trying to set stencil mode, but Pass depth texture does not use a stencil format");
  bool hasReplace = false;
  hasReplace |= pass->pipeline->info.stencil.failOp == GPU_STENCIL_REPLACE;
  hasReplace |= pass->pipeline->info.stencil.depthFailOp == GPU_STENCIL_REPLACE;
  hasReplace |= pass->pipeline->info.stencil.passOp == GPU_STENCIL_REPLACE;
  if (hasReplace && test != COMPARE_NONE) {
    lovrCheck(value == pass->pipeline->info.stencil.value, "When stencil write is 'replace' and stencil test is active, their values must match");
  }
  switch (test) { // (Reversed compare mode)
    case COMPARE_NONE: default: pass->pipeline->info.stencil.test = GPU_COMPARE_NONE; break;
    case COMPARE_EQUAL: pass->pipeline->info.stencil.test = GPU_COMPARE_EQUAL; break;
    case COMPARE_NEQUAL: pass->pipeline->info.stencil.test = GPU_COMPARE_NEQUAL; break;
    case COMPARE_LESS: pass->pipeline->info.stencil.test = GPU_COMPARE_GREATER; break;
    case COMPARE_LEQUAL: pass->pipeline->info.stencil.test = GPU_COMPARE_GEQUAL; break;
    case COMPARE_GREATER: pass->pipeline->info.stencil.test = GPU_COMPARE_LESS; break;
    case COMPARE_GEQUAL: pass->pipeline->info.stencil.test = GPU_COMPARE_LEQUAL; break;
  }
  pass->pipeline->info.stencil.testMask = mask;
  if (test != COMPARE_NONE) pass->pipeline->info.stencil.value = value;
  pass->pipeline->dirty = true;
  return true;
}

bool lovrPassSetStencilWrite(Pass* pass, StencilAction actions[3], uint8_t value, uint8_t mask) {
  TextureFormat depthFormat = pass->canvas.depth.texture ? pass->canvas.depth.texture->info.format : pass->canvas.depth.format;
  lovrCheck(depthFormat == FORMAT_D32FS8 || depthFormat == FORMAT_D24S8, "Trying to set stencil mode, but Pass depth texture does not use a stencil format");
  bool hasReplace = actions[0] == STENCIL_REPLACE || actions[1] == STENCIL_REPLACE || actions[2] == STENCIL_REPLACE;
  if (hasReplace && pass->pipeline->info.stencil.test != GPU_COMPARE_NONE) {
    lovrCheck(value == pass->pipeline->info.stencil.value, "When stencil write is 'replace' and stencil test is active, their values must match");
  }
  pass->pipeline->info.stencil.failOp = (gpu_stencil_op) actions[0];
  pass->pipeline->info.stencil.depthFailOp = (gpu_stencil_op) actions[1];
  pass->pipeline->info.stencil.passOp = (gpu_stencil_op) actions[2];
  pass->pipeline->info.stencil.writeMask = mask;
  if (hasReplace) pass->pipeline->info.stencil.value = value;
  pass->pipeline->dirty = true;
  return true;
}

void lovrPassSetViewCull(Pass* pass, bool enable) {
  pass->pipeline->viewCull = enable;
}

void lovrPassSetViewport(Pass* pass, float viewport[6]) {
  if (~pass->flags & DIRTY_VIEWPORT) {
    float* viewports = lovrPassAllocate(pass, (pass->viewportCount + 1) * 6 * sizeof(float));
    if (pass->viewports) memcpy(viewports, pass->viewports, pass->viewportCount * 6 * sizeof(float));
    pass->flags |= DIRTY_VIEWPORT;
    pass->viewports = viewports;
    pass->viewportCount++;
  }

  float* v = pass->viewports + (pass->viewportCount - 1) * 6;

  if (viewport) {
    memcpy(v, viewport, 6 * sizeof(float));
  } else {
    v[0] = 0;
    v[1] = 0;
    v[2] = (float) pass->canvas.width;
    v[3] = (float) pass->canvas.height;
    v[4] = 0.f;
    v[5] = 1.f;
  }
}

void lovrPassSetWinding(Pass* pass, Winding winding) {
  pass->pipeline->dirty |= pass->pipeline->info.rasterizer.winding != (gpu_winding) winding;
  pass->pipeline->info.rasterizer.winding = (gpu_winding) winding;
}

void lovrPassSetWireframe(Pass* pass, bool wireframe) {
  if (state.features.wireframe) {
    pass->pipeline->dirty |= pass->pipeline->info.rasterizer.wireframe != (gpu_winding) wireframe;
    pass->pipeline->info.rasterizer.wireframe = wireframe;
  }
}

bool lovrPassSendBuffer(Pass* pass, const char* name, size_t length, Buffer* buffer, uint32_t offset, uint32_t extent) {
  Shader* shader = pass->pipeline->shader;
  lovrCheck(shader, "A Shader must be active to send resources");

  ShaderResource* resource = findShaderResource(shader, name, length);
  lovrCheck(resource, "Shader has no variable named '%s'", name);
  uint32_t slot = resource->binding;

  lovrCheck(shader->bufferMask & (1u << slot), "Trying to send a Buffer to '%s', but the active Shader doesn't have a Buffer in that slot", name);
  lovrCheck(offset < buffer->info.size, "Buffer offset is past the end of the Buffer");

  uint32_t limit;

  if (shader->storageMask & (1u << slot)) {
    lovrCheck((offset & (state.limits.storageBufferAlign - 1)) == 0, "Storage buffer offset (%d) is not aligned to storageBufferAlign limit (%d)", offset, state.limits.storageBufferAlign);
    limit = state.limits.storageBufferRange;
  } else {
    lovrCheck((offset & (state.limits.uniformBufferAlign - 1)) == 0, "Uniform buffer offset (%d) is not aligned to uniformBufferAlign limit (%d)", offset, state.limits.uniformBufferAlign);
    limit = state.limits.uniformBufferRange;
  }

  if (extent == 0) {
    extent = MIN(buffer->info.size - offset, limit);
  } else {
    lovrCheck(offset + extent <= buffer->info.size, "Buffer range goes past the end of the Buffer");
    lovrCheck(extent <= limit, "Buffer range exceeds storageBufferRange/uniformBufferRange limit");
  }

  trackBuffer(pass, buffer, resource->phase, resource->cache);
  pass->bindings[slot].buffer.object = buffer->gpu;
  pass->bindings[slot].buffer.offset = buffer->base + offset;
  pass->bindings[slot].buffer.extent = extent;
  pass->flags |= DIRTY_BINDINGS;
  return true;
}

bool lovrPassSendTexture(Pass* pass, const char* name, size_t length, Texture* texture) {
  Shader* shader = pass->pipeline->shader;
  lovrCheck(shader, "A Shader must be active to send resources");

  ShaderResource* resource = findShaderResource(shader, name, length);
  lovrCheck(resource, "Shader has no variable named '%s'", name);
  uint32_t slot = resource->binding;

  lovrCheck(shader->textureMask & (1u << slot), "Trying to send a Texture to '%s', but the active Shader doesn't have a Texture in that slot", name);

  gpu_texture* view;
  if (shader->storageMask & (1u << slot)) {
    lovrCheck(texture->info.usage & TEXTURE_STORAGE, "Textures must be created with the 'storage' usage to send them to image variables in shaders");
    view = texture->storageView;
  } else {
    lovrCheck(texture->info.usage & TEXTURE_SAMPLE, "Textures must be created with the 'sample' usage to send them to sampler variables in shaders");
    view = texture->sampleView;
  }

  trackTexture(pass, texture, resource->phase, resource->cache);
  pass->bindings[slot].texture.object = view;
  pass->bindings[slot].texture.sampler = texture->sampler ? texture->sampler->gpu : state.defaultSamplers[FILTER_LINEAR]->gpu;
  pass->flags |= DIRTY_BINDINGS;
  return true;
}

bool lovrPassSendSampler(Pass* pass, const char* name, size_t length, Sampler* sampler) {
  Shader* shader = pass->pipeline->shader;
  lovrCheck(shader, "A Shader must be active to send resources");

  ShaderResource* resource = findShaderResource(shader, name, length);
  lovrCheck(resource, "Shader has no variable named '%s'", name);
  uint32_t slot = resource->binding;

  lovrCheck(shader->samplerMask & (1u << slot), "Trying to send a Sampler to '%s', but the active Shader doesn't have a Sampler in that slot", name);

  pass->bindings[slot].texture.object = state.defaultTexture->gpu;
  pass->bindings[slot].texture.sampler = sampler->gpu;
  pass->flags |= DIRTY_BINDINGS;
  return true;
}

bool lovrPassSendData(Pass* pass, const char* name, size_t length, void** data, DataField** format) {
  Shader* shader = pass->pipeline->shader;
  lovrCheck(shader, "A Shader must be active to send data to it");

  uint32_t hash = (uint32_t) hash64(name, length);
  for (uint32_t i = 0; i < shader->uniformCount; i++) {
    if (shader->uniforms[i].hash == hash) {
      *data = (char*) pass->uniforms + shader->uniforms[i].offset;
      *format = &shader->uniforms[i];
      pass->flags |= DIRTY_UNIFORMS;
      return true;
    }
  }

  ShaderResource* resource = findShaderResource(shader, name, length);
  lovrCheck(resource, "Shader has no variable named '%s'", name);
  uint32_t slot = resource->binding;

  lovrCheck(shader->bufferMask & (1u << slot), "Trying to send data to '%s', but that slot isn't a Buffer", name);
  lovrCheck(~shader->storageMask & (1u << slot), "Unable to send table data to a storage buffer");

  uint32_t size = resource->format->stride * MAX(resource->format->length, 1);
  BufferView view = lovrPassGetBuffer(pass, size, state.limits.uniformBufferAlign);
  if (!view.buffer) return false;
  pass->bindings[slot].buffer = (gpu_buffer_binding) { view.buffer, view.offset, view.extent };
  pass->flags |= DIRTY_BINDINGS;

  *data = view.pointer;
  *format = resource->format;
  return true;
}

static void lovrPassResolvePipeline(Pass* pass, DrawInfo* info, Draw* draw, Draw* prev) {
  Pipeline* pipeline = pass->pipeline;
  Shader* shader = draw->shader;

  if (pipeline->info.drawMode != (gpu_draw_mode) info->mode) {
    pipeline->info.drawMode = (gpu_draw_mode) info->mode;
    pipeline->dirty = true;
  }

  if (!pipeline->shader && pipeline->info.shader != shader->gpu) {
    pipeline->info.shader = shader->gpu;
    pipeline->info.flags = NULL;
    pipeline->info.flagCount = 0;
    pipeline->dirty = true;
  }

  // Vertex formats
  if (info->vertex.buffer && pipeline->lastVertexBuffer != info->vertex.buffer) {
    pipeline->lastVertexFormat = ~0u;
    pipeline->lastVertexBuffer = info->vertex.buffer;
    pipeline->dirty = true;

    const DataField* format = info->vertex.buffer->info.format;
    const DataField* fields = format->fieldCount > 0 ? format->fields : format;
    uint32_t fieldCount = MAX(format->fieldCount, 1);

    pipeline->info.vertex.bufferCount = 2;
    pipeline->info.vertex.attributeCount = shader->attributeCount;
    pipeline->info.vertex.bufferStrides[0] = format->stride;
    pipeline->info.vertex.bufferStrides[1] = 0;

    for (uint32_t i = 0; i < shader->attributeCount; i++) {
      ShaderAttribute* attribute = &shader->attributes[i];
      bool found = false;

      for (uint32_t j = 0; j < fieldCount; j++) {
        if (fields[j].hash == attribute->hash) {
          pipeline->info.vertex.attributes[i] = (gpu_attribute) {
            .buffer = 0,
            .location = attribute->location,
            .offset = fields[j].offset,
            .type = fields[j].type
          };
          found = true;
          break;
        }
      }

      if (!found) {
        pipeline->info.vertex.attributes[i] = (gpu_attribute) {
          .buffer = 1,
          .location = attribute->location,
          .offset = attribute->location == LOCATION_COLOR ? 16 : 0,
          .type = GPU_TYPE_F32x4
        };
      }
    }
  } else if (!info->vertex.buffer && pipeline->lastVertexFormat != info->vertex.format) {
    pipeline->lastVertexFormat = info->vertex.format;
    pipeline->lastVertexBuffer = NULL;
    pipeline->info.vertex = state.vertexFormats[info->vertex.format];
    pipeline->dirty = true;

    if (shader->hasCustomAttributes) {
      for (uint32_t i = 0; i < shader->attributeCount; i++) {
        if (shader->attributes[i].location < 10) {
          pipeline->info.vertex.attributes[pipeline->info.vertex.attributeCount++] = (gpu_attribute) {
            .buffer = 1,
            .location = shader->attributes[i].location,
            .type = GPU_TYPE_F32x4,
            .offset = shader->attributes[i].location == LOCATION_COLOR ? 16 : 0
          };
        }
      }
    }
  }

  if (pipeline->dirty) {
    pipeline->dirty = false;
    draw->pipelineInfo = lovrPassAllocate(pass, sizeof(gpu_pipeline_info));
    memcpy(draw->pipelineInfo, &pipeline->info, sizeof(pipeline->info));
    draw->pipeline = NULL;
  } else {
    draw->pipelineInfo = prev->pipelineInfo;
    draw->pipeline = prev->pipeline;
  }
}

static gpu_bundle_info* lovrPassResolveBindings(Pass* pass, Shader* shader, gpu_bundle_info* previous) {
  if (shader->resourceCount == 0) {
    return NULL;
  }

  if (~pass->flags & DIRTY_BINDINGS) {
    return previous;
  }

  gpu_bundle_info* bundle = lovrPassAllocate(pass, sizeof(gpu_bundle_info));
  bundle->bindings = lovrPassAllocate(pass, shader->resourceCount * sizeof(gpu_binding));
  bundle->layout = shader->layout->gpu;
  bundle->count = shader->resourceCount;

  for (uint32_t i = 0; i < bundle->count; i++) {
    bundle->bindings[i] = pass->bindings[shader->resources[i].binding];
    bundle->bindings[i].type = shader->resources[i].type;
    bundle->bindings[i].number = shader->resources[i].binding;
    bundle->bindings[i].count = 0;
  }

  pass->flags &= ~DIRTY_BINDINGS;
  return bundle;
}

static bool lovrPassResolveUniforms(Pass* pass, Shader* shader, gpu_buffer** buffer, uint32_t* offset, void* previous) {
  if (shader->uniformCount == 0) {
    *buffer = NULL;
    *offset = 0;
  } else if (pass->flags & DIRTY_UNIFORMS) {
    BufferView view = lovrPassGetBuffer(pass, shader->uniformSize, state.limits.uniformBufferAlign);
    if (!view.buffer) return false;
    memcpy(view.pointer, pass->uniforms, shader->uniformSize);
    *buffer = view.buffer;
    *offset = view.offset;
    pass->flags &= ~DIRTY_UNIFORMS;
  } else if (previous) {
    bool graphics = shader->info.type == SHADER_GRAPHICS;
    *buffer = graphics ? ((Draw*) previous)->uniformBuffer : ((Compute*) previous)->uniformBuffer;
    *offset = graphics ? ((Draw*) previous)->uniformOffset : ((Compute*) previous)->uniformOffset;
  } else {
    *buffer = NULL;
    *offset = 0;
  }

  return true;
}

static bool lovrPassResolveVertices(Pass* pass, DrawInfo* info, Draw* draw) {
  CachedShape* cached = info->hash ? &pass->geocache[info->hash & (COUNTOF(pass->geocache) - 1)] : NULL;

  if (cached && cached->hash == info->hash) {
    draw->vertexBuffer = cached->vertexBuffer;
    draw->indexBuffer = cached->indexBuffer;
    draw->start = cached->start;
    draw->baseVertex = cached->baseVertex;
    draw->vertexBufferOffset = cached->vertexBufferOffset;
    *info->vertex.pointer = NULL;
    *info->index.pointer = NULL;
    return true;
  }

  if (!info->vertex.buffer && info->vertex.count > 0) {
    lovrCheck(info->vertex.count <= UINT16_MAX, "Shape has too many vertices (max is 65535)");
    uint32_t stride = state.vertexFormats[info->vertex.format].bufferStrides[0];
    BufferView view = lovrPassGetBuffer(pass, info->vertex.count * stride, stride);
    if (!view.buffer) return false;
    *info->vertex.pointer = view.pointer;
    draw->vertexBuffer = view.buffer;
    draw->vertexBufferOffset = view.offset;
  } else if (info->vertex.buffer) {
    Buffer* buffer = info->vertex.buffer;
    uint32_t stride = buffer->info.format->stride;
    lovrCheck(stride <= state.limits.vertexBufferStride, "Vertex buffer stride exceeds vertexBufferStride limit");
    trackBuffer(pass, buffer, GPU_PHASE_INPUT_VERTEX, GPU_CACHE_VERTEX);
    draw->vertexBuffer = buffer->gpu;
    draw->vertexBufferOffset = buffer->base;
  } else {
    draw->vertexBuffer = state.defaultBuffer->gpu;
    draw->vertexBufferOffset = state.defaultBuffer->base;
  }

  if (!info->index.buffer && info->index.count > 0) {
    BufferView view = lovrPassGetBuffer(pass, info->index.count * 2, 2);
    if (!view.buffer) return false;
    *info->index.pointer = view.pointer;
    draw->indexBuffer = view.buffer;
    draw->start = view.offset / 2;
  } else if (info->index.buffer) {
    trackBuffer(pass, info->index.buffer, GPU_PHASE_INPUT_INDEX, GPU_CACHE_INDEX);
    draw->indexBuffer = info->index.buffer->gpu;
    draw->flags |= info->index.buffer->info.format->stride == 4 ? DRAW_INDEX32 : 0;
    draw->start += info->index.buffer->base / info->index.buffer->info.format->stride;
  } else {
    draw->indexBuffer = NULL;
  }

  if (info->hash) {
    cached->hash = info->hash;
    cached->vertexBuffer = draw->vertexBuffer;
    cached->indexBuffer = draw->indexBuffer;
    cached->start = draw->start;
    cached->baseVertex = draw->baseVertex;
    cached->vertexBufferOffset = draw->vertexBufferOffset;
  }

  return true;
}

bool lovrPassDraw(Pass* pass, DrawInfo* info) {
  if (pass->drawCount >= pass->drawCapacity) {
    lovrAssert(pass->drawCount < 1 << 16, "Pass has too many draws!");
    pass->drawCapacity = pass->drawCapacity > 0 ? pass->drawCapacity << 1 : 1;
    Draw* draws = lovrPassAllocate(pass, pass->drawCapacity * sizeof(Draw));
    if (pass->draws) memcpy(draws, pass->draws, pass->drawCount * sizeof(Draw));
    pass->draws = draws;
  }

  Draw* previous = pass->drawCount > 0 ? &pass->draws[pass->drawCount - 1] : NULL;
  Draw* draw = &pass->draws[pass->drawCount];

  draw->flags = 0;
  draw->tally = pass->tally.active ? pass->tally.count : 0xff;
  draw->camera = pass->cameraCount - 1;
  draw->viewport = pass->viewportCount - 1;
  draw->scissor = pass->scissorCount - 1;
  pass->flags &= ~DIRTY_CAMERA & ~DIRTY_VIEWPORT & ~DIRTY_SCISSOR;

  draw->shader = pass->pipeline->shader ? pass->pipeline->shader : lovrGraphicsGetDefaultShader(info->shader);
  if (!draw->shader) return false;

  lovrCheck(draw->shader->info.type == SHADER_GRAPHICS, "Tried to draw while a compute shader is active");

  draw->material = info->material;
  if (!draw->material) draw->material = pass->pipeline->material;
  if (!draw->material) draw->material = state.defaultMaterial;
  trackMaterial(pass, draw->material);

  draw->start = info->start;
  draw->count = info->count > 0 ? info->count : (info->index.buffer || info->index.count > 0 ? info->index.count : info->vertex.count);
  draw->instances = MAX(info->instances, 1);
  draw->baseVertex = info->baseVertex;

  lovrPassResolvePipeline(pass, info, draw, previous);
  draw->bundleInfo = lovrPassResolveBindings(pass, draw->shader, previous ? previous->bundleInfo : NULL);
  if (!lovrPassResolveUniforms(pass, draw->shader, &draw->uniformBuffer, &draw->uniformOffset, previous)) return false;
  if (!lovrPassResolveVertices(pass, info, draw)) return false;

  if (pass->pipeline->viewCull && info->bounds) {
    memcpy(draw->bounds, info->bounds, sizeof(draw->bounds));
    draw->flags |= DRAW_HAS_BOUNDS;
    pass->flags |= NEEDS_VIEW_CULL;
  }

  mat4_init(draw->transform, pass->transform);
  if (info->transform) mat4_mul(draw->transform, info->transform);
  memcpy(draw->color, pass->pipeline->color, 4 * sizeof(float));

  lovrRetain(draw->material);
  lovrRetain(draw->shader);
  pass->drawCount++;
  return true;
}

bool lovrPassPoints(Pass* pass, uint32_t count, float** points) {
  return lovrPassDraw(pass, &(DrawInfo) {
    .mode = DRAW_POINTS,
    .vertex.format = VERTEX_POINT,
    .vertex.pointer = (void**) points,
    .vertex.count = count
  });
}

bool lovrPassLine(Pass* pass, uint32_t count, float** points) {
  lovrCheck(count >= 2, "Need at least 2 points to make a line");

  uint16_t* indices;

  DrawInfo draw = {
    .mode = DRAW_LINES,
    .vertex.format = VERTEX_POINT,
    .vertex.pointer = (void**) points,
    .vertex.count = count,
    .index.pointer = (void**) &indices,
    .index.count = 2 * (count - 1)
  };

  if (!lovrPassDraw(pass, &draw)) {
    return false;
  }

  for (uint32_t i = 0; i < count - 1; i++) {
    indices[2 * i + 0] = i;
    indices[2 * i + 1] = i + 1;
  }

  return true;
}

bool lovrPassPolygon(Pass* pass, uint32_t count, float** vertices) {
  lovrCheck(count >= 3, "Need at least 3 points to make a polygon");

  uint16_t* indices;

  DrawInfo draw = {
    .mode = DRAW_TRIANGLES,
    .vertex.format = VERTEX_POINT,
    .vertex.pointer = (void**) vertices,
    .vertex.count = count,
    .index.pointer = (void**) &indices,
    .index.count = 3 * count - 2
  };

  if (!lovrPassDraw(pass, &draw)) {
    return false;
  }

  for (uint32_t i = 3; i <= count; i++) {
    *indices++ = 0;
    *indices++ = i - 2;
    *indices++ = i - 1;
  }

  return true;
}

bool lovrPassPlane(Pass* pass, float* transform, DrawStyle style, uint32_t cols, uint32_t rows) {
  uint32_t key[] = { SHAPE_PLANE, style, cols, rows };
  uint32_t vertexCount = (cols + 1) * (rows + 1);
  uint32_t indexCount = style == STYLE_LINE ? (2 * (rows + 1) + 2 * (cols + 1)) : (cols * rows) * 6;
  ShapeVertex* vertices;
  uint16_t* indices;

  DrawInfo draw = {
    .hash = hash64(key, sizeof(key)),
    .mode = style == STYLE_LINE ? DRAW_LINES : DRAW_TRIANGLES,
    .transform = transform,
    .bounds = (float[6]) { 0.f, 0.f, 0.f, .5f, .5f, 0.f },
    .vertex.pointer = (void**) &vertices,
    .vertex.count = vertexCount,
    .index.pointer = (void**) &indices,
    .index.count = indexCount
  };

  if (!lovrPassDraw(pass, &draw)) {
    return false;
  }

  if (vertices) {
    for (uint32_t y = 0; y <= rows; y++) {
      float v = y * (1.f / rows);
      for (uint32_t x = 0; x <= cols; x++) {
        float u = x * (1.f / cols);
        *vertices++ = (ShapeVertex) {
          .position = { u - .5f, .5f - v, 0.f },
          .normal = { 0.f, 0.f, 1.f },
          .uv = { u, v }
        };
      }
    }

    if (style == STYLE_LINE) {
      for (uint32_t y = 0; y <= rows; y++) {
        uint16_t a = y * (cols + 1);
        uint16_t b = a + cols;
        uint16_t line[] = { a, b };
        memcpy(indices, line, sizeof(line));
        indices += COUNTOF(line);
      }

      for (uint32_t x = 0; x <= cols; x++) {
        uint16_t a = x;
        uint16_t b = x + ((cols + 1) * rows);
        uint16_t line[] = { a, b };
        memcpy(indices, line, sizeof(line));
        indices += COUNTOF(line);
      }
    } else {
      for (uint32_t y = 0; y < rows; y++) {
        for (uint32_t x = 0; x < cols; x++) {
          uint16_t a = (y * (cols + 1)) + x;
          uint16_t b = a + 1;
          uint16_t c = a + cols + 1;
          uint16_t d = a + cols + 2;
          uint16_t cell[] = { a, c, b, b, c, d };
          memcpy(indices, cell, sizeof(cell));
          indices += COUNTOF(cell);
        }
      }
    }
  }

  return true;
}

bool lovrPassRoundrect(Pass* pass, float* transform, float r, uint32_t segments) {
  bool thicc = vec3_length(transform + 8) > 0.f;
  float w = vec3_length(transform + 0);
  float h = vec3_length(transform + 4);
  r = MIN(MIN(r, w / 2.f), h / 2.f);
  float rx = MIN(r / w, .5f);
  float ry = MIN(r / h, .5f);
  uint32_t n = segments + 1;

  if (!thicc && (r <= 0.f || w == 0.f || h == 0.f)) {
    return lovrPassPlane(pass, transform, STYLE_FILL, 1, 1);
  }

  uint32_t vertexCount;
  uint32_t indexCount;

  if (thicc) {
    vertexCount = 8 + (segments + 1) * 16;
    indexCount = 3 * 8 * segments + 6 * 4 * (segments + 1) + 60;
  } else {
    vertexCount = 4 + (segments + 1) * 4;
    indexCount = 3 * 4 * segments + 30;
  }

  ShapeVertex* vertices;
  uint16_t* indices;

  DrawInfo draw = {
    .mode = DRAW_TRIANGLES,
    .transform = transform,
    .bounds = (float[6]) { 0.f, 0.f, 0.f, .5f, .5f, .5f },
    .vertex.pointer = (void**) &vertices,
    .vertex.count = vertexCount,
    .index.pointer = (void**) &indices,
    .index.count = indexCount
  };

  if (!lovrPassDraw(pass, &draw)) {
    return false;
  }

  uint32_t c = vertexCount - (thicc ? 8 : 4);
  ShapeVertex* corner = vertices + c;

  float angle = 0.f;
  float step = (float) M_PI / 2.f / segments;
  float x = .5f - rx;
  float y = .5f - ry;
  float z = .5f;
  float nz = 1.f;

  // If the rounded rectangle is thick, loop twice (front and back), otherwise do only a single side
  for (uint32_t side = 0; side <= (uint32_t) thicc; side++, z *= -1.f, nz *= -1.f, angle = 0.f) {
    for (uint32_t i = 0; i < n; i++, angle += step) {
      float c = cosf(angle);
      float s = sinf(angle);

      vertices[n * 0 + i] = (ShapeVertex) { {  x + c * rx,  y + s * ry, z }, { 0.f, 0.f, nz }, { .5f + x + c * rx, .5f - y - s * ry } };
      vertices[n * 1 + i] = (ShapeVertex) { { -x - s * rx,  y + c * ry, z }, { 0.f, 0.f, nz }, { .5f - x - s * rx, .5f - y - c * ry } };
      vertices[n * 2 + i] = (ShapeVertex) { { -x - c * rx, -y - s * ry, z }, { 0.f, 0.f, nz }, { .5f - x - c * rx, .5f + y + s * ry } };
      vertices[n * 3 + i] = (ShapeVertex) { {  x + s * rx, -y - c * ry, z }, { 0.f, 0.f, nz }, { .5f + x + s * rx, .5f + y + c * ry } };

      if (thicc) {
        vertices[n * 8  + i] = (ShapeVertex) { {  x + c * rx,  y + s * ry, z }, { c, s, 0.f }, { .5f + x + c * rx, .5f - y - s * ry } };
        vertices[n * 9  + i] = (ShapeVertex) { { -x - s * rx,  y + c * ry, z }, { c, s, 0.f }, { .5f - x - s * rx, .5f - y - c * ry } };
        vertices[n * 10 + i] = (ShapeVertex) { { -x - c * rx, -y - s * ry, z }, { c, s, 0.f }, { .5f - x - c * rx, .5f + y + s * ry } };
        vertices[n * 11 + i] = (ShapeVertex) { {  x + s * rx, -y - c * ry, z }, { c, s, 0.f }, { .5f + x + s * rx, .5f + y + c * ry } };
      }
    }

    vertices += 4 * n;

    // 4 extra corner vertices per-side, used for the triangle fans and 9-slice quads
    *corner++ = (ShapeVertex) { {  x,  y, z }, { 0.f, 0.f, nz }, { .5f + x, .5f - y } };
    *corner++ = (ShapeVertex) { { -x,  y, z }, { 0.f, 0.f, nz }, { .5f - x, .5f - y } };
    *corner++ = (ShapeVertex) { { -x, -y, z }, { 0.f, 0.f, nz }, { .5f - x, .5f + y } };
    *corner++ = (ShapeVertex) { {  x, -y, z }, { 0.f, 0.f, nz }, { .5f + x, .5f + y } };
  }

  uint32_t m = segments;

  uint16_t front[] = {
    n * 0 + m, n * 1, c + 0, c + 0, n * 1, c + 1, // top
    c + 1, n * 1 + m, c + 2, c + 2, n * 1 + m, n * 2, // left
    n * 0, c + 0, n * 3 + m, n * 3 + m, c + 0, c + 3, // right
    c + 3, c + 2, n * 3, n * 3, c + 2, 2 * n + m, // bot
    c + 0, c + 1, c + 3, c + 3, c + 1, c + 2 // center
  };

  memcpy(indices, front, sizeof(front));
  indices += COUNTOF(front);

  for (uint32_t i = 0; i < 4; i++) {
    for (uint32_t j = 0; j < segments; j++) {
      memcpy(indices, (uint16_t[]) { c + i, n * i + j, n * i + j + 1 }, 3 * sizeof(uint16_t));
      indices += 3;
    }
  }

  if (thicc) {
    uint16_t back[] = {
      n * 4 + m, c + 4, n * 5, n * 5, c + 4, c + 5, // top
      c + 5, c + 6, n * 5 + m, n * 5 + m, c + 6, n * 6, // left
      n * 4, n * 7 + m, c + 4, c + 4, n * 7 + m, c + 7, // right
      c + 7, n * 7, c + 6, c + 6, n * 7, 6 * n + m, // bot
      c + 4, c + 7, c + 5, c + 5, c + 7, c + 6 // center
    };

    memcpy(indices, back, sizeof(back));
    indices += COUNTOF(back);

    for (uint32_t i = 4; i < 8; i++) {
      for (uint32_t j = 0; j < segments; j++) {
        memcpy(indices, (uint16_t[]) { n * i + j, c + i, n * i + j + 1 }, 3 * sizeof(uint16_t));
        indices += 3;
      }
    }

    // Stitch sides together
    for (uint32_t i = 0; i < 4 * n - 1; i++) {
      uint16_t a = 8 * n + i;
      uint16_t b = 12 * n + i;
      memcpy(indices, (uint16_t[]) { a, b, b + 1, a, b + 1, a + 1 }, 6 * sizeof(uint16_t));
      indices += 6;
    }

    // Handle discontinuity
    uint16_t a = 11 * n + m;
    uint16_t b = 15 * n + m;
    uint16_t c = 12 * n;
    uint16_t d = 8 * n;
    memcpy(indices, (uint16_t[]) { a, b, c, a, c, d }, 6 * sizeof(uint16_t));
    indices += 6;
  }

  return true;
}

bool lovrPassBox(Pass* pass, float* transform, DrawStyle style) {
  uint32_t key[] = { SHAPE_BOX, style };
  ShapeVertex* vertices;
  uint16_t* indices;

  if (style == STYLE_LINE) {
    static ShapeVertex vertexData[] = {
      { { -.5f,  .5f, -.5f }, { 0.f, 0.f, 0.f }, { 0.f, 0.f } }, // Front
      { {  .5f,  .5f, -.5f }, { 0.f, 0.f, 0.f }, { 0.f, 0.f } },
      { {  .5f, -.5f, -.5f }, { 0.f, 0.f, 0.f }, { 0.f, 0.f } },
      { { -.5f, -.5f, -.5f }, { 0.f, 0.f, 0.f }, { 0.f, 0.f } },
      { { -.5f,  .5f,  .5f }, { 0.f, 0.f, 0.f }, { 0.f, 0.f } }, // Back
      { {  .5f,  .5f,  .5f }, { 0.f, 0.f, 0.f }, { 0.f, 0.f } },
      { {  .5f, -.5f,  .5f }, { 0.f, 0.f, 0.f }, { 0.f, 0.f } },
      { { -.5f, -.5f,  .5f }, { 0.f, 0.f, 0.f }, { 0.f, 0.f } }
    };

    static uint16_t indexData[] = {
      0, 1, 1, 2, 2, 3, 3, 0, // Front
      4, 5, 5, 6, 6, 7, 7, 4, // Back
      0, 4, 1, 5, 2, 6, 3, 7  // Connections
    };

    DrawInfo draw = {
      .hash = hash64(key, sizeof(key)),
      .mode = DRAW_LINES,
      .transform = transform,
      .bounds = (float[6]) { 0.f, 0.f, 0.f, .5f, .5f, .5f },
      .vertex.pointer = (void**) &vertices,
      .vertex.count = COUNTOF(vertexData),
      .index.pointer = (void**) &indices,
      .index.count = COUNTOF(indexData)
    };

    if (!lovrPassDraw(pass, &draw)) {
      return false;
    }

    if (vertices) {
      memcpy(vertices, vertexData, sizeof(vertexData));
      memcpy(indices, indexData, sizeof(indexData));
    }
  } else {
    static ShapeVertex vertexData[] = {
      { { -.5f, -.5f, -.5f }, {  0.f,  0.f, -1.f }, { 0.f, 0.f } }, // Front
      { { -.5f,  .5f, -.5f }, {  0.f,  0.f, -1.f }, { 0.f, 1.f } },
      { {  .5f, -.5f, -.5f }, {  0.f,  0.f, -1.f }, { 1.f, 0.f } },
      { {  .5f,  .5f, -.5f }, {  0.f,  0.f, -1.f }, { 1.f, 1.f } },
      { {  .5f,  .5f, -.5f }, {  1.f,  0.f,  0.f }, { 0.f, 1.f } }, // Right
      { {  .5f,  .5f,  .5f }, {  1.f,  0.f,  0.f }, { 1.f, 1.f } },
      { {  .5f, -.5f, -.5f }, {  1.f,  0.f,  0.f }, { 0.f, 0.f } },
      { {  .5f, -.5f,  .5f }, {  1.f,  0.f,  0.f }, { 1.f, 0.f } },
      { {  .5f, -.5f,  .5f }, {  0.f,  0.f,  1.f }, { 0.f, 0.f } }, // Back
      { {  .5f,  .5f,  .5f }, {  0.f,  0.f,  1.f }, { 0.f, 1.f } },
      { { -.5f, -.5f,  .5f }, {  0.f,  0.f,  1.f }, { 1.f, 0.f } },
      { { -.5f,  .5f,  .5f }, {  0.f,  0.f,  1.f }, { 1.f, 1.f } },
      { { -.5f,  .5f,  .5f }, { -1.f,  0.f,  0.f }, { 0.f, 1.f } }, // Left
      { { -.5f,  .5f, -.5f }, { -1.f,  0.f,  0.f }, { 1.f, 1.f } },
      { { -.5f, -.5f,  .5f }, { -1.f,  0.f,  0.f }, { 0.f, 0.f } },
      { { -.5f, -.5f, -.5f }, { -1.f,  0.f,  0.f }, { 1.f, 0.f } },
      { { -.5f, -.5f, -.5f }, {  0.f, -1.f,  0.f }, { 0.f, 0.f } }, // Bottom
      { {  .5f, -.5f, -.5f }, {  0.f, -1.f,  0.f }, { 1.f, 0.f } },
      { { -.5f, -.5f,  .5f }, {  0.f, -1.f,  0.f }, { 0.f, 1.f } },
      { {  .5f, -.5f,  .5f }, {  0.f, -1.f,  0.f }, { 1.f, 1.f } },
      { { -.5f,  .5f, -.5f }, {  0.f,  1.f,  0.f }, { 0.f, 1.f } }, // Top
      { { -.5f,  .5f,  .5f }, {  0.f,  1.f,  0.f }, { 0.f, 0.f } },
      { {  .5f,  .5f, -.5f }, {  0.f,  1.f,  0.f }, { 1.f, 1.f } },
      { {  .5f,  .5f,  .5f }, {  0.f,  1.f,  0.f }, { 1.f, 0.f } }
    };

    static uint16_t indexData[] = {
      0,  1,   2,  2,  1,  3,
      4,  5,   6,  6,  5,  7,
      8,  9,  10, 10,  9, 11,
      12, 13, 14, 14, 13, 15,
      16, 17, 18, 18, 17, 19,
      20, 21, 22, 22, 21, 23
    };

    DrawInfo draw = {
      .hash = hash64(key, sizeof(key)),
      .mode = DRAW_TRIANGLES,
      .transform = transform,
      .bounds = (float[6]) { 0.f, 0.f, 0.f, .5f, .5f, .5f },
      .vertex.pointer = (void**) &vertices,
      .vertex.count = COUNTOF(vertexData),
      .index.pointer = (void**) &indices,
      .index.count = COUNTOF(indexData)
    };

    if (!lovrPassDraw(pass, &draw)) {
      return false;
    }

    if (vertices) {
      memcpy(vertices, vertexData, sizeof(vertexData));
      memcpy(indices, indexData, sizeof(indexData));
    }
  }

  return true;
}

bool lovrPassCircle(Pass* pass, float* transform, DrawStyle style, float angle1, float angle2, uint32_t segments) {
  if (fabsf(angle1 - angle2) >= 2.f * (float) M_PI) {
    angle1 = 0.f;
    angle2 = 2.f * (float) M_PI;
  }

  uint32_t key[] = { SHAPE_CIRCLE, style, FLOAT_BITS(angle1), FLOAT_BITS(angle2), segments };
  ShapeVertex* vertices;
  uint16_t* indices;

  DrawInfo draw = {
    .hash = hash64(key, sizeof(key)),
    .mode = style == STYLE_LINE ? DRAW_LINES : DRAW_TRIANGLES,
    .transform = transform,
    .bounds = (float[6]) { 0.f, 0.f, 0.f, 1.f, 1.f, 0.f },
    .vertex.pointer = (void**) &vertices,
    .vertex.count = style == STYLE_LINE ? segments + 1 : segments + 2,
    .index.pointer = (void**) &indices,
    .index.count = style == STYLE_LINE ? segments * 2 : segments * 3
  };

  if (!lovrPassDraw(pass, &draw)) {
    return false;
  }

  if (!vertices) {
    return true;
  }

  // Center
  if (style == STYLE_FILL) {
    *vertices++ = (ShapeVertex) { { 0.f, 0.f, 0.f }, { 0.f, 0.f, 1.f }, { .5f, .5f } };
  }

  float angleShift = (angle2 - angle1) / segments;
  for (uint32_t i = 0; i <= segments; i++) {
    float theta = angle1 + i * angleShift;
    float x = cosf(theta);
    float y = sinf(theta);
    *vertices++ = (ShapeVertex) { { x, y, 0.f }, { 0.f, 0.f, 1.f }, { x + .5f, .5f - y } };
  }

  if (style == STYLE_LINE) {
    for (uint32_t i = 0; i < segments; i++) {
      uint16_t segment[] = { i, i + 1 };
      memcpy(indices, segment, sizeof(segment));
      indices += COUNTOF(segment);
    }
  } else {
    for (uint32_t i = 0; i < segments; i++) {
      uint16_t wedge[] = { 0, i + 1, i + 2 };
      memcpy(indices, wedge, sizeof(wedge));
      indices += COUNTOF(wedge);
    }
  }

  return true;
}

bool lovrPassSphere(Pass* pass, float* transform, uint32_t segmentsH, uint32_t segmentsV) {
  lovrCheck(segmentsH >= 2 && segmentsV >= 2, "Sphere segment count must be >= 2");

  uint32_t vertexCount = 2 + (segmentsH + 1) * (segmentsV - 1);
  uint32_t indexCount = 2 * 3 * segmentsH + segmentsH * (segmentsV - 2) * 6;
  ShapeVertex* vertices;
  uint16_t* indices;

  uint32_t key[] = { SHAPE_SPHERE, segmentsH, segmentsV };

  DrawInfo draw = {
    .hash = hash64(key, sizeof(key)),
    .mode = DRAW_TRIANGLES,
    .transform = transform,
    .bounds = (float[6]) { 0.f, 0.f, 0.f, 1.f, 1.f, 1.f },
    .vertex.pointer = (void**) &vertices,
    .vertex.count = vertexCount,
    .index.pointer = (void**) &indices,
    .index.count = indexCount,
  };

  if (!lovrPassDraw(pass, &draw)) {
    return false;
  }

  if (!vertices) {
    return true;
  }

  // Top
  *vertices++ = (ShapeVertex) { { 0.f, 1.f, 0.f }, { 0.f, 1.f, 0.f }, { .5f, 0.f } };

  // Rings
  for (uint32_t i = 1; i < segmentsV; i++) {
    float v = i / (float) segmentsV;
    float phi = v * (float) M_PI;
    float sinphi = sinf(phi);
    float cosphi = cosf(phi);
    for (uint32_t j = 0; j <= segmentsH; j++) {
      float u = j / (float) segmentsH;
      float theta = u * 2.f * (float) M_PI;
      float sintheta = sinf(theta);
      float costheta = cosf(theta);
      float x = sintheta * sinphi;
      float y = cosphi;
      float z = -costheta * sinphi;
      *vertices++ = (ShapeVertex) { { x, y, z }, { x, y, z }, { u, v } };
    }
  }

  // Bottom
  *vertices++ = (ShapeVertex) { { 0.f, -1.f, 0.f }, { 0.f, -1.f, 0.f }, { .5f, 1.f } };

  // Top
  for (uint32_t i = 0; i < segmentsH; i++) {
    uint16_t wedge[] = { 0, i + 2, i + 1 };
    memcpy(indices, wedge, sizeof(wedge));
    indices += COUNTOF(wedge);
  }

  // Rings
  for (uint32_t i = 0; i < segmentsV - 2; i++) {
    for (uint32_t j = 0; j < segmentsH; j++) {
      uint16_t a = 1 + i * (segmentsH + 1) + 0 + j;
      uint16_t b = 1 + i * (segmentsH + 1) + 1 + j;
      uint16_t c = 1 + i * (segmentsH + 1) + 0 + segmentsH + 1 + j;
      uint16_t d = 1 + i * (segmentsH + 1) + 1 + segmentsH + 1 + j;
      uint16_t quad[] = { a, b, c, c, b, d };
      memcpy(indices, quad, sizeof(quad));
      indices += COUNTOF(quad);
    }
  }

  // Bottom
  for (uint32_t i = 0; i < segmentsH; i++) {
    uint16_t wedge[] = { vertexCount - 1, vertexCount - 1 - (i + 2), vertexCount - 1 - (i + 1) };
    memcpy(indices, wedge, sizeof(wedge));
    indices += COUNTOF(wedge);
  }

  return true;
}

bool lovrPassCylinder(Pass* pass, float* transform, bool capped, float angle1, float angle2, uint32_t segments) {
  if (fabsf(angle1 - angle2) >= 2.f * (float) M_PI) {
    angle1 = 0.f;
    angle2 = 2.f * (float) M_PI;
  }

  uint32_t key[] = { SHAPE_CYLINDER, capped, FLOAT_BITS(angle1), FLOAT_BITS(angle2), segments };

  uint32_t vertexCount = 2 * (segments + 1);
  uint32_t indexCount = 6 * segments;
  ShapeVertex* vertices;
  uint16_t* indices;

  if (capped) {
    vertexCount *= 2;
    vertexCount += 2;
    indexCount += 3 * segments * 2;
  }

  DrawInfo draw = {
    .hash = hash64(key, sizeof(key)),
    .mode = DRAW_TRIANGLES,
    .transform = transform,
    .bounds = (float[6]) { 0.f, 0.f, 0.f, 1.f, 1.f, .5f },
    .vertex.pointer = (void**) &vertices,
    .vertex.count = vertexCount,
    .index.pointer = (void**) &indices,
    .index.count = indexCount
  };

  if (!lovrPassDraw(pass, &draw)) {
    return false;
  }

  if (!vertices) {
    return true;
  }

  float angleShift = (angle2 - angle1) / segments;

  // Tube
  for (uint32_t i = 0; i <= segments; i++) {
    float theta = angle1 + i * angleShift;
    float x = cosf(theta);
    float y = sinf(theta);
    *vertices++ = (ShapeVertex) { { x, y, -.5f }, { x, y, 0.f }, { x + .5f, .5f - y } };
    *vertices++ = (ShapeVertex) { { x, y,  .5f }, { x, y, 0.f }, { x + .5f, .5f - y } };
  }

  // Tube quads
  for (uint32_t i = 0; i < segments; i++) {
    uint16_t a = i * 2 + 0;
    uint16_t b = i * 2 + 1;
    uint16_t c = i * 2 + 2;
    uint16_t d = i * 2 + 3;
    uint16_t quad[] = { a, c, b, b, c, d };
    memcpy(indices, quad, sizeof(quad));
    indices += COUNTOF(quad);
  }

  if (capped) {
    // Cap centers
    *vertices++ = (ShapeVertex) { { 0.f, 0.f, -.5f }, { 0.f, 0.f, -1.f }, { .5f, .5f } };
    *vertices++ = (ShapeVertex) { { 0.f, 0.f,  .5f }, { 0.f, 0.f,  1.f }, { .5f, .5f } };

    // Caps
    for (uint32_t i = 0; i <= segments; i++) {
      float theta = angle1 + i * angleShift;
      float x = cosf(theta);
      float y = sinf(theta);
      *vertices++ = (ShapeVertex) { { x, y, -.5f }, { 0.f, 0.f, -1.f }, { x + .5f, y - .5f } };
      *vertices++ = (ShapeVertex) { { x, y,  .5f }, { 0.f, 0.f,  1.f }, { x + .5f, y - .5f } };
    }

    // Cap wedges
    uint16_t base = 2 * (segments + 1);
    for (uint32_t i = 0; i < segments; i++) {
      uint16_t a = base + 0;
      uint16_t b = base + (i + 1) * 2;
      uint16_t c = base + (i + 2) * 2;
      uint16_t wedge1[] = { a + 0, c + 0, b + 0 };
      uint16_t wedge2[] = { a + 1, b + 1, c + 1 };
      memcpy(indices + 0, wedge1, sizeof(wedge1));
      memcpy(indices + 3, wedge2, sizeof(wedge2));
      indices += 6;
    }
  }

  return true;
}

bool lovrPassCone(Pass* pass, float* transform, uint32_t segments) {
  uint32_t key[] = { SHAPE_CONE, segments };
  uint32_t vertexCount = 2 * segments + 1;
  uint32_t indexCount = 3 * (segments - 2) + 3 * segments;
  ShapeVertex* vertices;
  uint16_t* indices;

  DrawInfo draw = {
    .hash = hash64(key, sizeof(key)),
    .mode = DRAW_TRIANGLES,
    .transform = transform,
    .bounds = (float[6]) { 0.0f, 0.f, -.5f, 1.f, 1.f, .5f },
    .vertex.pointer = (void**) &vertices,
    .vertex.count = vertexCount,
    .index.pointer = (void**) &indices,
    .index.count = indexCount
  };

  if (!lovrPassDraw(pass, &draw)) {
    return false;
  }

  if (!vertices) {
    return true;
  }

  for (uint32_t i = 0; i < segments; i++) {
    float theta = i * 2.f * (float) M_PI / segments;
    float x = cosf(theta);
    float y = sinf(theta);
    float rsqrt3 = .57735f;
    float nx = cosf(theta) * rsqrt3;
    float ny = sinf(theta) * rsqrt3;
    float nz = -rsqrt3;
    float u = x + .5f;
    float v = .5f - y;
    vertices[segments * 0] = (ShapeVertex) { { x, y, 0.f }, { 0.f, 0.f, 1.f }, { u, v } };
    vertices[segments * 1] = (ShapeVertex) { { x, y, 0.f }, { nx, ny, nz }, { u, v } };
    vertices++;
  }

  vertices[segments] = (ShapeVertex) { { 0.f, 0.f, -1.f }, { 0.f, 0.f, 0.f }, { .5f, .5f } };

  // Base
  for (uint32_t i = 0; i < segments - 2; i++) {
    uint16_t tri[] = { 0, i + 1, i + 2 };
    memcpy(indices, tri, sizeof(tri));
    indices += COUNTOF(tri);
  }

  // Sides
  for (uint32_t i = 0; i < segments; i++) {
    uint16_t tri[] = { segments + (i + 1) % segments, segments + i, vertexCount - 1 };
    memcpy(indices, tri, sizeof(tri));
    indices += COUNTOF(tri);
  }

  return true;
}

bool lovrPassCapsule(Pass* pass, float* transform, uint32_t segments) {
  float sx = vec3_length(transform + 0);
  float sy = vec3_length(transform + 4);
  float sz = vec3_length(transform + 8);
  float length = sz * .5f;
  float radius = sx;

  if (length == 0.f) {
    float rotation[4];
    vec3_cross(vec3_init(transform + 8, transform + 0), transform + 4);
    vec3_scale(transform + 8, 1.f / radius);
    mat4_rotateQuat(transform, quat_fromAngleAxis(rotation, (float) M_PI / 2.f, 1.f, 0.f, 0.f));
    return lovrPassSphere(pass, transform, segments, segments);
  }

  vec3_scale(transform + 0, 1.f / sx);
  vec3_scale(transform + 4, 1.f / sy);
  vec3_scale(transform + 8, 1.f / sz);

  uint32_t key[] = { SHAPE_CAPSULE, FLOAT_BITS(radius), FLOAT_BITS(length), segments };

  uint32_t rings = segments / 2;
  uint32_t vertexCount = 2 * (1 + rings * (segments + 1));
  uint32_t indexCount = 2 * (3 * segments + 6 * segments * (rings - 1)) + 6 * segments;
  ShapeVertex* vertices;
  uint16_t* indices;

  DrawInfo draw = {
    .hash = hash64(key, sizeof(key)),
    .mode = DRAW_TRIANGLES,
    .transform = transform,
    .bounds = (float[6]) { 0.f, 0.f, 0.f, radius, radius, length + radius },
    .vertex.pointer = (void**) &vertices,
    .vertex.count = vertexCount,
    .index.pointer = (void**) &indices,
    .index.count = indexCount
  };

  if (!lovrPassDraw(pass, &draw)) {
    return false;
  }

  if (!vertices) {
    return true;
  }

  float tip = length + radius;
  uint32_t h = vertexCount / 2;
  vertices[0] = (ShapeVertex) { { 0.f, 0.f, -tip }, { 0.f, 0.f, -1.f }, { .5f, 0.f } };
  vertices[h] = (ShapeVertex) { { 0.f, 0.f,  tip }, { 0.f, 0.f,  1.f }, { .5f, 1.f } };
  vertices++;

  for (uint32_t i = 1; i <= rings; i++) {
    float v = i / (float) rings;
    float phi = v * (float) M_PI / 2.f;
    float sinphi = sinf(phi);
    float cosphi = cosf(phi);
    for (uint32_t j = 0; j <= segments; j++) {
      float u = j / (float) segments;
      float theta = u * (float) M_PI * 2.f;
      float sintheta = sinf(theta);
      float costheta = cosf(theta);
      float x = costheta * sinphi;
      float y = sintheta * sinphi;
      float z = cosphi;
      vertices[0] = (ShapeVertex) { { x * radius, y * radius, -(length + z * radius) }, { x, y, -z }, { u, v } };
      vertices[h] = (ShapeVertex) { { x * radius, y * radius,  (length + z * radius) }, { x, y,  z }, { u, 1.f - v } };
      vertices++;
    }
  }

  uint16_t* i1 = indices;
  uint16_t* i2 = indices + (indexCount - 6 * segments) / 2;
  for (uint32_t i = 0; i < segments; i++) {
    uint16_t wedge1[] = { 0, 0 + i + 2, 0 + i + 1 };
    uint16_t wedge2[] = { h, h + i + 1, h + i + 2 };
    memcpy(i1, wedge1, sizeof(wedge1));
    memcpy(i2, wedge2, sizeof(wedge2));
    i1 += COUNTOF(wedge1);
    i2 += COUNTOF(wedge2);
  }

  for (uint32_t i = 0; i < rings - 1; i++) {
    for (uint32_t j = 0; j < segments; j++) {
      uint16_t a = 1 + i * (segments + 1) + 0 + j;
      uint16_t b = 1 + i * (segments + 1) + 1 + j;
      uint16_t c = 1 + i * (segments + 1) + 0 + segments + 1 + j;
      uint16_t d = 1 + i * (segments + 1) + 1 + segments + 1 + j;
      uint16_t quad1[] = { a, b, c, c, b, d };
      uint16_t quad2[] = { h + a, h + c, h + b, h + b, h + c, h + d };
      memcpy(i1, quad1, sizeof(quad1));
      memcpy(i2, quad2, sizeof(quad2));
      i1 += COUNTOF(quad1);
      i2 += COUNTOF(quad2);
    }
  }

  for (uint32_t i = 0; i < segments; i++) {
    uint16_t a = h - segments - 1 + i;
    uint16_t b = h - segments - 1 + i + 1;
    uint16_t c = vertexCount - segments - 1 + i;
    uint16_t d = vertexCount - segments - 1 + i + 1;
    uint16_t quad[] = { a, b, c, c, b, d };
    memcpy(i2, quad, sizeof(quad));
    i2 += COUNTOF(quad);
  }

  return true;
}

bool lovrPassTorus(Pass* pass, float* transform, uint32_t segmentsT, uint32_t segmentsP) {
  float sx = vec3_length(transform + 0);
  float sy = vec3_length(transform + 4);
  float sz = vec3_length(transform + 8);
  vec3_scale(transform + 0, 1.f / sx);
  vec3_scale(transform + 4, 1.f / sy);
  vec3_scale(transform + 8, 1.f / sz);
  float radius = sx * .5f;
  float thickness = sz * .5f;

  uint32_t key[] = { SHAPE_TORUS, FLOAT_BITS(radius), FLOAT_BITS(thickness), segmentsT, segmentsP };
  uint32_t vertexCount = segmentsT * segmentsP;
  uint32_t indexCount = segmentsT * segmentsP * 6;
  ShapeVertex* vertices;
  uint16_t* indices;

  DrawInfo draw = {
    .hash = hash64(key, sizeof(key)),
    .mode = DRAW_TRIANGLES,
    .transform = transform,
    .bounds = (float[6]) { 0.f, 0.f, 0.f, radius + thickness, radius + thickness, thickness },
    .vertex.pointer = (void**) &vertices,
    .vertex.count = vertexCount,
    .index.pointer = (void**) &indices,
    .index.count = indexCount
  };

  if (!lovrPassDraw(pass, &draw)) {
    return false;
  }

  if (!vertices) {
    return true;
  }

  // T and P stand for toroidal and poloidal, or theta and phi
  float dt = (2.f * (float) M_PI) / segmentsT;
  float dp = (2.f * (float) M_PI) / segmentsP;
  for (uint32_t t = 0; t < segmentsT; t++) {
    float theta = t * dt;
    float tx = cosf(theta);
    float ty = sinf(theta);
    for (uint32_t p = 0; p < segmentsP; p++) {
      float phi = p * dp;
      float nx = cosf(phi) * tx;
      float ny = cosf(phi) * ty;
      float nz = sinf(phi);

      *vertices++ = (ShapeVertex) {
        .position = { tx * radius + nx * thickness, ty * radius + ny * thickness, nz * thickness },
        .normal = { nx, ny, nz }
      };

      uint16_t a = (t + 0) * segmentsP + p;
      uint16_t b = (t + 1) % segmentsT * segmentsP + p;
      uint16_t c = (t + 0) * segmentsP + (p + 1) % segmentsP;
      uint16_t d = (t + 1) % segmentsT * segmentsP + (p + 1) % segmentsP;
      uint16_t quad[] = { a, b, c, c, b, d };
      memcpy(indices, quad, sizeof(quad));
      indices += COUNTOF(quad);
    }
  }

  return true;
}

bool lovrPassText(Pass* pass, ColoredString* strings, uint32_t count, float* transform, float wrap, HorizontalAlign halign, VerticalAlign valign) {
  Font* font = pass->pipeline->font ? pass->pipeline->font : lovrGraphicsGetDefaultFont();

  if (!font) {
    return false;
  }

  size_t totalLength = 0;
  for (uint32_t i = 0; i < count; i++) {
    totalLength += strings[i].length;
  }

  if (!beginFrame()) {
    return false;
  }

  size_t stack = tempPush(&state.allocator);
  GlyphVertex* vertices = tempAlloc(&state.allocator, totalLength * 4 * sizeof(GlyphVertex));
  uint32_t glyphCount;
  uint32_t lineCount;

  float leading = lovrRasterizerGetLeading(font->info.rasterizer) * font->lineSpacing;
  float ascent = lovrRasterizerGetAscent(font->info.rasterizer);
  float scale = 1.f / font->pixelDensity;
  wrap /= scale;

  Material* material;
  bool flip = pass->cameras[(pass->cameraCount - 1) * pass->canvas.views].projection[5] > 0.f;
  if (!lovrFontGetVertices(font, strings, count, wrap, halign, valign, vertices, &glyphCount, &lineCount, &material, flip)) {
    tempPop(&state.allocator, stack);
    return false;
  }

  mat4_scale(transform, scale, scale, scale);
  float offset = -ascent + valign / 2.f * (leading * lineCount);
  mat4_translate(transform, 0.f, flip ? -offset : offset, 0.f);

  GlyphVertex* vertexPointer;
  uint16_t* indices;

  DrawInfo draw = {
    .mode = DRAW_TRIANGLES,
    .shader = lovrRasterizerGetType(font->info.rasterizer) == RASTERIZER_TTF ? SHADER_FONT : SHADER_UNLIT,
    .material = font->material,
    .transform = transform,
    .vertex.format = VERTEX_GLYPH,
    .vertex.pointer = (void**) &vertexPointer,
    .vertex.count = glyphCount * 4,
    .index.pointer = (void**) &indices,
    .index.count = glyphCount * 6
  };

  if (!lovrPassDraw(pass, &draw)) {
    tempPop(&state.allocator, stack);
    return false;
  }

  memcpy(vertexPointer, vertices, glyphCount * 4 * sizeof(GlyphVertex));

  for (uint32_t i = 0; i < glyphCount * 4; i += 4) {
    uint16_t quad[] = { i + 0, i + 2, i + 1, i + 1, i + 2, i + 3 };
    memcpy(indices, quad, sizeof(quad));
    indices += COUNTOF(quad);
  }

  tempPop(&state.allocator, stack);
  return true;
}

bool lovrPassSkybox(Pass* pass, Texture* texture) {
  return lovrPassDraw(pass, &(DrawInfo) {
    .mode = DRAW_TRIANGLES,
    .shader = !texture || texture->info.type == TEXTURE_2D ? SHADER_EQUIRECT : SHADER_CUBEMAP,
    .material = texture ? lovrTextureToMaterial(texture) : NULL,
    .vertex.format = VERTEX_EMPTY,
    .count = 6
  });
}

bool lovrPassFill(Pass* pass, Texture* texture) {
  return lovrPassDraw(pass, &(DrawInfo) {
    .mode = DRAW_TRIANGLES,
    .shader = texture && texture->info.type == TEXTURE_ARRAY ? SHADER_FILL_ARRAY : SHADER_FILL_2D,
    .material = texture ? lovrTextureToMaterial(texture) : NULL,
    .vertex.format = VERTEX_EMPTY,
    .count = 3
  });
}

bool lovrPassMonkey(Pass* pass, float* transform) {
  uint32_t key[] = { SHAPE_MONKEY };
  uint32_t vertexCount = COUNTOF(monkey_vertices) / 6;
  ShapeVertex* vertices;
  uint16_t* indices;

  DrawInfo draw = {
    .hash = hash64(key, sizeof(key)),
    .mode = DRAW_TRIANGLES,
    .vertex.pointer = (void**) &vertices,
    .vertex.count = vertexCount,
    .index.pointer = (void**) &indices,
    .index.count = COUNTOF(monkey_indices),
    .transform = transform,
    .bounds = monkey_bounds
  };

  if (!lovrPassDraw(pass, &draw)) {
    return false;
  }

  if (!vertices) {
    return true;
  }

  // Manual vertex format conversion to avoid another format (and sn8x3 isn't always supported)
  for (uint32_t i = 0; i < vertexCount; i++) {
    vertices[i] = (ShapeVertex) {
      .position.x = monkey_vertices[6 * i + 0] / 255.f * monkey_bounds[3] * 2.f + monkey_offset[0],
      .position.y = monkey_vertices[6 * i + 1] / 255.f * monkey_bounds[4] * 2.f + monkey_offset[1],
      .position.z = monkey_vertices[6 * i + 2] / 255.f * monkey_bounds[5] * 2.f + monkey_offset[2],
      .normal.x = monkey_vertices[6 * i + 3] / 255.f * 2.f - 1.f,
      .normal.y = monkey_vertices[6 * i + 4] / 255.f * 2.f - 1.f,
      .normal.z = monkey_vertices[6 * i + 5] / 255.f * 2.f - 1.f,
    };
  }

  memcpy(indices, monkey_indices, sizeof(monkey_indices));

  return true;
}

bool lovrPassDrawMesh(Pass* pass, Mesh* mesh, float* transform, uint32_t instances) {
  uint32_t extent = mesh->indexCount > 0 ? mesh->indexCount : mesh->vertexBuffer->info.format->length;
  uint32_t start = MIN(mesh->drawStart, extent - 1);
  uint32_t count = mesh->drawCount > 0 ? MIN(mesh->drawCount, extent - start) : extent - start;

  if (!lovrMeshFlush(mesh)) {
    return false;
  }

  return lovrPassDraw(pass, &(DrawInfo) {
    .mode = mesh->mode,
    .transform = transform,
    .bounds = mesh->hasBounds ? mesh->bounds : NULL,
    .material = mesh->material,
    .vertex.buffer = mesh->vertexBuffer,
    .index.buffer = mesh->indexBuffer,
    .start = start,
    .count = count,
    .baseVertex = mesh->baseVertex,
    .instances = instances
  });
}

static bool drawNode(Pass* pass, Model* model, uint32_t index, uint32_t instances) {
  ModelNode* node = &model->info.data->nodes[index];
  mat4 globalTransform = model->globalTransforms + 16 * index;

  for (uint32_t i = 0; i < node->primitiveCount; i++) {
    DrawInfo draw = model->draws[node->primitiveIndex + i];
    if (node->skin == ~0u) draw.transform = globalTransform;
    draw.instances = instances;
    if (!lovrPassDraw(pass, &draw)) return false;
  }

  for (uint32_t i = 0; i < node->childCount; i++) {
    if (!drawNode(pass, model, node->children[i], instances)) {
      return false;
    }
  }

  return true;
}

bool lovrPassDrawModel(Pass* pass, Model* model, float* transform, uint32_t instances) {
  if (!lovrModelAnimateVertices(model)) {
    return false;
  }

  if (model->transformsDirty) {
    updateModelTransforms(model, model->info.data->rootNode, (float[]) MAT4_IDENTITY);
    model->transformsDirty = false;
  }

  if (!lovrPassPush(pass, STACK_TRANSFORM)) return false;
  lovrPassTransform(pass, transform);
  if (!drawNode(pass, model, model->info.data->rootNode, instances)) return false;
  if (!lovrPassPop(pass, STACK_TRANSFORM)) return false;
  return true;
}

bool lovrPassDrawTexture(Pass* pass, Texture* texture, float* transform) {
  uint32_t key[] = { SHAPE_PLANE, STYLE_FILL, 1, 1 };
  ShapeVertex* vertices;
  uint16_t* indices;

  float aspect = (float) texture->info.height / texture->info.width;
  transform[4] *= aspect;
  transform[5] *= aspect;
  transform[6] *= aspect;
  transform[7] *= aspect;

  ShapeVertex vertexData[] = {
    { { -.5f,  .5f, 0.f }, { 0.f, 0.f, 1.f }, { 0.f, 0.f } },
    { {  .5f,  .5f, 0.f }, { 0.f, 0.f, 1.f }, { 1.f, 0.f } },
    { { -.5f, -.5f, 0.f }, { 0.f, 0.f, 1.f }, { 0.f, 1.f } },
    { {  .5f, -.5f, 0.f }, { 0.f, 0.f, 1.f }, { 1.f, 1.f } }
  };

  uint16_t indexData[] = { 0, 2, 1, 1, 2, 3 };

  DrawInfo draw = {
    .hash = hash64(key, sizeof(key)),
    .mode = DRAW_TRIANGLES,
    .transform = transform,
    .bounds = (float[6]) { 0.f, 0.f, 0.f, .5f, .5f, 0.f },
    .material = lovrTextureToMaterial(texture),
    .vertex.pointer = (void**) &vertices,
    .vertex.count = COUNTOF(vertexData),
    .index.pointer = (void**) &indices,
    .index.count = COUNTOF(indexData)
  };

  if (!lovrPassDraw(pass, &draw)) {
    return false;
  }

  if (vertices) {
    memcpy(vertices, vertexData, sizeof(vertexData));
    memcpy(indices, indexData, sizeof(indexData));
  }

  return true;
}

bool lovrPassMesh(Pass* pass, Buffer* vertices, Buffer* indices, float* transform, uint32_t start, uint32_t count, uint32_t instances, uint32_t baseVertex) {
  lovrCheck(!indices || indices->info.format, "Buffer must have been created with a format to use it as a%s buffer", "n index");
  lovrCheck(!vertices || vertices->info.format, "Buffer must have been created with a format to use it as a%s buffer", " vertex");
  lovrCheck(!vertices || vertices->supportsMesh, "Vertex buffer has invalid format (can not contain nested structs/arrays, or matrix/index types)");

  if (count == ~0u) {
    if (indices || vertices) {
      Buffer* buffer = indices ? indices : vertices;
      count = buffer->info.format->length - start;
    } else {
      count = 0;
    }
  } else if (indices) {
    lovrCheck(count <= indices->info.format->length - start, "Mesh draw range exceeds index buffer size");
  } else if (vertices) {
    lovrCheck(count <= vertices->info.format->length - start, "Mesh draw range exceeds vertex buffer size");
  }

  return lovrPassDraw(pass, &(DrawInfo) {
    .mode = pass->pipeline->mode,
    .vertex.buffer = vertices,
    .index.buffer = indices,
    .transform = transform,
    .start = start,
    .count = count,
    .instances = instances,
    .baseVertex = baseVertex
  });
}

bool lovrPassMeshIndirect(Pass* pass, Buffer* vertices, Buffer* indices, Buffer* draws, uint32_t count, uint32_t offset, uint32_t stride) {
  stride = stride ? stride : (indices ? 20 : 16);
  Shader* shader = pass->pipeline->shader;

  lovrCheck(shader, "A custom Shader must be bound to source draws from a Buffer");
  lovrCheck(offset % 4 == 0, "Draw Buffer offset must be a multiple of 4");
  lovrCheck(offset + count * stride < draws->info.size, "Draw buffer range exceeds the size of the buffer");
  lovrCheck(!vertices || vertices->supportsMesh, "Vertex buffer has invalid format (can not contain nested structs/arrays, or matrix/index types)");

  DrawInfo info = {
    .mode = pass->pipeline->mode,
    .vertex.buffer = vertices,
    .index.buffer = indices
  };

  if (pass->drawCount >= pass->drawCapacity) {
    lovrAssert(pass->drawCount < 1 << 16, "Pass has too many draws!");
    pass->drawCapacity = pass->drawCapacity > 0 ? pass->drawCapacity << 1 : 1;
    Draw* draws = lovrPassAllocate(pass, pass->drawCapacity * sizeof(Draw));
    memcpy(draws, pass->draws, pass->drawCount * sizeof(Draw));
    pass->draws = draws;
  }

  Draw* previous = pass->drawCount > 0 ? &pass->draws[pass->drawCount - 1] : NULL;
  Draw* draw = &pass->draws[pass->drawCount++];

  draw->flags = DRAW_INDIRECT;
  draw->tally = pass->tally.active ? pass->tally.count : 0xff;
  draw->camera = pass->cameraCount - 1;
  draw->viewport = pass->viewportCount - 1;
  draw->scissor = pass->scissorCount - 1;
  pass->flags &= ~DIRTY_CAMERA & ~DIRTY_VIEWPORT & ~DIRTY_SCISSOR;

  draw->shader = shader;
  draw->material = pass->pipeline->material;
  if (!draw->material) draw->material = state.defaultMaterial;
  trackMaterial(pass, draw->material);

  draw->indirect.buffer = draws->gpu;
  draw->indirect.offset = draws->base + offset;
  draw->indirect.count = count;
  draw->indirect.stride = stride;

  lovrPassResolvePipeline(pass, &info, draw, previous);
  draw->bundleInfo = lovrPassResolveBindings(pass, shader, previous ? previous->bundleInfo : NULL);
  if (!lovrPassResolveUniforms(pass, draw->shader, &draw->uniformBuffer, &draw->uniformOffset, previous)) return false;
  if (!lovrPassResolveVertices(pass, &info, draw)) return false;

  mat4_init(draw->transform, pass->transform);
  memcpy(draw->color, pass->pipeline->color, 4 * sizeof(float));

  trackBuffer(pass, draws, GPU_PHASE_INDIRECT, GPU_CACHE_INDIRECT);
  lovrRetain(draw->material);
  lovrRetain(shader);
  return true;
}

bool lovrPassBeginTally(Pass* pass, uint32_t* index) {
  lovrCheck(pass->tally.count < MAX_TALLIES, "Pass has too many tallies!");
  lovrCheck(!pass->tally.active, "Trying to start a tally, but the previous tally wasn't finished");
  pass->tally.active = true;
  if (index) *index = pass->tally.count;
  return true;
}

bool lovrPassFinishTally(Pass* pass, uint32_t* index) {
  lovrCheck(pass->tally.active, "Trying to finish a tally, but no tally was started");
  pass->tally.active = false;
  if (index) *index = pass->tally.count++;
  return true;
}

Buffer* lovrPassGetTallyBuffer(Pass* pass, uint32_t* offset) {
  *offset = pass->tally.bufferOffset;
  return pass->tally.buffer;
}

bool lovrPassSetTallyBuffer(Pass* pass, Buffer* buffer, uint32_t offset) {
  lovrCheck(offset % 4 == 0, "Tally buffer offset must be a multiple of 4");
  lovrRelease(pass->tally.buffer, lovrBufferDestroy);
  pass->tally.buffer = buffer;
  pass->tally.bufferOffset = offset;
  lovrRetain(buffer);
  return true;
}

bool lovrPassCompute(Pass* pass, uint32_t x, uint32_t y, uint32_t z, Buffer* indirect, uint32_t offset) {
  if ((pass->computeCount & (pass->computeCount - 1)) == 0) {
    Compute* computes = lovrPassAllocate(pass, MAX(pass->computeCount << 1, 1) * sizeof(Compute));
    if (pass->computes) memcpy(computes, pass->computes, pass->computeCount * sizeof(Compute));
    pass->computes = computes;
  }

  Compute* previous = pass->computeCount > 0 ? &pass->computes[pass->computeCount - 1] : NULL;
  Compute* compute = &pass->computes[pass->computeCount++];
  Shader* shader = pass->pipeline->shader;

  lovrCheck(shader->info.type == SHADER_COMPUTE, "To run a compute shader, a compute shader must be active");
  lovrCheck(x <= state.limits.workgroupCount[0], "Compute %s count exceeds workgroupCount limit", "x");
  lovrCheck(y <= state.limits.workgroupCount[1], "Compute %s count exceeds workgroupCount limit", "y");
  lovrCheck(z <= state.limits.workgroupCount[2], "Compute %s count exceeds workgroupCount limit", "z");

  compute->flags = 0;
  compute->shader = shader;
  compute->bundleInfo = lovrPassResolveBindings(pass, shader, previous ? previous->bundleInfo : NULL);
  if (!lovrPassResolveUniforms(pass, shader, &compute->uniformBuffer, &compute->uniformOffset, previous)) return false;
  lovrRetain(shader);

  if (indirect) {
    compute->flags |= COMPUTE_INDIRECT;
    compute->indirect.buffer = indirect->gpu;
    compute->indirect.offset = indirect->base + offset;
    trackBuffer(pass, indirect, GPU_PHASE_INDIRECT, GPU_CACHE_INDIRECT);
  } else {
    compute->x = x;
    compute->y = y;
    compute->z = z;
  }

  return true;
}

void lovrPassBarrier(Pass* pass) {
  if (pass->computeCount > 0) {
    pass->computes[pass->computeCount - 1].flags |= COMPUTE_BARRIER;
  }
}

// Helpers

static void* tempAlloc(Allocator* allocator, size_t size) {
  if (size == 0) {
    return NULL;
  }

  while (allocator->cursor + size > allocator->length) {
    if ((allocator->length << 1) > allocator->limit) {
      fprintf(stderr, "Out of memory");
      abort();
    }
    os_vm_commit(allocator->memory + allocator->length, allocator->length);
    allocator->length <<= 1;
  }

  uint32_t cursor = ALIGN(allocator->cursor, 8);
  allocator->cursor = cursor + size;
  return allocator->memory + cursor;
}

static size_t tempPush(Allocator* allocator) {
  return allocator->cursor;
}

static void tempPop(Allocator* allocator, size_t stack) {
  allocator->cursor = stack;
}

static gpu_pipeline* getPipeline(uint32_t index) {
  return (gpu_pipeline*) ((char*) state.pipelines + index * gpu_sizeof_pipeline());
}

static BufferBlock* getBlock(gpu_buffer_type type, uint32_t size) {
  BufferBlock* block = state.bufferAllocators[type].freelist;

  if (block && block->size >= size && gpu_is_complete(block->tick)) {
    state.bufferAllocators[type].freelist = block->next;
    block->next = NULL;
    return block;
  }

  block = lovrMalloc(sizeof(BufferBlock) + gpu_sizeof_buffer());
  block->handle = (gpu_buffer*) (block + 1);
  block->size = MAX(size, 1 << 22);
  block->next = NULL;
  block->ref = 0;

  gpu_buffer_info info = {
    .type = type,
    .size = block->size,
    .pointer = &block->pointer,
    .label = "Buffer Block"
  };

  if (!gpu_buffer_init(block->handle, &info)) {
    lovrSetError("Failed to create GPU buffer: %s", gpu_get_error());
    lovrFree(block);
    return NULL;
  }

  return block;
}

static void freeBlock(BufferAllocator* allocator, BufferBlock* block) {
  BufferBlock** list = &allocator->freelist;
  while (*list) list = (BufferBlock**) &(*list)->next;
  block->next = NULL;
  *list = block;
}

static BufferView allocateBuffer(BufferAllocator* allocator, gpu_buffer_type type, uint32_t size, size_t align) {
  uint32_t cursor = (uint32_t) ((allocator->cursor + (align - 1)) / align * align);
  BufferBlock* block = allocator->current;

  if (!block || cursor + size > block->size) {
    if (block && type != GPU_BUFFER_STATIC) {
      block->tick = state.tick;
      freeBlock(allocator, block);
    }

    if ((block = getBlock(type, size)) == NULL) {
      return (BufferView) { 0 };
    }

    allocator->current = block;
    cursor = 0;
  }

  allocator->cursor = cursor + size;

  return (BufferView) {
    .block = block,
    .buffer = block->handle,
    .offset = cursor,
    .extent = size,
    .pointer = block->pointer ? (char*) block->pointer + cursor : NULL
  };
}

static BufferView getBuffer(gpu_buffer_type type, uint32_t size, size_t align) {
  return allocateBuffer(&state.bufferAllocators[type], type, size, align);
}

static int u64cmp(const void* a, const void* b) {
  uint64_t x = *(uint64_t*) a, y = *(uint64_t*) b;
  return (x > y) - (x < y);
}

static uint32_t gcd(uint32_t a, uint32_t b) {
  return b ? gcd(b, a % b) : a;
}

static uint32_t lcm(uint32_t a, uint32_t b) {
  return (a / gcd(a, b)) * b;
}

static bool beginFrame(void) {
  if (!state.active) {
    if (!gpu_begin(&state.tick)) {
      return lovrSetError("Failed to begin GPU frame: %s", gpu_get_error());
    }

    state.active = true;
    memset(&state.barrier, 0, sizeof(gpu_barrier));
    memset(&state.transferBarrier, 0, sizeof(gpu_barrier));
    state.allocator.cursor = 0;
    processReadbacks();
  }

  if (!state.stream && (state.stream = gpu_stream_begin("Internal")) == NULL) {
    return lovrSetError("Failed to begin command buffer: %s", gpu_get_error());
  }

  return true;
}

// When a Texture is garbage collected, if it has any transfer operations recorded to state.stream,
// those transfers need to be submitted before it gets destroyed.  The allocator offset is saved and
// restored, which is pretty gross, but we don't want to invalidate temp memory (currently this is
// only a problem for Font: when the font's atlas gets destroyed, it could invalidate the temp
// memory used by Font:getLines and Pass:text).
static void flushTransfers(void) {
  if (state.active) {
    size_t cursor = state.allocator.cursor;
    lovrGraphicsSubmit(NULL, 0);
    beginFrame();
    state.allocator.cursor = cursor;
  }
}

static void processReadbacks(void) {
  while (state.oldestReadback && gpu_is_complete(state.oldestReadback->tick)) {
    Readback* readback = state.oldestReadback;

    switch (readback->type) {
      case READBACK_BUFFER:
        memcpy(readback->blob->data, readback->view.pointer, readback->view.extent);
        break;
      case READBACK_TEXTURE:;
        size_t size = lovrImageGetLayerSize(readback->image, 0);
        void* data = lovrImageGetLayerData(readback->image, 0, 0);
        memcpy(data, readback->view.pointer, size);
        break;
      case READBACK_TIMESTAMP:;
        uint32_t* timestamps = readback->view.pointer;
        for (uint32_t i = 0; i < readback->count; i++) {
          Pass* pass = readback->times[i].pass;
          pass->stats.submitTime = readback->times[i].cpuTime;
          pass->stats.gpuTime = (timestamps[2 * i + 1] - timestamps[2 * i + 0]) * state.limits.timestampPeriod / 1e9;
        }
        break;
      default: break;
    }

    Readback* next = readback->next;
    lovrRelease(readback, lovrReadbackDestroy);
    state.oldestReadback = next;
  }

  if (!state.oldestReadback) {
    state.newestReadback = NULL;
  }
}

static gpu_pass* getPass(Canvas* canvas) {
  gpu_pass_info info = { 0 };

  for (uint32_t i = 0; i < canvas->count; i++) {
    info.color[i].format = (gpu_texture_format) canvas->color[i].texture->info.format;
    info.color[i].srgb = canvas->color[i].texture->info.srgb;
    info.color[i].load = canvas->resolve ? GPU_LOAD_OP_CLEAR : (gpu_load_op) canvas->color[i].load;
  }

  DepthAttachment* depth = &canvas->depth;

  if (depth->texture || depth->format) {
    info.depth.format = (gpu_texture_format) (depth->texture ? depth->texture->info.format : depth->format);
    info.depth.load = (gpu_load_op) canvas->resolve ? GPU_LOAD_OP_CLEAR : (gpu_load_op) depth->load;
    info.depth.save = depth->texture ? GPU_SAVE_OP_KEEP : GPU_SAVE_OP_DISCARD;
    info.depth.stencilLoad = info.depth.load;
    info.depth.stencilSave = info.depth.save;
  }

  info.colorCount = canvas->count;
  info.samples = canvas->samples;
  info.views = canvas->views;
  info.resolveColor = canvas->resolve;
  info.resolveDepth = canvas->resolve && !!depth->texture;
  info.surface = canvas->count > 0 && canvas->color[0].texture == state.window;

  uint64_t hash = hash64(&info, sizeof(info));
  uint64_t value = map_get(&state.passLookup, hash);

  if (value == MAP_NIL) {
    gpu_pass* pass = lovrMalloc(gpu_sizeof_pass());

    if (!gpu_pass_init(pass, &info)) {
      lovrFree(pass);
      return NULL;
    }

    map_set(&state.passLookup, hash, (uint64_t) (uintptr_t) pass);
    return pass;
  }

  return (gpu_pass*) (uintptr_t) value;
}

static Layout* getLayout(gpu_slot* slots, uint32_t count) {
  uint64_t hash = hash64(slots, count * sizeof(gpu_slot));

  for (Layout* layout = state.layouts; layout; layout = layout->next) {
    if (layout->hash == hash) {
      return layout;
    }
  }

  Layout* layout = lovrCalloc(sizeof(Layout) + gpu_sizeof_layout());

  layout->hash = hash;
  layout->gpu = (gpu_layout*) (layout + 1);

  gpu_layout_info info = {
    .slots = slots,
    .count = count
  };

  if (!gpu_layout_init(layout->gpu, &info)) {
    lovrSetError("Failed to create GPU layout: %s", gpu_get_error());
    lovrFree(layout);
    return NULL;
  }

  layout->next = state.layouts;
  state.layouts = layout;
  return layout;
}

static gpu_bundle* getBundle(Layout* layout, gpu_binding* bindings, uint32_t count) {
  BundlePool* pool = layout->head;
  const uint32_t POOL_SIZE = 512;
  gpu_bundle* bundle = NULL;

  if (pool) {
    if (pool->cursor < POOL_SIZE) {
      bundle = (gpu_bundle*) ((char*) pool->bundles + gpu_sizeof_bundle() * pool->cursor++);
      goto write;
    }

    // If the pool's closed, move it to the end of the list and try to use the next pool
    layout->tail->next = pool;
    layout->tail = pool;
    layout->head = pool->next;
    pool->next = NULL;
    pool->tick = state.tick;
    pool = layout->head;

    if (pool && gpu_is_complete(pool->tick)) {
      bundle = pool->bundles;
      pool->cursor = 1;
      goto write;
    }
  }

  // If no pool was available, make a new one
  pool = lovrMalloc(sizeof(BundlePool));
  gpu_bundle_pool* gpu = lovrMalloc(gpu_sizeof_bundle_pool());
  gpu_bundle* bundles = lovrMalloc(POOL_SIZE * gpu_sizeof_bundle());
  pool->gpu = gpu;
  pool->bundles = bundles;
  pool->cursor = 1;
  pool->next = layout->head;

  gpu_bundle_pool_info info = {
    .bundles = pool->bundles,
    .layout = layout->gpu,
    .count = POOL_SIZE
  };

  if (!gpu_bundle_pool_init(pool->gpu, &info)) {
    lovrSetError("Failed to allocate descriptor set: %s", gpu_get_error());
    lovrFree(bundles);
    lovrFree(gpu);
    lovrFree(pool);
    return NULL;
  }

  layout->head = pool;
  if (!layout->tail) layout->tail = pool;
  bundle = pool->bundles;
write:
  gpu_bundle_write(&bundle, &(gpu_bundle_info) { layout->gpu, bindings, count }, 1);
  return bundle;
}

static gpu_texture* getScratchTexture(gpu_stream* stream, Canvas* canvas, TextureFormat format, bool srgb) {
  uint16_t key[] = { canvas->width, canvas->height, canvas->views, format, srgb, canvas->samples };
  uint32_t hash = (uint32_t) hash64(key, sizeof(key));

  // Find a matching scratch texture that hasn't been used this frame
  for (uint32_t i = 0; i < state.scratchTextures.length; i++) {
    if (state.scratchTextures.data[i].hash == hash && state.scratchTextures.data[i].tick != state.tick) {
      return state.scratchTextures.data[i].texture;
    }
  }

  // Find something to evict
  ScratchTexture* scratch = NULL;
  for (uint32_t i = 0; i < state.scratchTextures.length; i++) {
    if (state.tick - state.scratchTextures.data[i].tick > 16) {
      scratch = &state.scratchTextures.data[i];
      break;
    }
  }

  if (scratch) {
    gpu_texture_destroy(scratch->texture);
  } else {
    arr_expand(&state.scratchTextures, 1);
    scratch = &state.scratchTextures.data[state.scratchTextures.length++];
    scratch->texture = lovrCalloc(gpu_sizeof_texture());
  }

  gpu_texture_info info = {
    .type = GPU_TEXTURE_ARRAY,
    .format = (gpu_texture_format) format,
    .srgb = srgb,
    .size = { canvas->width, canvas->height, canvas->views },
    .mipmaps = 1,
    .samples = canvas->samples,
    .usage = GPU_TEXTURE_RENDER,
    .upload.stream = stream
  };

  gpu_texture_init(scratch->texture, &info);
  scratch->hash = hash;
  scratch->tick = state.tick;
  return scratch->texture;
}

static bool isDepthFormat(TextureFormat format) {
  return format == FORMAT_D16 || format == FORMAT_D24 || format == FORMAT_D32F || format == FORMAT_D24S8 || format == FORMAT_D32FS8;
}

static bool supportsSRGB(TextureFormat format) {
  switch (format) {
    case FORMAT_R8:
    case FORMAT_RG8:
    case FORMAT_RGBA8:
    case FORMAT_BC1:
    case FORMAT_BC2:
    case FORMAT_BC3:
    case FORMAT_BC7:
    case FORMAT_ASTC_4x4:
    case FORMAT_ASTC_5x4:
    case FORMAT_ASTC_5x5:
    case FORMAT_ASTC_6x5:
    case FORMAT_ASTC_6x6:
    case FORMAT_ASTC_8x5:
    case FORMAT_ASTC_8x6:
    case FORMAT_ASTC_8x8:
    case FORMAT_ASTC_10x5:
    case FORMAT_ASTC_10x6:
    case FORMAT_ASTC_10x8:
    case FORMAT_ASTC_10x10:
    case FORMAT_ASTC_12x10:
    case FORMAT_ASTC_12x12:
      return true;
    default:
      return false;
  }
}

// Returns number of bytes of a 3D texture region of a given format
static uint32_t measureTexture(TextureFormat format, uint32_t w, uint32_t h, uint32_t d) {
  switch (format) {
    case FORMAT_R8: return w * h * d;
    case FORMAT_RG8:
    case FORMAT_R16:
    case FORMAT_R16F:
    case FORMAT_RGB565:
    case FORMAT_RGB5A1:
    case FORMAT_D16: return w * h * d * 2;
    case FORMAT_RGBA8:
    case FORMAT_RG16:
    case FORMAT_RG16F:
    case FORMAT_R32F:
    case FORMAT_RG11B10F:
    case FORMAT_RGB10A2:
    case FORMAT_D24:
    case FORMAT_D24S8:
    case FORMAT_D32F: return w * h * d * 4;
    case FORMAT_D32FS8: return w * h * d * 5;
    case FORMAT_RGBA16:
    case FORMAT_RGBA16F:
    case FORMAT_RG32F: return w * h * d * 8;
    case FORMAT_RGBA32F: return w * h * d * 16;
    case FORMAT_BC1: return ((w + 3) / 4) * ((h + 3) / 4) * d * 8;
    case FORMAT_BC2: return ((w + 3) / 4) * ((h + 3) / 4) * d * 16;
    case FORMAT_BC3: return ((w + 3) / 4) * ((h + 3) / 4) * d * 16;
    case FORMAT_BC4U: return ((w + 3) / 4) * ((h + 3) / 4) * d * 8;
    case FORMAT_BC4S: return ((w + 3) / 4) * ((h + 3) / 4) * d * 8;
    case FORMAT_BC5U: return ((w + 3) / 4) * ((h + 3) / 4) * d * 16;
    case FORMAT_BC5S: return ((w + 3) / 4) * ((h + 3) / 4) * d * 16;
    case FORMAT_BC6UF: return ((w + 3) / 4) * ((h + 3) / 4) * d * 16;
    case FORMAT_BC6SF: return ((w + 3) / 4) * ((h + 3) / 4) * d * 16;
    case FORMAT_BC7: return ((w + 3) / 4) * ((h + 3) / 4) * d * 16;
    case FORMAT_ASTC_4x4: return ((w + 3) / 4) * ((h + 3) / 4) * d * 16;
    case FORMAT_ASTC_5x4: return ((w + 4) / 5) * ((h + 3) / 4) * d * 16;
    case FORMAT_ASTC_5x5: return ((w + 4) / 5) * ((h + 4) / 5) * d * 16;
    case FORMAT_ASTC_6x5: return ((w + 5) / 6) * ((h + 4) / 5) * d * 16;
    case FORMAT_ASTC_6x6: return ((w + 5) / 6) * ((h + 5) / 6) * d * 16;
    case FORMAT_ASTC_8x5: return ((w + 7) / 8) * ((h + 4) / 5) * d * 16;
    case FORMAT_ASTC_8x6: return ((w + 7) / 8) * ((h + 5) / 6) * d * 16;
    case FORMAT_ASTC_8x8: return ((w + 7) / 8) * ((h + 7) / 8) * d * 16;
    case FORMAT_ASTC_10x5: return ((w + 9) / 10) * ((h + 4) / 5) * d * 16;
    case FORMAT_ASTC_10x6: return ((w + 9) / 10) * ((h + 5) / 6) * d * 16;
    case FORMAT_ASTC_10x8: return ((w + 9) / 10) * ((h + 7) / 8) * d * 16;
    case FORMAT_ASTC_10x10: return ((w + 9) / 10) * ((h + 9) / 10) * d * 16;
    case FORMAT_ASTC_12x10: return ((w + 11) / 12) * ((h + 9) / 10) * d * 16;
    case FORMAT_ASTC_12x12: return ((w + 11) / 12) * ((h + 11) / 12) * d * 16;
    default: lovrUnreachable();
  }
}

// Check if a 3D texture region is within the texture's bounds
static bool checkTextureBounds(const TextureInfo* info, uint32_t offset[4], uint32_t extent[3]) {
  uint32_t maxWidth = MAX(info->width >> offset[3], 1);
  uint32_t maxHeight = MAX(info->height >> offset[3], 1);
  uint32_t maxLayers = info->type == TEXTURE_3D ? MAX(info->layers >> offset[3], 1) : info->layers;
  lovrCheck(offset[0] + extent[0] <= maxWidth, "Texture x range [%d,%d] exceeds width (%d)", offset[0], offset[0] + extent[0], maxWidth);
  lovrCheck(offset[1] + extent[1] <= maxHeight, "Texture y range [%d,%d] exceeds height (%d)", offset[1], offset[1] + extent[1], maxHeight);
  lovrCheck(offset[2] + extent[2] <= maxLayers, "Texture layer range [%d,%d] exceeds layer count (%d)", offset[2], offset[2] + extent[2], maxLayers);
  lovrCheck(offset[3] < info->mipmaps, "Texture mipmap %d exceeds its mipmap count (%d)", offset[3] + 1, info->mipmaps);
  return true;
}

static void mipmapTexture(gpu_stream* stream, Texture* texture, uint32_t base, uint32_t count) {
  if (count == ~0u) count = texture->info.mipmaps - (base + 1);
  bool volumetric = texture->info.type == TEXTURE_3D;
  for (uint32_t i = 0; i < count; i++) {
    uint32_t level = base + i + 1;
    uint32_t srcOffset[4] = { 0, 0, 0, level - 1 };
    uint32_t dstOffset[4] = { 0, 0, 0, level };
    uint32_t srcExtent[3] = {
      MAX(texture->info.width >> (level - 1), 1),
      MAX(texture->info.height >> (level - 1), 1),
      volumetric ? MAX(texture->info.layers >> (level - 1), 1) : 1
    };
    uint32_t dstExtent[3] = {
      MAX(texture->info.width >> level, 1),
      MAX(texture->info.height >> level, 1),
      volumetric ? MAX(texture->info.layers >> level, 1) : 1
    };
    gpu_blit(stream, texture->root->gpu, texture->root->gpu, srcOffset, dstOffset, srcExtent, dstExtent, GPU_FILTER_LINEAR);
    if (i != count - 1) {
      gpu_sync(stream, &(gpu_barrier) {
        .prev = GPU_PHASE_BLIT,
        .next = GPU_PHASE_BLIT,
        .flush = GPU_CACHE_TRANSFER_WRITE,
        .clear = GPU_CACHE_TRANSFER_READ
      }, 1);
    }
  }
}

static ShaderResource* findShaderResource(Shader* shader, const char* name, size_t length) {
  uint32_t hash = (uint32_t) hash64(name, length);
  for (uint32_t i = 0; i < shader->resourceCount; i++) {
    if (shader->resources[i].hash == hash) {
      return &shader->resources[i];
    }
  }
  return NULL;
}

static Access* getNextAccess(Pass* pass, int type, bool texture) {
  AccessBlock* block = pass->access[type];

  if (!block || block->count >= COUNTOF(block->list)) {
    AccessBlock* new = lovrPassAllocate(pass, sizeof(AccessBlock));
    pass->access[type] = new;
    new->next = block;
    new->count = 0;
    new->textureMask = 0;
    block = new;
  }

  block->textureMask |= (uint64_t) texture << block->count;
  return &block->list[block->count++];
}

static void trackBuffer(Pass* pass, Buffer* buffer, gpu_phase phase, gpu_cache cache) {
  if (!buffer) return;
  Access* access = getNextAccess(pass, phase == GPU_PHASE_SHADER_COMPUTE ? ACCESS_COMPUTE : ACCESS_RENDER, false);
  access->sync = &buffer->sync;
  access->object = buffer;
  access->phase = phase;
  access->cache = cache;
  lovrRetain(buffer);
}

static void trackTexture(Pass* pass, Texture* texture, gpu_phase phase, gpu_cache cache) {
  if (!texture) return;

  // Sample-only textures can skip sync, but still need to be refcounted
  if (texture->root->info.usage == TEXTURE_SAMPLE) {
    phase = 0;
    cache = 0;
  }

  Access* access = getNextAccess(pass, phase == GPU_PHASE_SHADER_COMPUTE ? ACCESS_COMPUTE : ACCESS_RENDER, true);
  access->sync = &texture->root->sync;
  access->object = texture;
  access->phase = phase;
  access->cache = cache;
  lovrRetain(texture);
}

static void trackMaterial(Pass* pass, Material* material) {
  if (!material->hasWritableTexture) {
    return;
  }

  gpu_phase phase = GPU_PHASE_SHADER_VERTEX | GPU_PHASE_SHADER_FRAGMENT;
  gpu_cache cache = GPU_CACHE_TEXTURE;

  trackTexture(pass, material->info.texture, phase, cache);
  trackTexture(pass, material->info.glowTexture, phase, cache);
  trackTexture(pass, material->info.metalnessTexture, phase, cache);
  trackTexture(pass, material->info.roughnessTexture, phase, cache);
  trackTexture(pass, material->info.clearcoatTexture, phase, cache);
  trackTexture(pass, material->info.occlusionTexture, phase, cache);
  trackTexture(pass, material->info.normalTexture, phase, cache);
}

static bool syncResource(Access* access, gpu_barrier* barrier) {
  // There are 4 types of access patterns:
  // - read after read:
  //   - no hazard, no barrier necessary
  // - read after write:
  //   - needs execution dependency to ensure the read happens after the write
  //   - needs to flush the writes from the cache
  //   - needs to clear the cache for the read so it gets the new data
  //   - only needs to happen once for each type of read after a write (tracked by pendingReads)
  //     - if a second read happens, the first read would have already synchronized (transitive)
  // - write after write:
  //   - needs execution dependency to ensure writes don't overlap
  //   - needs to flush and clear the cache
  //   - clears pendingReads
  // - write after read:
  //   - needs execution dependency to ensure write starts after read is finished
  //   - does not need to flush any caches
  //   - does clear the write cache
  //   - clears pendingReads

  Sync* sync = access->sync;
  uint32_t read = access->cache & GPU_CACHE_READ_MASK;
  uint32_t write = access->cache & GPU_CACHE_WRITE_MASK;
  uint32_t newReads = read & ~sync->pendingReads;
  bool hasNewReads = newReads || (access->phase & ~sync->readPhase);
  bool readAfterWrite = read && sync->pendingWrite && hasNewReads;
  bool writeAfterWrite = write && sync->pendingWrite && !sync->pendingReads;
  bool writeAfterRead = write && sync->pendingReads;

  if (readAfterWrite) {
    barrier->prev |= sync->writePhase;
    barrier->next |= access->phase;
    barrier->flush |= sync->pendingWrite;
    barrier->clear |= newReads;
    sync->readPhase |= access->phase;
    sync->pendingReads |= read;
  }

  if (writeAfterWrite) {
    barrier->prev |= sync->writePhase;
    barrier->next |= access->phase;
    barrier->flush |= sync->pendingWrite;
    barrier->clear |= write;
  }

  if (writeAfterRead) {
    barrier->prev |= sync->readPhase;
    barrier->next |= access->phase;
    sync->readPhase = 0;
    sync->pendingReads = 0;
  }

  if (write) {
    sync->writePhase = access->phase;
    sync->pendingWrite = write;
  }

  return write;
}

static gpu_barrier syncTransfer(Sync* sync, gpu_phase phase, gpu_cache cache) {
  gpu_barrier localBarrier = { 0 };
  gpu_barrier* barrier = NULL;

  // If there was already a transfer write to the resource this frame, a "just in time" barrier is required
  // If this is a transfer write, a "just in time" barrier is only needed if there's been a transfer read this frame
  // Otherwise, the barrier can go at the beginning of the frame and get batched with other barriers
  if (sync->lastTransferWrite == state.tick || (sync->lastTransferRead == state.tick && (cache & GPU_CACHE_WRITE_MASK))) {
    barrier = &localBarrier;
  } else {
    barrier = &state.transferBarrier;
  }

  syncResource(&(Access) { sync, NULL, phase, cache }, barrier);
  if (cache & GPU_CACHE_READ_MASK) sync->lastTransferRead = state.tick;
  if (cache & GPU_CACHE_WRITE_MASK) sync->lastTransferWrite = state.tick;

  return localBarrier;
}

static void updateModelTransforms(Model* model, uint32_t nodeIndex, float* parent) {
  mat4 global = model->globalTransforms + 16 * nodeIndex;
  NodeTransform* local = &model->localTransforms[nodeIndex];

  mat4_init(global, parent);
  mat4_translate(global, local->position[0], local->position[1], local->position[2]);
  mat4_rotateQuat(global, local->rotation);
  mat4_scale(global, local->scale[0], local->scale[1], local->scale[2]);

  ModelNode* node = &model->info.data->nodes[nodeIndex];
  for (uint32_t i = 0; i < node->childCount; i++) {
    updateModelTransforms(model, node->children[i], global);
  }
}

// Only an explicit set of SPIR-V capabilities are allowed
// Some capabilities require a GPU feature to be supported
// Some common unsupported capabilities are checked directly, to provide better error messages
static bool checkShaderFeatures(uint32_t* features, uint32_t count) {
  for (uint32_t i = 0; i < count; i++) {
    switch (features[i]) {
      case 0: break; // Matrix
      case 1: break; // Shader
      case 2: return lovrSetError("Shader uses unsupported feature #%d: %s", features[i], "geometry shading");
      case 3: return lovrSetError("Shader uses unsupported feature #%d: %s", features[i], "tessellation shading");
      case 5: return lovrSetError("Shader uses unsupported feature #%d: %s", features[i], "linkage");
      case 9: return lovrSetError("Shader uses unsupported feature #%d: %s", features[i], "half floats");
      case 10: lovrCheck(state.features.float64, "GPU does not support shader feature #%d: %s", features[i], "64 bit floats"); break;
      case 11: lovrCheck(state.features.int64, "GPU does not support shader feature #%d: %s", features[i], "64 bit integers"); break;
      case 12: return lovrSetError("Shader uses unsupported feature #%d: %s", features[i], "64 bit atomics");
      case 22: lovrCheck(state.features.int16, "GPU does not support shader feature #%d: %s", features[i], "16 bit integers"); break;
      case 23: return lovrSetError("Shader uses unsupported feature #%d: %s", features[i], "tessellation shading");
      case 24: return lovrSetError("Shader uses unsupported feature #%d: %s", features[i], "geometry shading");
      case 25: return lovrSetError("Shader uses unsupported feature #%d: %s", features[i], "extended image gather");
      case 27: return lovrSetError("Shader uses unsupported feature #%d: %s", features[i], "multisample storage textures");
      case 32: lovrCheck(state.limits.clipDistances > 0, "GPU does not support shader feature #%d: %s", features[i], "clip distance"); break;
      case 33: lovrCheck(state.limits.cullDistances > 0, "GPU does not support shader feature #%d: %s", features[i], "cull distance"); break;
      case 34: return lovrSetError("Shader uses unsupported feature #%d: %s", features[i], "cubemap array textures");
      case 35: return lovrSetError("Shader uses unsupported feature #%d: %s", features[i], "sample rate shading");
      case 36: return lovrSetError("Shader uses unsupported feature #%d: %s", features[i], "rectangle textures");
      case 37: return lovrSetError("Shader uses unsupported feature #%d: %s", features[i], "rectangle textures");
      case 39: return lovrSetError("Shader uses unsupported feature #%d: %s", features[i], "8 bit integers");
      case 40: return lovrSetError("Shader uses unsupported feature #%d: %s", features[i], "input attachments");
      case 41: return lovrSetError("Shader uses unsupported feature #%d: %s", features[i], "sparse residency");
      case 42: return lovrSetError("Shader uses unsupported feature #%d: %s", features[i], "min LOD");
      case 43: return lovrSetError("Shader uses unsupported feature #%d: %s", features[i], "1D textures");
      case 44: return lovrSetError("Shader uses unsupported feature #%d: %s", features[i], "1D textures");
      case 45: break; // Cubemap arrays
      case 46: return lovrSetError("Shader uses unsupported feature #%d: %s", features[i], "texel buffers");
      case 47: return lovrSetError("Shader uses unsupported feature #%d: %s", features[i], "texel buffers");
      case 48: return lovrSetError("Shader uses unsupported feature #%d: %s", features[i], "multisampled storage textures");
      case 49: break; // StorageImageExtendedFormats (?)
      case 50: break; // ImageQuery
      case 51: break; // DerivativeControl
      case 52: return lovrSetError("Shader uses unsupported feature #%d: %s", features[i], "sample rate shading");
      case 53: return lovrSetError("Shader uses unsupported feature #%d: %s", features[i], "transform feedback");
      case 54: return lovrSetError("Shader uses unsupported feature #%d: %s", features[i], "geometry shading");
      case 55: return lovrSetError("Shader uses unsupported feature #%d: %s", features[i], "autoformat storage textures");
      case 56: return lovrSetError("Shader uses unsupported feature #%d: %s", features[i], "autoformat storage textures");
      case 57: return lovrSetError("Shader uses unsupported feature #%d: %s", features[i], "multiviewport");
      case 69: return lovrSetError("Shader uses unsupported feature #%d: %s", features[i], "layered rendering");
      case 70: return lovrSetError("Shader uses unsupported feature #%d: %s", features[i], "multiviewport");
      case 4427: break; // ShaderDrawParameters
      case 4437: return lovrSetError("Shader uses unsupported feature #%d: %s", features[i], "multigpu");
      case 4439: lovrCheck(state.limits.renderSize[2] > 1, "GPU does not support shader feature #%d: %s", features[i], "multiview"); break;
      case 5301: return lovrSetError("Shader uses unsupported feature #%d: %s", features[i], "non-uniform indexing");
      case 5306: return lovrSetError("Shader uses unsupported feature #%d: %s", features[i], "non-uniform indexing");
      case 5307: return lovrSetError("Shader uses unsupported feature #%d: %s", features[i], "non-uniform indexing");
      case 5308: return lovrSetError("Shader uses unsupported feature #%d: %s", features[i], "non-uniform indexing");
      case 5309: return lovrSetError("Shader uses unsupported feature #%d: %s", features[i], "non-uniform indexing");
      default: return lovrSetError("Shader uses unknown feature #%d", features[i]);
    }
  }
  return true;
}

static void onResize(uint32_t width, uint32_t height) {
  float density = os_window_get_pixel_density();

  width *= density;
  height *= density;

  state.window->info.width = width;
  state.window->info.height = height;
  state.resized = true;

  lovrEventPush((Event) {
    .type = EVENT_RESIZE,
    .data.resize.width = width,
    .data.resize.height = height
  });
}

static void onMessage(void* context, const char* message) {
  lovrLog(LOG_DEBUG, "GPU", message);
}
