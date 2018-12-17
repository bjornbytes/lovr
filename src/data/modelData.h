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
  uint32_t magic;
  uint32_t version;
  uint32_t length;
} gltfHeader;

typedef struct {
  uint32_t length;
  uint32_t type;
} gltfChunkHeader;

typedef struct {
  int view;
  int count;
  int offset;
  AttributeType type;
  int components : 3;
  int normalized : 1;
} ModelAccessor;

typedef struct {
  int nodeIndex;
  AnimationProperty property;
  int sampler;
} ModelAnimationChannel;

typedef struct {
  int times;
  int values;
  SmoothMode smoothing;
} ModelAnimationSampler;

typedef struct {
  ModelAnimationChannel* channels;
  ModelAnimationSampler* samplers;
  int channelCount;
  int samplerCount;
} ModelAnimation;

typedef struct {
  void* data;
  size_t size;
} ModelBlob;

typedef struct {
  int blob;
  int offset;
  int length;
  int stride;
} ModelView;

typedef struct {
  float scalars[MAX_MATERIAL_SCALARS];
  Color colors[MAX_MATERIAL_COLORS];
  int textures[MAX_MATERIAL_TEXTURES];
} ModelMaterial;

typedef struct {
  DrawMode mode;
  int attributes[MAX_DEFAULT_ATTRIBUTES];
  int indices;
  int material;
} ModelPrimitive;

typedef struct {
  ModelPrimitive* primitives;
  uint32_t primitiveCount;
} ModelMesh;

typedef struct {
  float transform[16];
  uint32_t* children;
  uint32_t childCount;
  int mesh;
  int skin;
} ModelNode;

typedef struct {
  uint32_t* joints;
  uint32_t jointCount;
  int skeleton;
  int inverseBindMatrices;
} ModelSkin;

typedef struct {
  double time;
  float data[4];
} Keyframe;

typedef vec_t(Keyframe) vec_keyframe_t;

typedef struct {
  const char* node;
  vec_keyframe_t positionKeyframes;
  vec_keyframe_t rotationKeyframes;
  vec_keyframe_t scaleKeyframes;
} AnimationChannel;

typedef map_t(AnimationChannel) map_channel_t;

typedef struct {
  const char* name;
  float duration;
  map_channel_t channels;
  int channelCount;
} Animation;

typedef struct {
  Ref ref;
  uint8_t* data;
  Blob* binaryBlob;
  ModelAccessor* accessors;
  ModelAnimationChannel* animationChannels;
  ModelAnimationSampler* animationSamplers;
  ModelAnimation* animations;
  ModelBlob* blobs;
  ModelView* views;
  TextureData** images;
  ModelMaterial* materials;
  ModelPrimitive* primitives;
  ModelMesh* meshes;
  ModelNode* nodes;
  ModelSkin* skins;
  int accessorCount;
  int animationChannelCount;
  int animationSamplerCount;
  int animationCount;
  int blobCount;
  int viewCount;
  int imageCount;
  int materialCount;
  int primitiveCount;
  int meshCount;
  int nodeCount;
  int skinCount;
  uint32_t* nodeChildren;
  uint32_t* skinJoints;
} ModelData;

typedef struct {
  void* (*read)(const char* path, size_t* bytesRead);
} ModelDataIO;

ModelData* lovrModelDataInit(ModelData* model, Blob* blob, ModelDataIO io);
#define lovrModelDataCreate(...) lovrModelDataInit(lovrAlloc(ModelData), __VA_ARGS__)
void lovrModelDataDestroy(void* ref);
