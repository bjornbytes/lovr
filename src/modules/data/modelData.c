#include "data/modelData.h"
#include "data/blob.h"
#include "data/image.h"
#include "core/maf.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>

static size_t typeSizes[] = {
  [I8] = 1,
  [U8] = 1,
  [I16] = 2,
  [U16] = 2,
  [I32] = 4,
  [U32] = 4,
  [F32] = 4,
  [SN10x3] = 4
};

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

  if (!lovrModelDataFinalize(model)) {
    lovrModelDataDestroy(model);
    return NULL;
  }

  return model;
}

void lovrModelDataDestroy(void* ref) {
  ModelData* model = ref;
  for (uint32_t i = 0; i < model->blobCount; i++) {
    lovrRelease(model->blobs[i], lovrBlobDestroy);
  }
  for (uint32_t i = 0; i < model->imageCount; i++) {
    lovrRelease(model->images[i], lovrImageDestroy);
  }
  map_free(model->blendShapeMap);
  map_free(model->animationMap);
  map_free(model->materialMap);
  map_free(model->nodeMap);
  lovrFree(model->vertices);
  lovrFree(model->indices);
  lovrFree(model->metadata);
  lovrFree(model->data);
  lovrFree(model);
}

// Batches allocations for all the ModelData arrays
void lovrModelDataAllocate(ModelData* model) {
  size_t totalSize = 0;
  size_t sizes[19];
  size_t alignment = 8;
  totalSize += sizes[0] = ALIGN(model->blobCount * sizeof(Blob*), alignment);
  totalSize += sizes[1] = ALIGN(model->bufferCount * sizeof(ModelBuffer), alignment);
  totalSize += sizes[2] = ALIGN(model->imageCount * sizeof(Image*), alignment);
  totalSize += sizes[3] = ALIGN(model->attributeCount * sizeof(ModelAttribute), alignment);
  totalSize += sizes[4] = ALIGN(model->primitiveCount * sizeof(ModelPrimitive), alignment);
  totalSize += sizes[5] = ALIGN(model->materialCount * sizeof(ModelMaterial), alignment);
  totalSize += sizes[6] = ALIGN(model->blendShapeCount * sizeof(ModelBlendShape), alignment);
  totalSize += sizes[7] = ALIGN(model->animationCount * sizeof(ModelAnimation), alignment);
  totalSize += sizes[8] = ALIGN(model->skinCount * sizeof(ModelSkin), alignment);
  totalSize += sizes[9] = ALIGN(model->nodeCount * sizeof(ModelNode), alignment);
  totalSize += sizes[10] = ALIGN(model->channelCount * sizeof(ModelAnimationChannel), alignment);
  totalSize += sizes[11] = ALIGN(model->blendDataCount * sizeof(ModelBlendData), alignment);
  totalSize += sizes[12] = ALIGN(model->childCount * sizeof(uint32_t), alignment);
  totalSize += sizes[13] = ALIGN(model->jointCount * sizeof(uint32_t), alignment);
  totalSize += sizes[14] = ALIGN(model->charCount * sizeof(char), alignment);
  totalSize += sizes[15] = ALIGN(sizeof(map_t), alignment);
  totalSize += sizes[16] = ALIGN(sizeof(map_t), alignment);
  totalSize += sizes[17] = ALIGN(sizeof(map_t), alignment);
  totalSize += sizes[18] = ALIGN(sizeof(map_t), alignment);

  size_t offset = 0;
  char* p = model->data = lovrCalloc(totalSize);
  model->blobs = (Blob**) (p + offset), offset += sizes[0];
  model->buffers = (ModelBuffer*) (p + offset), offset += sizes[1];
  model->images = (Image**) (p + offset), offset += sizes[2];
  model->attributes = (ModelAttribute*) (p + offset), offset += sizes[3];
  model->primitives = (ModelPrimitive*) (p + offset), offset += sizes[4];
  model->materials = (ModelMaterial*) (p + offset), offset += sizes[5];
  model->blendShapes = (ModelBlendShape*) (p + offset), offset += sizes[6];
  model->animations = (ModelAnimation*) (p + offset), offset += sizes[7];
  model->skins = (ModelSkin*) (p + offset), offset += sizes[8];
  model->nodes = (ModelNode*) (p + offset), offset += sizes[9];
  model->channels = (ModelAnimationChannel*) (p + offset), offset += sizes[10];
  model->blendData = (ModelBlendData*) (p + offset), offset += sizes[11];
  model->children = (uint32_t*) (p + offset), offset += sizes[12];
  model->joints = (uint32_t*) (p + offset), offset += sizes[13];
  model->chars = (char*) (p + offset), offset += sizes[14];
  model->blendShapeMap = (map_t*) (p + offset), offset += sizes[15];
  model->animationMap = (map_t*) (p + offset), offset += sizes[16];
  model->materialMap = (map_t*) (p + offset), offset += sizes[17];
  model->nodeMap = (map_t*) (p + offset), offset += sizes[18];

  map_init(model->blendShapeMap, model->blendShapeCount);
  map_init(model->animationMap, model->animationCount);
  map_init(model->materialMap, model->materialCount);
  map_init(model->nodeMap, model->nodeCount);
}

bool lovrModelDataFinalize(ModelData* model) {
  for (uint32_t i = 0; i < model->primitiveCount; i++) {
    model->primitives[i].skin = ~0u;
  }

  for (uint32_t i = 0; i < model->nodeCount; i++) {
    ModelNode* node = &model->nodes[i];

    if (node->primitiveCount > 0) {
      for (uint32_t j = 0; j < model->nodeCount; j++) {
        if (i == j || model->nodes[j].primitiveCount == 0 || node->primitiveIndex != model->nodes[j].primitiveIndex) continue;
        lovrCheck(node->skin == model->nodes[j].skin, "Model has a mesh used with multiple different skins, which is not supported");
      }
    }

    for (uint32_t j = node->primitiveIndex; j < node->primitiveIndex + node->primitiveCount; j++) {
      model->primitives[j].skin = node->skin;
    }
  }

  model->indexType = U16;
  for (uint32_t i = 0; i < model->primitiveCount; i++) {
    ModelPrimitive* primitive = &model->primitives[i];
    uint32_t vertexCount = primitive->attributes[ATTR_POSITION]->count;

    if (primitive->skin != ~0u) {
      model->skins[primitive->skin].vertexCount += vertexCount;
      model->skins[primitive->skin].blendedVertexCount += primitive->blendShapeCount > 0 ? vertexCount : 0;
      model->skinnedVertexCount += vertexCount;
    }

    model->blendShapeVertexCount += vertexCount * primitive->blendShapeCount;
    model->dynamicVertexCount += primitive->skin != ~0u || !!primitive->blendShapes ? vertexCount : 0;
    model->vertexCount += vertexCount;

    model->indexCount += primitive->indices ? primitive->indices->count : 0;
    if (primitive->indices) {
      if (primitive->indices->type == U32) {
        primitive->indices->stride = 4;
        model->indexType = U32;
      } else {
        primitive->indices->stride = 2;
      }
    }

    for (uint32_t j = 0; j < MAX_DEFAULT_ATTRIBUTES; j++) {
      ModelAttribute* attribute = primitive->attributes[j];
      if (!attribute) continue;
      attribute->stride = model->buffers[attribute->buffer].stride;
      if (attribute->stride == 0) attribute->stride = typeSizes[attribute->type] * attribute->components;
    }

    for (uint32_t j = 0; j < primitive->blendShapeCount; j++) {
      ModelBlendData* blendData = &primitive->blendShapes[j];
      ModelAttribute* attributes[] = { blendData->positions, blendData->normals, blendData->tangents };
      for (uint32_t k = 0; k < COUNTOF(attributes); k++) {
        if (!attributes[k]) continue;
        ModelAttribute* attribute = attributes[k];
        attribute->stride = model->buffers[attribute->buffer].stride;
        if (attribute->stride == 0) attribute->stride = typeSizes[attribute->type] * attribute->components;
      }
    }
  }

  for (uint32_t i = 0; i < model->nodeCount; i++) {
    model->nodes[i].parent = ~0u;
  }

  for (uint32_t i = 0; i < model->nodeCount; i++) {
    ModelNode* node = &model->nodes[i];
    for (uint32_t j = 0; j < node->childCount; j++) {
      model->nodes[node->children[j]].parent = i;
    }
  }

  return true;
}

void lovrModelDataCopyAttribute(ModelData* data, ModelAttribute* attribute, char* dst, AttributeType type, uint32_t components, bool normalized, uint32_t count, size_t stride, uint8_t clear) {
  char* src = attribute ? data->buffers[attribute->buffer].data + attribute->offset : NULL;
  size_t size = components * typeSizes[type];

  if (!attribute) {
    for (uint32_t i = 0; i < count; i++, dst += stride) {
      memset(dst, clear, size);
    }
  } else if (attribute->type == type && attribute->components >= components) {
    for (uint32_t i = 0; i < count; i++, src += attribute->stride, dst += stride) {
      memcpy(dst, src, size);
    }
  } else if (type == F32) {
    if (attribute->type == U8 && attribute->normalized) {
      for (uint32_t i = 0; i < count; i++, src += attribute->stride, dst += stride) {
        for (uint32_t j = 0; j < components; j++) {
          ((float*) dst)[j] = ((uint8_t*) src)[j] / 255.f;
        }
      }
    } else if (attribute->type == U16 && attribute->normalized) {
      for (uint32_t i = 0; i < count; i++, src += attribute->stride, dst += stride) {
        for (uint32_t j = 0; j < components; j++) {
          ((float*) dst)[j] = ((uint16_t*) src)[j] / 65535.f;
        }
      }
    } else {
      lovrUnreachable();
    }
  } else if (type == U8) {
    if (attribute->type == U16 && attribute->normalized && normalized) {
      for (uint32_t i = 0; i < count; i++, src += attribute->stride, dst += stride) {
        for (uint32_t j = 0; j < components; j++) {
          ((uint8_t*) dst)[j] = ((uint16_t*) src)[j] >> 8;
        }
        if (components == 4 && attribute->components == 3) {
          ((uint8_t*) dst)[3] = 255;
        }
      }
    } else if (attribute->type == U16 && !attribute->normalized && !normalized) {
      for (uint32_t i = 0; i < count; i++, src += attribute->stride, dst += stride) {
        for (uint32_t j = 0; j < components; j++) {
          ((uint8_t*) dst)[j] = (uint8_t) ((uint16_t*) src)[j];
        }
      }
    } else if (attribute->type == I16 && !attribute->normalized && !normalized) {
      for (uint32_t i = 0; i < count; i++, src += attribute->stride, dst += stride) {
        for (uint32_t j = 0; j < components; j++) {
          ((uint8_t*) dst)[j] = (uint8_t) ((int16_t*) src)[j];
        }
      }
    } else if (attribute->type == F32 && normalized) {
      for (uint32_t i = 0; i < count; i++, src += attribute->stride, dst += stride) {
        for (uint32_t j = 0; j < components; j++) {
          ((uint8_t*) dst)[j] = ((float*) src)[j] * 255.f + .5f;
        }
        if (components == 4 && attribute->components == 3) {
          ((uint8_t*) dst)[3] = 255;
        }
      }
    } else {
      lovrUnreachable();
    }
  } else if (type == SN10x3) {
    if (attribute->type == F32) {
      for (uint32_t i = 0; i < count; i++, src += attribute->stride, dst += stride) {
        float x = ((float*) src)[0];
        float y = ((float*) src)[1];
        float z = ((float*) src)[2];
        float w = attribute->components == 4 ? ((float*) src)[3] : 0.f;
        *(uint32_t*) dst =
          ((((uint32_t) (int32_t) (x * 511.f)) & 0x3ff) <<  0) |
          ((((uint32_t) (int32_t) (y * 511.f)) & 0x3ff) << 10) |
          ((((uint32_t) (int32_t) (z * 511.f)) & 0x3ff) << 20) |
          ((((uint32_t) (int32_t) (w * 2.f)) & 0x003) << 30);
      }
    } else {
      lovrUnreachable();
    }
  } else {
    lovrUnreachable();
  }
}

static void boundingBoxHelper(ModelData* model, uint32_t nodeIndex, float* parentTransform) {
  ModelNode* node = &model->nodes[nodeIndex];

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

  for (uint32_t i = 0; i < node->primitiveCount; i++) {
    ModelAttribute* position = model->primitives[node->primitiveIndex + i].attributes[ATTR_POSITION];

    if (!position || !position->hasMin || !position->hasMax) {
      continue;
    }

    float xa[3] = { position->min[0] * m[0], position->min[0] * m[1], position->min[0] * m[2] };
    float xb[3] = { position->max[0] * m[0], position->max[0] * m[1], position->max[0] * m[2] };

    float ya[3] = { position->min[1] * m[4], position->min[1] * m[5], position->min[1] * m[6] };
    float yb[3] = { position->max[1] * m[4], position->max[1] * m[5], position->max[1] * m[6] };

    float za[3] = { position->min[2] * m[8], position->min[2] * m[9], position->min[2] * m[10] };
    float zb[3] = { position->max[2] * m[8], position->max[2] * m[9], position->max[2] * m[10] };

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

    model->boundingBox[0] = MIN(model->boundingBox[0], min[0]);
    model->boundingBox[1] = MAX(model->boundingBox[1], max[0]);
    model->boundingBox[2] = MIN(model->boundingBox[2], min[1]);
    model->boundingBox[3] = MAX(model->boundingBox[3], max[1]);
    model->boundingBox[4] = MIN(model->boundingBox[4], min[2]);
    model->boundingBox[5] = MAX(model->boundingBox[5], max[2]);
  }

  for (uint32_t i = 0; i < node->childCount; i++) {
    boundingBoxHelper(model, node->children[i], m);
  }
}

void lovrModelDataGetBoundingBox(ModelData* model, float box[6]) {
  if (model->boundingBox[1] - model->boundingBox[0] == 0.f) {
    boundingBoxHelper(model, model->rootNode, (float[16]) MAT4_IDENTITY);
  }

  memcpy(box, model->boundingBox, sizeof(model->boundingBox));
}

static void boundingSphereHelper(ModelData* model, uint32_t nodeIndex, uint32_t* pointIndex, float* points, float* parentTransform) {
  ModelNode* node = &model->nodes[nodeIndex];

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

  for (uint32_t i = 0; i < node->primitiveCount; i++) {
    ModelAttribute* position = model->primitives[node->primitiveIndex + i].attributes[ATTR_POSITION];

    if (!position || !position->hasMin || !position->hasMax) {
      continue;
    }

    float* min = position->min;
    float* max = position->max;

    float corners[8][3] = {
      { min[0], min[1], min[2] },
      { min[0], min[1], max[2] },
      { min[0], max[1], min[2] },
      { min[0], max[1], max[2] },
      { max[0], min[1], min[2] },
      { max[0], min[1], max[2] },
      { max[0], max[1], min[2] },
      { max[0], max[1], max[2] }
    };

    for (uint32_t j = 0; j < 8; j++) {
      mat4_mulPoint(m, corners[j]);
      vec3_init(points + 3 * (*pointIndex)++, corners[j]);
    }
  }

  for (uint32_t i = 0; i < node->childCount; i++) {
    boundingSphereHelper(model, node->children[i], pointIndex, points, m);
  }
}

void lovrModelDataGetBoundingSphere(ModelData* model, float sphere[4]) {
  if (model->boundingSphere[3] == 0.f) {
    uint32_t totalPrimitiveCount = 0;

    for (uint32_t i = 0; i < model->nodeCount; i++) {
      totalPrimitiveCount += model->nodes[i].primitiveCount;
    }

    uint32_t pointCount = totalPrimitiveCount * 8;
    float* points = lovrMalloc(pointCount * 3 * sizeof(float));

    uint32_t pointIndex = 0;
    boundingSphereHelper(model, model->rootNode, &pointIndex, points, (float[16]) MAT4_IDENTITY);

    // Find point furthest away from first point

    float max = 0.f;
    float* a = NULL;
    for (uint32_t i = 1; i < pointCount; i++) {
      float d2 = vec3_distance2(&points[3 * i], &points[0]);
      if (d2 > max) {
        a = &points[3 * i];
        max = d2;
      }
    }

    // Find point furthest away from that point

    max = 0.f;
    float* b = NULL;
    for (uint32_t i = 0; i < pointCount; i++) {
      float d2 = vec3_distance2(&points[3 * i], a);
      if (d2 > max) {
        b = &points[3 * i];
        max = d2;
      }
    }

    // Create and refine sphere

    float dx = a[0] - b[0];
    float dy = a[1] - b[1];
    float dz = a[2] - b[2];
    float x = (a[0] + b[0]) / 2.f;
    float y = (a[1] + b[1]) / 2.f;
    float z = (a[2] + b[2]) / 2.f;
    float r = sqrtf(dx * dx + dy * dy + dz * dz) / 2.f;
    float r2 = r * r;

    for (uint32_t i = 0; i < pointCount; i++) {
      float dx = points[3 * i + 0] - x;
      float dy = points[3 * i + 1] - y;
      float dz = points[3 * i + 2] - z;
      float d2 = dx * dx + dy * dy + dz * dz;
      if (d2 > r2) {
        r = sqrtf(d2);
        r2 = r * r;
      }
    }

    model->boundingSphere[0] = x;
    model->boundingSphere[1] = y;
    model->boundingSphere[2] = z;
    model->boundingSphere[3] = r;
    lovrFree(points);
  }

  memcpy(sphere, model->boundingSphere, sizeof(model->boundingSphere));
}

static void countVertices(ModelData* model, uint32_t nodeIndex, uint32_t* vertexCount, uint32_t* indexCount) {
  ModelNode* node = &model->nodes[nodeIndex];

  for (uint32_t i = 0; i < node->primitiveCount; i++) {
    ModelPrimitive* primitive = &model->primitives[node->primitiveIndex + i];
    ModelAttribute* positions = primitive->attributes[ATTR_POSITION];
    if (!positions) continue;

    // If 2 meshes in the node use the same vertex buffer, don't count the vertices twice
    for (uint32_t j = 0; j < i; j++) {
      if (model->primitives[node->primitiveIndex + j].attributes[ATTR_POSITION] == positions) {
        positions = NULL;
        break;
      }
    }

    ModelAttribute* indices = primitive->indices;
    uint32_t count = positions ? positions->count : 0;
    *vertexCount += count;
    *indexCount += indices ? indices->count : count;
  }

  for (uint32_t i = 0; i < node->childCount; i++) {
    countVertices(model, node->children[i], vertexCount, indexCount);
  }
}

static void collectVertices(ModelData* model, uint32_t nodeIndex, float** vertices, uint32_t** indices, uint32_t* baseIndex, float* parentTransform) {
  ModelNode* node = &model->nodes[nodeIndex];

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

  uint32_t nodeBase = *baseIndex;

  for (uint32_t i = 0; i < node->primitiveCount; i++) {
    ModelPrimitive* primitive = &model->primitives[node->primitiveIndex + i];
    ModelAttribute* positions = primitive->attributes[ATTR_POSITION];
    ModelAttribute* index = primitive->indices;
    if (!positions) continue;

    uint32_t base = nodeBase;

    // If 2 meshes in the node use the same vertex buffer, don't add the vertices twice
    for (uint32_t j = 0; j < i; j++) {
      if (model->primitives[node->primitiveIndex + j].attributes[ATTR_POSITION] == positions) {
        break;
      } else {
        base += model->primitives[node->primitiveIndex + j].attributes[ATTR_POSITION]->count;
      }
    }

    if (base == *baseIndex) {
      char* data = (char*) model->buffers[positions->buffer].data + positions->offset;
      size_t stride = positions->stride == 0 ? 3 * sizeof(float) : positions->stride;

      for (uint32_t j = 0; j < positions->count; j++) {
        float v[3];
        memcpy(v, data, 3 * sizeof(float));
        mat4_mulPoint(m, v);
        vec3_init(*vertices, v);
        *vertices += 3;
        data += stride;
      }

      *baseIndex += positions->count;
    }

    if (indices && index) {
      if (index->type != U16 && index->type != U32) lovrUnreachable();

      char* data = (char*) model->buffers[index->buffer].data + index->offset;
      size_t stride = index->stride == 0 ? (index->type == U16 ? 2 : 4) : index->stride;

      for (uint32_t j = 0; j < index->count; j++) {
        **indices = (index->type == U16 ? ((uint32_t) *(uint16_t*) data) : *(uint32_t*) data) + base;
        *indices += 1;
        data += stride;
      }
    } else {
      for (uint32_t j = 0; j < positions->count; j++) {
        **indices = j + base;
        *indices += 1;
      }
    }

  }

  for (uint32_t i = 0; i < node->childCount; i++) {
    collectVertices(model, node->children[i], vertices, indices, baseIndex, m);
  }
}

void lovrModelDataGetTriangles(ModelData* model, float** vertices, uint32_t** indices, uint32_t* vertexCount, uint32_t* indexCount) {
  if (model->totalVertexCount == 0) {
    countVertices(model, model->rootNode, &model->totalVertexCount, &model->totalIndexCount);
  }

  if (vertices && !model->vertices) {
    uint32_t* tempIndices;
    uint32_t baseIndex = 0;
    model->vertices = lovrMalloc(model->totalVertexCount * 3 * sizeof(float));
    model->indices = lovrMalloc(model->totalIndexCount * sizeof(uint32_t));
    *vertices = model->vertices;
    tempIndices = model->indices;
    collectVertices(model, model->rootNode, vertices, &tempIndices, &baseIndex, (float[16]) MAT4_IDENTITY);
  }

  if (vertexCount) *vertexCount = model->totalVertexCount;
  if (indexCount) *indexCount = model->totalIndexCount;

  if (vertices) *vertices = model->vertices;
  if (indices) *indices = model->indices;
}
