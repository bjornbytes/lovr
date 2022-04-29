#include <stdbool.h>
#include <stdint.h>

#pragma once

typedef struct Buffer Buffer;
typedef struct Pass Pass;

typedef struct {
  uint32_t deviceId;
  uint32_t vendorId;
  const char* name;
  const char* renderer;
  uint32_t subgroupSize;
  bool discrete;
} GraphicsDevice;

typedef struct {
  bool textureBC;
  bool textureASTC;
  bool wireframe;
  bool depthClamp;
  bool indirectDrawFirstInstance;
  bool float64;
  bool int64;
  bool int16;
} GraphicsFeatures;

typedef struct {
  uint32_t textureSize2D;
  uint32_t textureSize3D;
  uint32_t textureSizeCube;
  uint32_t textureLayers;
  uint32_t renderSize[3];
  uint32_t uniformBufferRange;
  uint32_t storageBufferRange;
  uint32_t uniformBufferAlign;
  uint32_t storageBufferAlign;
  uint32_t vertexAttributes;
  uint32_t vertexBufferStride;
  uint32_t vertexShaderOutputs;
  uint32_t clipDistances;
  uint32_t cullDistances;
  uint32_t clipAndCullDistances;
  uint32_t computeDispatchCount[3];
  uint32_t computeWorkgroupSize[3];
  uint32_t computeWorkgroupVolume;
  uint32_t computeSharedMemory;
  uint32_t shaderConstantSize;
  uint32_t indirectDrawCount;
  uint32_t instances;
  float anisotropy;
  float pointSize;
} GraphicsLimits;

bool lovrGraphicsInit(bool debug);
void lovrGraphicsDestroy(void);

void lovrGraphicsGetDevice(GraphicsDevice* device);
void lovrGraphicsGetFeatures(GraphicsFeatures* features);
void lovrGraphicsGetLimits(GraphicsLimits* limits);

void lovrGraphicsSubmit(Pass** passes, uint32_t count);

// Buffer

typedef enum {
  FIELD_I8x4,
  FIELD_U8x4,
  FIELD_SN8x4,
  FIELD_UN8x4,
  FIELD_UN10x3,
  FIELD_I16,
  FIELD_I16x2,
  FIELD_I16x4,
  FIELD_U16,
  FIELD_U16x2,
  FIELD_U16x4,
  FIELD_SN16x2,
  FIELD_SN16x4,
  FIELD_UN16x2,
  FIELD_UN16x4,
  FIELD_I32,
  FIELD_I32x2,
  FIELD_I32x3,
  FIELD_I32x4,
  FIELD_U32,
  FIELD_U32x2,
  FIELD_U32x3,
  FIELD_U32x4,
  FIELD_F16x2,
  FIELD_F16x4,
  FIELD_F32,
  FIELD_F32x2,
  FIELD_F32x3,
  FIELD_F32x4,
  FIELD_MAT2,
  FIELD_MAT3,
  FIELD_MAT4
} FieldType;

typedef struct {
  uint16_t offset;
  uint8_t location;
  uint8_t type;
} BufferField;

typedef enum {
  LAYOUT_PACKED,
  LAYOUT_STD140,
  LAYOUT_STD430
} BufferLayout;

typedef struct {
  uint32_t length;
  uint32_t stride;
  uint32_t fieldCount;
  BufferField fields[16];
  void** pointer;
  const char* label;
  uintptr_t handle;
} BufferInfo;

Buffer* lovrGraphicsGetBuffer(BufferInfo* info, void** data);
Buffer* lovrBufferCreate(BufferInfo* info, void** data);
void lovrBufferDestroy(void* ref);
const BufferInfo* lovrBufferGetInfo(Buffer* buffer);
bool lovrBufferIsTemporary(Buffer* buffer);
void* lovrBufferMap(Buffer* buffer, uint32_t offset, uint32_t size);
void lovrBufferClear(Buffer* buffer, uint32_t offset, uint32_t size);

// Pass

typedef enum {
  PASS_RENDER,
  PASS_COMPUTE,
  PASS_TRANSFER
} PassType;

typedef struct {
  PassType type;
  const char* label;
} PassInfo;

Pass* lovrGraphicsGetPass(PassInfo* info);
void lovrPassDestroy(void* ref);
const PassInfo* lovrPassGetInfo(Pass* pass);
