#include "data/blob.h"
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
  char* data;
  size_t length;
} gltfString;

typedef struct {
  int view;
  int count;
  int offset;
  AttributeType type;
  int components : 3;
  int normalized : 1;
} ModelAccessor;

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
} ModelNode;

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
  Blob* glbBlob;
  ModelAccessor* accessors;
  ModelBlob* blobs;
  ModelView* views;
  ModelMesh* meshes;
  ModelNode* nodes;
  ModelPrimitive* primitives;
  uint32_t* childMap;
  int accessorCount;
  int blobCount;
  int viewCount;
  int meshCount;
  int nodeCount;
  int primitiveCount;
} ModelData;

typedef struct {
  void* (*read)(const char* path, size_t* bytesRead);
} ModelDataIO;

ModelData* lovrModelDataInit(ModelData* model, Blob* blob, ModelDataIO io);
#define lovrModelDataCreate(...) lovrModelDataInit(lovrAlloc(ModelData), __VA_ARGS__)
void lovrModelDataDestroy(void* ref);
