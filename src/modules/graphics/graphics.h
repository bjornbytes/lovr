#include <stdbool.h>
#include <stdint.h>

#pragma once

struct WindowFlags;

typedef struct Buffer Buffer;
typedef struct Texture Texture;

typedef struct {
  bool bptc;
  bool astc;
  bool pointSize;
  bool wireframe;
  bool anisotropy;
  bool clipDistance;
  bool cullDistance;
  bool fullIndexBufferRange;
  bool indirectDrawCount;
  bool indirectDrawFirstInstance;
  bool extraShaderInputs;
  bool multiview;
} GraphicsFeatures;

typedef struct {
  uint32_t textureSize2D;
  uint32_t textureSize3D;
  uint32_t textureSizeCube;
  uint32_t textureLayers;
  uint32_t renderSize[2];
  uint32_t renderViews;
  uint32_t bundleCount;
  uint32_t bundleSlots;
  uint32_t uniformBufferRange;
  uint32_t storageBufferRange;
  uint32_t uniformBufferAlign;
  uint32_t storageBufferAlign;
  uint32_t vertexAttributes;
  uint32_t vertexAttributeOffset;
  uint32_t vertexBuffers;
  uint32_t vertexBufferStride;
  uint32_t vertexShaderOutputs;
  uint32_t computeCount[3];
  uint32_t computeGroupSize[3];
  uint32_t computeGroupVolume;
  uint32_t computeSharedMemory;
  uint32_t indirectDrawCount;
  uint64_t allocationSize;
  uint32_t pointSize[2];
  float anisotropy;
} GraphicsLimits;

// Targets are either a Texture or a format (for a temporary target)

typedef enum {
  LOAD_KEEP,
  LOAD_CLEAR,
  LOAD_DISCARD
} LoadOp;

typedef enum {
  SAVE_KEEP,
  SAVE_DISCARD
} SaveOp;

typedef struct {
  Texture* texture;
  Texture* resolve;
  LoadOp load;
  SaveOp save;
  float clear[4];
} ColorTarget;

typedef struct {
  bool enabled;
  Texture* texture;
  uint32_t format;
  LoadOp load;
  SaveOp save;
  float clear;
  struct {
    LoadOp load;
    SaveOp save;
    uint8_t clear;
  } stencil;
} DepthTarget;

typedef struct {
  ColorTarget color[4];
  DepthTarget depth;
  uint32_t samples;
} Canvas;

bool lovrGraphicsInit(bool debug);
void lovrGraphicsDestroy(void);
void lovrGraphicsCreateWindow(struct WindowFlags* window);
bool lovrGraphicsHasWindow(void);
uint32_t lovrGraphicsGetWidth(void);
uint32_t lovrGraphicsGetHeight(void);
float lovrGraphicsGetPixelDensity(void);
void lovrGraphicsGetFeatures(GraphicsFeatures* features);
void lovrGraphicsGetLimits(GraphicsLimits* limits);
void lovrGraphicsBegin(void);
void lovrGraphicsFlush(void);
void lovrGraphicsRender(Canvas* canvas);
void lovrGraphicsCompute(void);
void lovrGraphicsEndPass(void);

// Buffer

typedef enum {
  BUFFER_VERTEX,
  BUFFER_INDEX,
  BUFFER_UNIFORM,
  BUFFER_COMPUTE,
  BUFFER_ARGUMENT,
  BUFFER_UPLOAD,
  BUFFER_DOWNLOAD
} BufferUsage;

typedef struct {
  uint32_t size;
  uint32_t usage;
  const char* label;
} BufferInfo;

Buffer* lovrBufferCreate(BufferInfo* info);
void lovrBufferDestroy(void* ref);
const BufferInfo* lovrBufferGetInfo(Buffer* buffer);
void* lovrBufferMap(Buffer* buffer, uint32_t offset, uint32_t size);

// Texture

typedef enum {
  TEXTURE_2D,
  TEXTURE_CUBE,
  TEXTURE_VOLUME,
  TEXTURE_ARRAY
} TextureType;

typedef enum {
  TEXTURE_SAMPLE,
  TEXTURE_RENDER,
  TEXTURE_COMPUTE,
  TEXTURE_UPLOAD,
  TEXTURE_DOWNLOAD
} TextureUsage;

typedef struct {
  Texture* source;
  TextureType type;
  uint32_t layerIndex;
  uint32_t layerCount;
  uint32_t mipmapIndex;
  uint32_t mipmapCount;
} TextureView;

typedef struct {
  TextureView view;
  TextureType type;
  uint32_t format;
  uint32_t size[3];
  uint32_t mipmaps;
  uint32_t samples;
  uint32_t usage;
  bool srgb;
  const char* label;
} TextureInfo;

Texture* lovrTextureCreate(TextureInfo* info);
Texture* lovrTextureCreateView(TextureView* view);
void lovrTextureDestroy(void* ref);
const TextureInfo* lovrTextureGetInfo(Texture* texture);
void lovrTextureGetPixels(Texture* texture, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t layer, uint32_t level, void (*callback)(void* data, uint64_t size, void* context), void* context);
