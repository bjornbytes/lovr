#include "data/blob.h"
#include "data/textureData.h"
#include "util.h"
#include "lib/map/map.h"
#include "lib/vec/vec.h"

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
  SMOOTH_CUBIC,
} SmoothMode;

typedef enum {
  PROP_TRANSLATION,
  PROP_ROTATION,
  PROP_SCALE,
} AnimationProperty;

typedef enum { I8, U8, I16, U16, I32, U32, F32 } AttributeType;

typedef struct {
  char* data;
  size_t size;
  size_t stride;
} ModelBuffer;

typedef struct {
  uint32_t buffer;
  size_t offset;
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
  ModelAttribute* times;
  ModelAttribute* data;
} ModelAnimationChannel;

typedef struct {
  ModelAnimationChannel* channels;
  int channelCount;
  float duration;
} ModelAnimation;

typedef struct {
  int imageIndex;
  TextureFilter filter;
  TextureWrap wrap;
  bool mipmaps;
} ModelTexture;

typedef struct {
  float scalars[MAX_MATERIAL_SCALARS];
  Color colors[MAX_MATERIAL_COLORS];
  int textures[MAX_MATERIAL_TEXTURES];
} ModelMaterial;

typedef struct {
  DrawMode mode;
  ModelAttribute* attributes[MAX_DEFAULT_ATTRIBUTES];
  ModelAttribute* indices;
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
  int skeleton;
  float* inverseBindMatrices;
} ModelSkin;

typedef struct {
  Ref ref;
  uint8_t* data;
  Blob** blobs;
  TextureData** images;
  ModelAnimation* animations;
  ModelAttribute* attributes;
  ModelBuffer* buffers;
  ModelTexture* textures;
  ModelMaterial* materials;
  ModelPrimitive* primitives;
  ModelNode* nodes;
  ModelSkin* skins;
  int blobCount;
  int imageCount;
  int animationCount;
  int attributeCount;
  int bufferCount;
  int textureCount;
  int materialCount;
  int primitiveCount;
  int nodeCount;
  int skinCount;
  int rootNode;
} ModelData;

typedef struct {
  void* (*read)(const char* path, size_t* bytesRead);
} ModelDataIO;

ModelData* lovrModelDataInit(ModelData* model, Blob* blob, ModelDataIO io);
#define lovrModelDataCreate(...) lovrModelDataInit(lovrAlloc(ModelData), __VA_ARGS__)
void lovrModelDataDestroy(void* ref);
