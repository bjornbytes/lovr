#include "data/blob.h"
#include "data/textureData.h"
#include "util.h"

#pragma once

#define MAX_BONES 48

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
  TEXTURE_ENVIRONMENT_MAP,
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
  size_t offset;
  uint32_t buffer;
  uint32_t count;
  AttributeType type;
  unsigned int components : 3;
  bool normalized : 1;
  bool matrix : 1;
  bool hasMin : 1;
  bool hasMax : 1;
  float min[4];
  float max[4];
} ModelAttribute;

typedef struct {
  int nodeIndex;
  AnimationProperty property;
  SmoothMode smoothing;
  int keyframeCount;
  float* times;
  float* data;
} ModelAnimationChannel;

typedef struct {
  const char* name;
  ModelAnimationChannel* channels;
  int channelCount;
  float duration;
} ModelAnimation;

typedef struct {
  int imageIndex;
  TextureFilter filter;
  TextureWrap wrap;
} ModelTexture;

typedef struct {
  float scalars[MAX_MATERIAL_SCALARS];
  Color colors[MAX_MATERIAL_COLORS];
  int textures[MAX_MATERIAL_TEXTURES];
  TextureFilter filters[MAX_MATERIAL_TEXTURES];
  TextureWrap wraps[MAX_MATERIAL_TEXTURES];
} ModelMaterial;

typedef struct {
  ModelAttribute* attributes[MAX_DEFAULT_ATTRIBUTES];
  ModelAttribute* indices;
  DrawMode mode;
  int material;
} ModelPrimitive;

typedef struct {
  float transform[16];
  uint32_t* children;
  uint32_t childCount;
  uint32_t primitiveIndex;
  uint32_t primitiveCount;
  int skin;
} ModelNode;

typedef struct {
  uint32_t* joints;
  uint32_t jointCount;
  float* inverseBindMatrices;
} ModelSkin;

typedef struct {
  Ref ref;
  void* data;
  Blob** blobs;
  ModelBuffer* buffers;
  TextureData** textures;
  ModelMaterial* materials;
  ModelAttribute* attributes;
  ModelPrimitive* primitives;
  ModelAnimation* animations;
  ModelSkin* skins;
  ModelNode* nodes;
  int rootNode;

  int blobCount;
  int bufferCount;
  int textureCount;
  int materialCount;
  int attributeCount;
  int primitiveCount;
  int animationCount;
  int skinCount;
  int nodeCount;

  ModelAnimationChannel* channels;
  uint32_t* children;
  uint32_t* joints;
  char* chars;
  int channelCount;
  int childCount;
  int jointCount;
  int charCount;
} ModelData;

ModelData* lovrModelDataInit(ModelData* model, Blob* blob);
#define lovrModelDataCreate(...) lovrModelDataInit(lovrAlloc(ModelData), __VA_ARGS__)
ModelData* lovrModelDataInitGltf(ModelData* model, Blob* blob);
ModelData* lovrModelDataInitObj(ModelData* model, Blob* blob);
void lovrModelDataDestroy(void* ref);
void lovrModelDataAllocate(ModelData* model);
