#include "core/map.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#pragma once

struct Blob;
struct Image;

typedef struct {
  uint32_t blob;
  size_t offset;
  size_t size;
  size_t stride;
  char* data;
} ModelBuffer;

enum {
  ATTR_POSITION,
  ATTR_NORMAL,
  ATTR_TEXCOORD,
  ATTR_COLOR,
  ATTR_TANGENT,
  ATTR_JOINTS,
  ATTR_WEIGHTS,
  MAX_MODEL_ATTRIBUTES
};

enum { I8, U8, I16, U16, I32, U32, F32 };

typedef struct {
  uint32_t offset;
  uint32_t buffer;
  uint32_t count;
  uint32_t type;
  unsigned components : 3;
  unsigned normalized : 1;
  unsigned matrix : 1;
  unsigned hasMin : 1;
  unsigned hasMax : 1;
  float min[4];
  float max[4];
} ModelAttribute;

typedef enum {
  TOPOLOGY_POINTS,
  TOPOLOGY_LINES,
  TOPOLOGY_LINE_LOOP,
  TOPOLOGY_LINE_STRIP,
  TOPOLOGY_TRIANGLES,
  TOPOLOGY_TRIANGLE_STRIP,
  TOPOLOGY_TRIANGLE_FAN
} Topology;

typedef struct {
  ModelAttribute* attributes[MAX_MODEL_ATTRIBUTES];
  ModelAttribute* indices;
  Topology topology;
  uint32_t material;
} ModelPrimitive;

typedef enum {
  SCALAR_METALNESS,
  SCALAR_ROUGHNESS
} MaterialScalar;

typedef enum {
  COLOR_BASE,
  COLOR_EMISSIVE
} MaterialColor;

typedef enum {
  TEXTURE_COLOR,
  TEXTURE_EMISSIVE,
  TEXTURE_METALNESS,
  TEXTURE_ROUGHNESS,
  TEXTURE_OCCLUSION,
  TEXTURE_NORMAL
} MaterialTexture;

typedef struct {
  const char* name;
  float metalness;
  float roughness;
  float color[4];
  float emissive[3];
  uint32_t colorTexture;
  uint32_t emissiveTexture;
  uint32_t metalnessRoughnessTexture;
  uint32_t occlusionTexture;
  uint32_t normalTexture;
} ModelMaterial;

typedef enum {
  PROP_TRANSLATION,
  PROP_ROTATION,
  PROP_SCALE,
} AnimationProperty;

typedef enum {
  SMOOTH_STEP,
  SMOOTH_LINEAR,
  SMOOTH_CUBIC
} SmoothMode;

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
  uint32_t* joints;
  uint32_t jointCount;
  float* inverseBindMatrices;
} ModelSkin;

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

typedef struct ModelData {
  uint32_t ref;
  void* data;

  struct Blob** blobs;
  struct Image** images;
  ModelBuffer* buffers;
  ModelAttribute* attributes;
  ModelPrimitive* primitives;
  ModelMaterial* materials;
  ModelAnimation* animations;
  ModelSkin* skins;
  ModelNode* nodes;
  uint32_t rootNode;

  uint32_t blobCount;
  uint32_t imageCount;
  uint32_t bufferCount;
  uint32_t attributeCount;
  uint32_t primitiveCount;
  uint32_t materialCount;
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
