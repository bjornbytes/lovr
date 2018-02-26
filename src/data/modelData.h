#include "data/blob.h"
#include "data/textureData.h"
#include "data/vertexData.h"
#include "util.h"
#include "lib/map/map.h"
#include "lib/vec/vec.h"

#pragma once

#define MAX_BONES_PER_VERTEX 4
#define MAX_BONES 48

typedef struct {
  const char* name;
  float offset[16];
} Bone;

typedef struct {
  int material;
  int drawStart;
  int drawCount;
  Bone bones[MAX_BONES];
  map_int_t boneMap;
  int boneCount;
} ModelPrimitive;

typedef struct ModelNode {
  const char* name;
  float transform[16];
  float globalTransform[16];
  int parent;
  vec_uint_t children;
  vec_uint_t primitives;
} ModelNode;

typedef struct {
  Color diffuseColor;
  Color emissiveColor;
  int diffuseTexture;
  int emissiveTexture;
  int metalnessTexture;
  int roughnessTexture;
  int occlusionTexture;
  int normalTexture;
  float metalness;
  float roughness;
} ModelMaterial;

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
  VertexData* vertexData;
  IndexPointer indices;
  int indexCount;
  size_t indexSize;
  ModelNode* nodes;
  map_int_t nodeMap;
  ModelPrimitive* primitives;
  Animation* animations;
  ModelMaterial* materials;
  vec_void_t textures;
  int nodeCount;
  int primitiveCount;
  int animationCount;
  int materialCount;
} ModelData;

ModelData* lovrModelDataCreate(Blob* blob);
void lovrModelDataDestroy(void* ref);
void lovrModelDataGetAABB(ModelData* modelData, float aabb[6]);
