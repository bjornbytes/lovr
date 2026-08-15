#include "data/modelData.h"
#include "data/blob.h"
#include "data/image.h"
#include "core/maf.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>
#include <float.h>

static void* nullIO(const char* path, size_t* count) {
  return NULL;
}

ModelData* lovrModelDataCreate(Blob* source, ModelDataIO* io) {
  if (!io) io = &nullIO;

  ModelData* model = NULL;
  if (!model && !lovrModelDataInitGltf(&model, source, io)) return false;
  if (!model && !lovrModelDataInitObj(&model, source, io)) return false;
  if (!model && !lovrModelDataInitStl(&model, source, io)) return false;

  if (!model) {
    lovrSetError("Unable to load model from '%s'", source->name);
    return NULL;
  }

  lovrModelDataFinalize(model);

  return model;
}

void lovrModelDataDestroy(void* ref) {
  ModelData* model = ref;
  for (uint32_t i = 0; model->images && i < model->meta.imageCount; i++) {
    lovrRelease(model->images[i], lovrImageDestroy);
  }
  lovrRelease(model->meta.blob, lovrBlobDestroy);
  lovrFree(model->vertices);
  lovrFree(model->indices);
  lovrFree(model->skinData);
  lovrFree(model->blendData);
  lovrFree(model->images);
  lovrFree(model);
}

// Batches allocations for all the ModelData arrays
void lovrModelDataAllocate(ModelData* model) {
  ModelMetadata* meta = &model->meta;

  size_t totalSize = 0;
  size_t sizes[17];
  size_t alignment = 8;
  totalSize += sizes[0] = ALIGN(meta->meshCount * sizeof(ModelMesh), alignment);
  totalSize += sizes[1] = ALIGN(meta->materialCount * sizeof(ModelMaterial), alignment);
  totalSize += sizes[2] = ALIGN(meta->animationCount * sizeof(ModelAnimation), alignment);
  totalSize += sizes[3] = ALIGN(meta->skinCount * sizeof(ModelSkin), alignment);
  totalSize += sizes[4] = ALIGN(meta->nodeCount * sizeof(ModelNode), alignment);
  totalSize += sizes[5] = ALIGN(meta->partCount * sizeof(ModelPart), alignment);
  totalSize += sizes[6] = ALIGN(meta->blendShapeCount * sizeof(ModelBlendShape), alignment);
  totalSize += sizes[7] = ALIGN(meta->channelCount * sizeof(ModelAnimationChannel), alignment);
  totalSize += sizes[8] = ALIGN(meta->keyframeDataCount * sizeof(float), alignment);
  totalSize += sizes[9] = ALIGN(meta->jointCount * 16 * sizeof(float), alignment);
  totalSize += sizes[10] = ALIGN(meta->jointCount * sizeof(uint32_t), alignment);
  totalSize += sizes[11] = ALIGN(meta->charCount * sizeof(char), alignment);
  totalSize += sizes[12] = ALIGN(meta->blendShapeCount * sizeof(uint32_t), alignment);
  totalSize += sizes[13] = ALIGN(meta->animationCount * sizeof(uint32_t), alignment);
  totalSize += sizes[14] = ALIGN(meta->materialCount * sizeof(uint32_t), alignment);
  totalSize += sizes[15] = ALIGN(meta->nodeCount * sizeof(uint32_t), alignment);
  totalSize += sizes[16] = ALIGN(meta->commentLength, alignment);

  size_t offset = 0;
  char* p = lovrCalloc(totalSize);
  meta->blob = lovrBlobCreate(p, totalSize, "Model Metadata");
  meta->meshes = (ModelMesh*) (p + offset), offset += sizes[0];
  meta->materials = (ModelMaterial*) (p + offset), offset += sizes[1];
  meta->animations = (ModelAnimation*) (p + offset), offset += sizes[2];
  meta->skins = (ModelSkin*) (p + offset), offset += sizes[3];
  meta->nodes = (ModelNode*) (p + offset), offset += sizes[4];
  meta->parts = (ModelPart*) (p + offset), offset += sizes[5];
  meta->blendShapes = (ModelBlendShape*) (p + offset), offset += sizes[6];
  meta->channels = (ModelAnimationChannel*) (p + offset), offset += sizes[7];
  meta->keyframeData = (float*) (p + offset), offset += sizes[8];
  meta->inverseBindMatrices = (float*) (p + offset), offset += sizes[9];
  meta->joints = (uint32_t*) (p + offset), offset += sizes[10];
  meta->chars = (char*) (p + offset), offset += sizes[11];
  meta->blendShapeLookup = (uint32_t*) (p + offset), offset += sizes[12];
  meta->animationLookup = (uint32_t*) (p + offset), offset += sizes[13];
  meta->materialLookup = (uint32_t*) (p + offset), offset += sizes[14];
  meta->nodeLookup = (uint32_t*) (p + offset), offset += sizes[15];
  meta->comment = (char*) (p + offset), offset += sizes[16];

  model->vertices = meta->vertexCount > 0 ? lovrMalloc(meta->vertexCount * sizeof(ModelVertex)) : NULL;
  model->indices = meta->indexCount > 0 ? lovrMalloc(meta->indexCount * meta->indexSize) : NULL;
  model->skinData = meta->skinnedVertexCount > 0 ? lovrMalloc(meta->skinnedVertexCount * sizeof(SkinData)) : NULL;
  model->blendData = meta->blendedVertexCount > 0 ? lovrMalloc(meta->blendedVertexCount * sizeof(BlendData)) : NULL;
  model->images = meta->imageCount > 0 ? lovrCalloc(meta->imageCount * sizeof(Image*)) : NULL;

  for (uint32_t i = 0; i < meta->meshCount; i++) {
    meta->meshes[i].vertexOffset = ~0u;
    meta->meshes[i].indexOffset = ~0u;
    meta->meshes[i].skinDataOffset = ~0u;
    meta->meshes[i].blendDataOffset = ~0u;
  }

  for (uint32_t i = 0; i < meta->partCount; i++) {
    meta->parts[i].mode = DRAW_TRIANGLE_LIST;
    meta->parts[i].material = ~0u;
  }

  for (uint32_t i = 0; i < meta->materialCount; i++) {
    meta->materials[i] = (ModelMaterial) {
      .metalness = 1.f,
      .roughness = 1.f,
      .clearcoat = 0.f,
      .clearcoatRoughness = 0.f,
      .occlusionStrength = 1.f,
      .normalScale = 1.f,
      .alphaCutoff = 0.f,
      .color = { 1.f, 1.f, 1.f, 1.f },
      .glow = { 0.f, 0.f, 0.f, 1.f },
      .quad = { 0.f, 0.f, 1.f, 1.f },
      .texture = ~0u,
      .glowTexture = ~0u,
      .metalnessTexture = ~0u,
      .roughnessTexture = ~0u,
      .clearcoatTexture = ~0u,
      .occlusionTexture = ~0u,
      .normalTexture = ~0u
    };
  }

  for (uint32_t i = 0; i < meta->nodeCount; i++) {
    vec3_set(meta->nodes[i].transform.translation, 0.f, 0.f, 0.f);
    quat_identity(meta->nodes[i].transform.rotation);
    vec3_set(meta->nodes[i].transform.scale, 1.f, 1.f, 1.f);
    meta->nodes[i].hasMatrix = false;
    meta->nodes[i].child = ~0u;
    meta->nodes[i].sibling = ~0u;
    meta->nodes[i].parent = ~0u;
    meta->nodes[i].mesh = ~0u;
    meta->nodes[i].skin = ~0u;
  }

  meta->bounds[0] = FLT_MAX;
  meta->bounds[1] = -FLT_MAX;
  meta->bounds[2] = FLT_MAX;
  meta->bounds[3] = -FLT_MAX;
  meta->bounds[4] = FLT_MAX;
  meta->bounds[5] = -FLT_MAX;
}

bool lovrModelDataFinalize(ModelData* model) {
  ModelMetadata* meta = &model->meta;

  for (uint32_t i = 0; i < meta->meshCount; i++) {
    uint32_t skin = ~0u;
    ModelNode* node = meta->nodes;
    for (uint32_t j = 0; j < meta->nodeCount; j++, node++) {
      if (node->mesh == i) {
        if (skin == ~0u) {
          skin = node->skin;
        } else {
          lovrAssert(node->skin == skin, "Model has mesh used with multiple different skins, which is not currently supported");
        }
      }
    }
  }

  for (uint32_t i = 0; i < meta->blendShapeCount; i++) {
    const char* name = meta->blendShapes[i].name;
    meta->blendShapeLookup[i] = name ? (uint32_t) hash64(name, strlen(name)) : ~0u;
  }

  for (uint32_t i = 0; i < meta->animationCount; i++) {
    const char* name = meta->animations[i].name;
    meta->animationLookup[i] = name ? (uint32_t) hash64(name, strlen(name)) : ~0u;
  }

  for (uint32_t i = 0; i < meta->materialCount; i++) {
    const char* name = meta->materials[i].name;
    meta->materialLookup[i] = name ? (uint32_t) hash64(name, strlen(name)) : ~0u;
  }

  for (uint32_t i = 0; i < meta->nodeCount; i++) {
    const char* name = meta->nodes[i].name;
    meta->nodeLookup[i] = name ? (uint32_t) hash64(name, strlen(name)) : ~0u;
  }

  return true;
}

static void collectVertices(ModelData* model, uint32_t nodeIndex, float** vertices, uint32_t** indices, uint32_t* baseIndex, float* parentTransform) {
  ModelNode* node = &model->meta.nodes[nodeIndex];

  float m[16];
  mat4_init(m, parentTransform);

  if (node->hasMatrix) {
    mat4_mul(m, node->transform.matrix);
  } else {
    float* T = node->transform.translation;
    float* R = node->transform.rotation;
    float* S = node->transform.scale;
    mat4_translate(m, T[0], T[1], T[2]);
    mat4_rotateQuat(m, R);
    mat4_scale(m, S[0], S[1], S[2]);
  }

  if (node->mesh != ~0u) {
    ModelMesh* mesh = &model->meta.meshes[node->mesh];
    ModelVertex* vertex = model->vertices + mesh->vertexOffset;
    ModelPart* part = mesh->parts;

    for (uint32_t i = 0; i < mesh->vertexCount; i++, vertex++) {
      float v[3] = { vertex->position.x, vertex->position.y, vertex->position.z };
      vec3_init(*vertices, mat4_mulPoint(m, v));
      *vertices += 3;
    }

    if (*indices) {
      for (uint32_t i = 0; i < mesh->partCount; i++, part++) {
        if (part->mode != DRAW_TRIANGLE_LIST) {
          continue;
        }

        if (mesh->indexCount > 0) {
          if (model->meta.indexSize == 4) {
            uint32_t* indexData = (uint32_t*) model->indices + mesh->indexOffset + part->start;
            for (uint32_t j = 0; j < part->count; j++) {
              **indices = indexData[j] + part->baseVertex + *baseIndex;
              *indices += 1;
            }
          } else if (model->meta.indexSize == 2) {
            uint16_t* indexData = (uint16_t*) model->indices + mesh->indexOffset + part->start;
            for (uint32_t j = 0; j < part->count; j++) {
              **indices = (uint32_t) indexData[j] + part->baseVertex + *baseIndex;
              *indices += 1;
            }
          } else {
            lovrUnreachable();
          }
        } else {
          for (uint32_t j = 0; j < part->count; j++) {
            **indices = *baseIndex + j;
            *indices += 1;
          }
        }
      }
    }

    *baseIndex += mesh->vertexCount;
  }

  for (uint32_t i = node->child; i != ~0u; i = model->meta.nodes[i].sibling) {
    collectVertices(model, i, vertices, indices, baseIndex, m);
  }
}

void lovrModelDataGetTriangles(ModelData* model, float** vertices, uint32_t** indices, uint32_t* vertexCount, uint32_t* indexCount) {
  ModelMetadata* meta = &model->meta;

  *vertexCount = 0;
  if (indexCount) *indexCount = 0;

  for (uint32_t i = 0; i < meta->nodeCount; i++) {
    if (meta->nodes[i].mesh != ~0u) {
      ModelMesh* mesh = &meta->meshes[meta->nodes[i].mesh];
      *vertexCount += mesh->vertexCount;

      for (uint32_t j = 0; j < mesh->partCount; j++) {
        if (mesh->parts[j].mode == DRAW_TRIANGLE_LIST) {
          if (indexCount) *indexCount += mesh->parts[j].count;
        }
      }
    }
  }

  float* positions = lovrMalloc(*vertexCount * 3 * sizeof(float));
  uint32_t* indexData = indices ? lovrMalloc(*indexCount * sizeof(uint32_t)) : NULL;

  *vertices = positions;
  if (indices) *indices = indexData;

  uint32_t baseIndex = 0;
  collectVertices(model, model->meta.rootNode, &positions, &indexData, &baseIndex, (float[16]) MAT4_IDENTITY);
}

static void boundingBoxHelper(ModelMetadata* meta, uint32_t nodeIndex, float* parentTransform) {
  ModelNode* node = &meta->nodes[nodeIndex];

  float m[16];
  mat4_init(m, parentTransform);

  if (node->hasMatrix) {
    mat4_mul(m, node->transform.matrix);
  } else {
    float* T = node->transform.translation;
    float* R = node->transform.rotation;
    float* S = node->transform.scale;
    mat4_fromPose(m, T, R);
    mat4_scale(m, S[0], S[1], S[2]);
  }

  if (node->mesh != ~0u) {
    ModelMesh* mesh = &meta->meshes[node->mesh];

    for (uint32_t i = 0; i < mesh->partCount; i++) {
      ModelPart* part = &mesh->parts[i];

      float xmin = part->bounds[0], xmax = part->bounds[1];
      float ymin = part->bounds[2], ymax = part->bounds[3];
      float zmin = part->bounds[4], zmax = part->bounds[5];

      float xa[3] = { xmin * m[0], xmin * m[1], xmin * m[2] };
      float xb[3] = { xmax * m[0], xmax * m[1], xmax * m[2] };

      float ya[3] = { ymin * m[4], ymin * m[5], ymin * m[6] };
      float yb[3] = { ymax * m[4], ymax * m[5], ymax * m[6] };

      float za[3] = { zmin * m[8], zmin * m[9], zmin * m[10] };
      float zb[3] = { zmax * m[8], zmax * m[9], zmax * m[10] };

      float min[3] = {
        MIN(xa[0], xb[0]) + MIN(ya[0], yb[0]) + MIN(za[0], zb[0]) + m[12],
        MIN(xa[1], xb[1]) + MIN(ya[1], yb[1]) + MIN(za[1], zb[1]) + m[13],
        MIN(xa[2], xb[2]) + MIN(ya[2], yb[2]) + MIN(za[2], zb[2]) + m[14]
      };

      float max[3] = {
        MAX(xa[0], xb[0]) + MAX(ya[0], yb[0]) + MAX(za[0], zb[0]) + m[12],
        MAX(xa[1], xb[1]) + MAX(ya[1], yb[1]) + MAX(za[1], zb[1]) + m[13],
        MAX(xa[2], xb[2]) + MAX(ya[2], yb[2]) + MAX(za[2], zb[2]) + m[14]
      };

      meta->bounds[0] = MIN(meta->bounds[0], min[0]);
      meta->bounds[1] = MAX(meta->bounds[1], max[0]);
      meta->bounds[2] = MIN(meta->bounds[2], min[1]);
      meta->bounds[3] = MAX(meta->bounds[3], max[1]);
      meta->bounds[4] = MIN(meta->bounds[4], min[2]);
      meta->bounds[5] = MAX(meta->bounds[5], max[2]);
    }
  }

  for (uint32_t i = node->child; i != ~0u; i = meta->nodes[i].sibling) {
    boundingBoxHelper(meta, i, m);
  }
}

void lovrModelMetadataGetBoundingBox(ModelMetadata* meta, float box[6]) {
  if (meta->bounds[1] < meta->bounds[0]) {
    boundingBoxHelper(meta, meta->rootNode, (float[16]) MAT4_IDENTITY);
  }

  memcpy(box, meta->bounds, sizeof(meta->bounds));
}

void lovrModelMetadataGetMeshBoundingBox(ModelMetadata* meta, uint32_t index, float box[6]) {
  ModelMesh* mesh = &meta->meshes[index];
  memcpy(box, mesh->parts[0].bounds, 6 * sizeof(float));
  for (uint32_t i = 1; i < mesh->partCount; i++) {
    box[0] = MIN(box[0], mesh->parts[i].bounds[0]);
    box[1] = MAX(box[1], mesh->parts[i].bounds[1]);
    box[2] = MIN(box[2], mesh->parts[i].bounds[2]);
    box[3] = MAX(box[3], mesh->parts[i].bounds[3]);
    box[4] = MIN(box[4], mesh->parts[i].bounds[4]);
    box[5] = MAX(box[5], mesh->parts[i].bounds[5]);
  }
}

uint32_t lovrModelMetadataNextNodeWithMesh(ModelMetadata* meta, uint32_t node) {
  while (++node < meta->nodeCount) {
    if (meta->nodes[node].mesh != ~0u) {
      return node;
    }
  }

  return ~0u;
}
