#include "core/map.h"
#include "core/util.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#pragma once

#define MAX_BONES 48

struct Blob;
struct Image;

typedef enum {
  ATTR_POSITION,
  ATTR_NORMAL,
  ATTR_TEXCOORD,
  ATTR_COLOR,
  ATTR_TANGENT,
  ATTR_BONES,
  ATTR_WEIGHTS,
  MAX_DEFAULT_ATTRIBUTES
} DefaultAttribute2;

typedef enum {
  TOPOLOGY_POINTS,
  TOPOLOGY_LINES,
  TOPOLOGY_LINE_LOOP,
  TOPOLOGY_LINE_STRIP,
  TOPOLOGY_TRIANGLES,
  TOPOLOGY_TRIANGLE_STRIP,
  TOPOLOGY_TRIANGLE_FAN
} Topology;

typedef enum {
  MODEL_NEAREST,
  MODEL_LINEAR
} ModelFilter;

typedef enum {
  MODEL_CLAMP,
  MODEL_REPEAT,
  MODEL_MIRROR
} ModelWrap;

typedef enum {
  SCALAR_METALNESS,
  SCALAR_ROUGHNESS,
  MAX_MATERIAL_SCALARS
} MaterialScalar;

typedef enum {
  COLOR_DIFFUSE,
  COLOR_EMISSIVE,
  MAX_MATERIAL_COLORS
} MaterialColor;

typedef enum {
  TEXTURE_DIFFUSE,
  TEXTURE_EMISSIVE,
  TEXTURE_METALNESS,
  TEXTURE_ROUGHNESS,
  TEXTURE_OCCLUSION,
  TEXTURE_NORMAL,
  MAX_MATERIAL_TEXTURES
} MaterialTexture;

typedef enum {
  SMOOTH_STEP,
  SMOOTH_LINEAR,
  SMOOTH_CUBIC
} SmoothMode;

typedef enum {
  PROP_TRANSLATION,
  PROP_ROTATION,
  PROP_SCALE,
} AnimationProperty;

typedef enum { I8, U8, I16, U16, I32, U32, F32 } AttributeType;

typedef union {
  void* raw;
  int8_t* i8;
  uint8_t* u8;
  int16_t* i16;
  uint16_t* u16;
  int32_t* i32;
  uint32_t* u32;
  float* f32;
} AttributeData;

typedef struct {
  uint32_t blob;
  size_t offset;
  size_t size;
  size_t stride;
  char* data;
} ModelBuffer;

typedef struct {
  uint32_t offset;
  uint32_t buffer;
  uint32_t count;
  AttributeType type;
  unsigned components : 3;
  unsigned normalized : 1;
  unsigned matrix : 1;
  unsigned hasMin : 1;
  unsigned hasMax : 1;
  float min[4];
  float max[4];
} ModelAttribute;

typedef struct {
  uint32_t nodeIndex;
  AnimationProperty property;
  SmoothMode smoothing;
  uint32_t keyframeCount;
  float* times;
  float* data;
} ModelAnimationChannel;

typedef struct {
  const char* name;
  ModelAnimationChannel* channels;
  uint32_t channelCount;
  float duration;
} ModelAnimation;

typedef struct {
  ModelFilter min, mag, mip;
  ModelWrap u, v, w;
} ModelSampler;

typedef struct {
  const char* name;
  float scalars[MAX_MATERIAL_SCALARS];
  float colors[MAX_MATERIAL_COLORS][4];
  uint32_t images[MAX_MATERIAL_TEXTURES];
  uint32_t samplers[MAX_MATERIAL_TEXTURES];
} ModelMaterial;

typedef struct {
  ModelAttribute* attributes[MAX_DEFAULT_ATTRIBUTES];
  ModelAttribute* indices;
  Topology topology;
  uint32_t material;
} ModelPrimitive;

typedef struct {
  const char* name;
  union {
    float matrix[16];
    struct {
      float translation[4];
      float rotation[4];
      float scale[4];
    } properties;
  } transform;
  uint32_t* children;
  uint32_t childCount;
  uint32_t primitiveIndex;
  uint32_t primitiveCount;
  uint32_t skin;
  bool matrix;
} ModelNode;

typedef struct {
  uint32_t* joints;
  uint32_t jointCount;
  float* inverseBindMatrices;
} ModelSkin;

typedef struct ModelData {
  uint32_t ref;
  void* data;
  struct Blob** blobs;
  ModelBuffer* buffers;
  struct Image** images;
  ModelSampler* samplers;
  ModelMaterial* materials;
  ModelAttribute* attributes;
  ModelPrimitive* primitives;
  ModelAnimation* animations;
  ModelSkin* skins;
  ModelNode* nodes;
  uint32_t rootNode;

  uint32_t blobCount;
  uint32_t bufferCount;
  uint32_t imageCount;
  uint32_t samplerCount;
  uint32_t materialCount;
  uint32_t attributeCount;
  uint32_t primitiveCount;
  uint32_t animationCount;
  uint32_t skinCount;
  uint32_t nodeCount;

  ModelAnimationChannel* channels;
  uint32_t* children;
  uint32_t* joints;
  char* chars;
  uint32_t channelCount;
  uint32_t childCount;
  uint32_t jointCount;
  uint32_t charCount;

  map_t animationMap;
  map_t materialMap;
  map_t nodeMap;
} ModelData;

typedef void* ModelDataIO(const char* filename, size_t* bytesRead);

ModelData* lovrModelDataCreate(struct Blob* blob, ModelDataIO* io);
ModelData* lovrModelDataInitGltf(ModelData* model, struct Blob* blob, ModelDataIO* io);
ModelData* lovrModelDataInitObj(ModelData* model, struct Blob* blob, ModelDataIO* io);
ModelData* lovrModelDataInitStl(ModelData* model, struct Blob* blob, ModelDataIO* io);
void lovrModelDataDestroy(void* ref);
void lovrModelDataAllocate(ModelData* model);
