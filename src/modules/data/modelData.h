#include "util.h"
#include "core/map.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#pragma once

#define MAX_BONES 48

struct TextureData;
struct Blob;

typedef enum {
  ATTR_POSITION,
  ATTR_NORMAL,
  ATTR_TEXCOORD,
  ATTR_COLOR,
  ATTR_TANGENT,
  ATTR_BONES,
  ATTR_WEIGHTS,
  MAX_DEFAULT_ATTRIBUTES
} DefaultAttribute;

typedef enum {
  DRAW_POINTS,
  DRAW_LINES,
  DRAW_LINE_LOOP,
  DRAW_LINE_STRIP,
  DRAW_TRIANGLES,
  DRAW_TRIANGLE_STRIP,
  DRAW_TRIANGLE_FAN
} DrawMode;

typedef enum {
  FILTER_NEAREST,
  FILTER_BILINEAR,
  FILTER_TRILINEAR,
  FILTER_ANISOTROPIC
} FilterMode;

typedef struct {
  FilterMode mode;
  float anisotropy;
} TextureFilter;

typedef enum {
  WRAP_CLAMP,
  WRAP_REPEAT,
  WRAP_MIRRORED_REPEAT
} WrapMode;

typedef struct {
  WrapMode s;
  WrapMode t;
  WrapMode r;
} TextureWrap;

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
  char* data;
  size_t size;
  size_t stride;
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
  const char* name;
  float scalars[MAX_MATERIAL_SCALARS];
  Color colors[MAX_MATERIAL_COLORS];
  uint32_t textures[MAX_MATERIAL_TEXTURES];
  TextureFilter filters[MAX_MATERIAL_TEXTURES];
  TextureWrap wraps[MAX_MATERIAL_TEXTURES];
} ModelMaterial;

typedef struct {
  ModelAttribute* attributes[MAX_DEFAULT_ATTRIBUTES];
  ModelAttribute* indices;
  DrawMode mode;
  uint32_t material;
} ModelPrimitive;

typedef struct {
  const char* name;
  float transform[16];
  float translation[4];
  float rotation[4];
  float scale[4];
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
  void* data;
  struct Blob** blobs;
  ModelBuffer* buffers;
  struct TextureData** textures;
  ModelMaterial* materials;
  ModelAttribute* attributes;
  ModelPrimitive* primitives;
  ModelAnimation* animations;
  ModelSkin* skins;
  ModelNode* nodes;
  uint32_t rootNode;

  uint32_t blobCount;
  uint32_t bufferCount;
  uint32_t textureCount;
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

ModelData* lovrModelDataInit(ModelData* model, struct Blob* blob);
#define lovrModelDataCreate(...) lovrModelDataInit(lovrAlloc(ModelData), __VA_ARGS__)
ModelData* lovrModelDataInitGltf(ModelData* model, struct Blob* blob);
ModelData* lovrModelDataInitObj(ModelData* model, struct Blob* blob);
void lovrModelDataDestroy(void* ref);
void lovrModelDataAllocate(ModelData* model);
