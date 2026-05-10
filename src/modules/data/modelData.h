#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#pragma once

struct Blob;
struct Image;

typedef struct {
  struct { float x, y, z; } position;
#ifdef LOVR_WEBGPU
  struct { float x, y, z; } normal;
#else
  uint32_t normal;
#endif
  struct { float u, v; } uv;
  struct { uint16_t u, v; } uv2;
  struct { uint8_t r, g, b, a; } color;
#ifdef LOVR_WEBGPU
  struct { float x, y, z, w; } tangent;
#else
  uint32_t tangent;
#endif
} ModelVertex;

typedef struct {
  uint8_t joints[4];
  uint8_t weights[4];
} SkinData;

typedef struct {
  float x, y, z;
  float nx, ny, nz;
  float tx, ty, tz;
} BlendData;

typedef enum {
  DRAW_POINT_LIST,
  DRAW_LINE_LIST,
  DRAW_LINE_LOOP,
  DRAW_LINE_STRIP,
  DRAW_TRIANGLE_LIST,
  DRAW_TRIANGLE_STRIP,
  DRAW_TRIANGLE_FAN
} ModelDrawMode;

typedef struct {
  uint32_t start;
  uint32_t count;
  uint32_t baseVertex;
  uint32_t material;
  ModelDrawMode mode;
  float bounds[6];
} ModelPart;

typedef struct {
  const char* name;
  float weight;
} ModelBlendShape;

typedef struct {
  ModelPart* parts;
  uint32_t partCount;
  uint32_t vertexOffset;
  uint32_t vertexCount;
  uint32_t indexOffset;
  uint32_t indexCount;
  uint32_t skinDataOffset;
  uint32_t blendDataOffset;
  uint32_t blendShapeCount;
  ModelBlendShape* blendShapes;
} ModelMesh;

typedef struct {
  float color[4];
  float glow[4];
  float uvShift[2];
  float uvScale[2];
  float sdfRange[2];
  float metalness;
  float roughness;
  float clearcoat;
  float clearcoatRoughness;
  float occlusionStrength;
  float normalScale;
  float alphaCutoff;
  uint32_t texture;
  uint32_t glowTexture;
  uint32_t metalnessTexture;
  uint32_t roughnessTexture;
  uint32_t clearcoatTexture;
  uint32_t occlusionTexture;
  uint32_t normalTexture;
  const char* name;
} ModelMaterial;

typedef enum {
  PROP_TRANSLATION,
  PROP_ROTATION,
  PROP_SCALE,
  PROP_WEIGHTS
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
  float* inverseBindMatrices;
  uint32_t jointCount;
} ModelSkin;

typedef struct {
  const char* name;
  union {
    float matrix[16];
    struct {
      float translation[3];
      float rotation[4];
      float scale[3];
    };
  } transform;
  uint32_t child;
  uint32_t sibling;
  uint32_t parent;
  uint32_t mesh;
  uint32_t skin;
  bool hasMatrix;
} ModelNode;

typedef struct ModelMetadata {
  struct Blob* blob;

  uint64_t id;
  char* comment;
  size_t commentLength;

  uint32_t meshCount;
  uint32_t materialCount;
  uint32_t animationCount;
  uint32_t skinCount;
  uint32_t nodeCount;
  uint32_t rootNode;

  ModelMesh* meshes;
  ModelMaterial* materials;
  ModelAnimation* animations;
  ModelSkin* skins;
  ModelNode* nodes;

  uint32_t partCount;
  uint32_t blendShapeCount;
  uint32_t channelCount;
  uint32_t keyframeDataCount;
  uint32_t jointCount;
  uint32_t charCount;

  ModelPart* parts;
  ModelBlendShape* blendShapes;
  ModelAnimationChannel* channels;
  float* keyframeData;
  float* inverseBindMatrices;
  uint32_t* joints;
  char* chars;

  uint32_t vertexCount;
  uint32_t indexCount;
  uint32_t skinnedVertexCount;
  uint32_t blendedVertexCount;
  uint32_t animatedVertexCount;
  uint32_t indexSize;
  uint32_t imageCount;

  uint32_t* blendShapeLookup;
  uint32_t* animationLookup;
  uint32_t* materialLookup;
  uint32_t* nodeLookup;

  float bounds[6];
} ModelMetadata;

typedef struct ModelData {
  atomic_uint ref;
  ModelMetadata meta;
  ModelVertex* vertices;
  void* indices;
  SkinData* skinData;
  BlendData* blendData;
  struct Image** images;
} ModelData;

typedef void* ModelDataIO(const char* filename, size_t* bytesRead);

ModelData* lovrModelDataCreate(struct Blob* blob, ModelDataIO* io);
bool lovrModelDataInitGltf(ModelData** model, struct Blob* blob, ModelDataIO* io);
bool lovrModelDataInitObj(ModelData** model, struct Blob* blob, ModelDataIO* io);
bool lovrModelDataInitStl(ModelData** model, struct Blob* blob, ModelDataIO* io);
void lovrModelDataDestroy(void* ref);
void lovrModelDataAllocate(ModelData* model);
bool lovrModelDataFinalize(ModelData* model);
void lovrModelDataGetTriangles(ModelData* model, float** vertices, uint32_t** indices, uint32_t* vertexCount, uint32_t* indexCount);

void lovrModelMetadataGetBoundingBox(ModelMetadata* meta, float box[6]);
void lovrModelMetadataGetMeshBoundingBox(ModelMetadata* meta, uint32_t index, float box[6]);
uint32_t lovrModelMetadataNextNodeWithMesh(ModelMetadata* meta, uint32_t node);
