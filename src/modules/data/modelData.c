#include "data/modelData.h"
#include "data/blob.h"
#include "data/image.h"
#include "core/maf.h"
#include <stdlib.h>
#include <string.h>

static size_t typeSizes[] = {
  [I8] = 1,
  [U8] = 1,
  [I16] = 2,
  [U16] = 2,
  [I32] = 4,
  [U32] = 4,
  [F32] = 4
};

ModelData* lovrModelDataCreate(Blob* source, ModelDataIO* io) {
  ModelData* model = calloc(1, sizeof(ModelData));
  lovrAssert(model, "Out of memory");
  model->ref = 1;

  if (!lovrModelDataInitGltf(model, source, io)) {
    if (!lovrModelDataInitObj(model, source, io)) {
      if (!lovrModelDataInitStl(model, source, io)) {
        lovrThrow("Unable to load model from '%s'", source->name);
        return NULL;
      }
    }
  }

  lovrModelDataFinalize(model);

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
  map_free(&model->animationMap);
  map_free(&model->materialMap);
  map_free(&model->nodeMap);
  free(model->vertices);
  free(model->indices);
  free(model->metadata);
  free(model->data);
  free(model);
}

// Note: this code is a scary optimization
void lovrModelDataAllocate(ModelData* model) {
  size_t totalSize = 0;
  size_t sizes[13];
  size_t alignment = 8;
  totalSize += sizes[0] = ALIGN(model->blobCount * sizeof(Blob*), alignment);
  totalSize += sizes[1] = ALIGN(model->bufferCount * sizeof(ModelBuffer), alignment);
  totalSize += sizes[2] = ALIGN(model->imageCount * sizeof(Image*), alignment);
  totalSize += sizes[3] = ALIGN(model->materialCount * sizeof(ModelMaterial), alignment);
  totalSize += sizes[4] = ALIGN(model->attributeCount * sizeof(ModelAttribute), alignment);
  totalSize += sizes[5] = ALIGN(model->primitiveCount * sizeof(ModelPrimitive), alignment);
  totalSize += sizes[6] = ALIGN(model->animationCount * sizeof(ModelAnimation), alignment);
  totalSize += sizes[7] = ALIGN(model->skinCount * sizeof(ModelSkin), alignment);
  totalSize += sizes[8] = ALIGN(model->nodeCount * sizeof(ModelNode), alignment);
  totalSize += sizes[9] = ALIGN(model->channelCount * sizeof(ModelAnimationChannel), alignment);
  totalSize += sizes[10] = ALIGN(model->childCount * sizeof(uint32_t), alignment);
  totalSize += sizes[11] = ALIGN(model->jointCount * sizeof(uint32_t), alignment);
  totalSize += sizes[12] = model->charCount * sizeof(char);

  size_t offset = 0;
  char* p = model->data = calloc(1, totalSize);
  lovrAssert(model->data, "Out of memory");
  model->blobs = (Blob**) (p + offset), offset += sizes[0];
  model->buffers = (ModelBuffer*) (p + offset), offset += sizes[1];
  model->images = (Image**) (p + offset), offset += sizes[2];
  model->materials = (ModelMaterial*) (p + offset), offset += sizes[3];
  model->attributes = (ModelAttribute*) (p + offset), offset += sizes[4];
  model->primitives = (ModelPrimitive*) (p + offset), offset += sizes[5];
  model->animations = (ModelAnimation*) (p + offset), offset += sizes[6];
  model->skins = (ModelSkin*) (p + offset), offset += sizes[7];
  model->nodes = (ModelNode*) (p + offset), offset += sizes[8];
  model->channels = (ModelAnimationChannel*) (p + offset), offset += sizes[9];
  model->children = (uint32_t*) (p + offset), offset += sizes[10];
  model->joints = (uint32_t*) (p + offset), offset += sizes[11];
  model->chars = (char*) (p + offset), offset += sizes[12];

  map_init(&model->animationMap, model->animationCount);
  map_init(&model->materialMap, model->materialCount);
  map_init(&model->nodeMap, model->nodeCount);
}

void lovrModelDataFinalize(ModelData* model) {
  for (uint32_t i = 0; i < model->primitiveCount; i++) {
    model->primitives[i].skin = 0xaaaaaaaa;
  }

  for (uint32_t i = 0; i < model->nodeCount; i++) {
    ModelNode* node = &model->nodes[i];
    for (uint32_t j = 0, index = node->primitiveIndex; j < node->primitiveCount; j++, index++) {
      if (model->primitives[index].skin != 0xaaaaaaaa) {
        lovrCheck(model->primitives[index].skin == node->skin, "Model has a mesh used with multiple skins, which is not supported");
      } else {
        model->primitives[index].skin = node->skin;
      }
    }
  }

  model->indexType = U16;
  for (uint32_t i = 0; i < model->primitiveCount; i++) {
    ModelPrimitive* primitive = &model->primitives[i];

    uint32_t vertexCount = primitive->attributes[ATTR_POSITION]->count;
    if (primitive->skin != ~0u) {
      model->skins[primitive->skin].vertexCount += vertexCount;
      model->skinnedVertexCount += vertexCount;
    }
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

    for (uint32_t i = 0; i < MAX_DEFAULT_ATTRIBUTES; i++) {
      ModelAttribute* attribute = primitive->attributes[i];
      if (attribute) {
        attribute->stride = model->buffers[attribute->buffer].stride;
        if (attribute->stride == 0) {
          attribute->stride = typeSizes[attribute->type] * attribute->components;
        }
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
          ((float*) dst)[3] = 255;
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
          ((float*) dst)[3] = 255;
        }
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

    float corners[8][4] = {
      { min[0], min[1], min[2], 1.f },
      { min[0], min[1], max[2], 1.f },
      { min[0], max[1], min[2], 1.f },
      { min[0], max[1], max[2], 1.f },
      { max[0], min[1], min[2], 1.f },
      { max[0], min[1], max[2], 1.f },
      { max[0], max[1], min[2], 1.f },
      { max[0], max[1], max[2], 1.f }
    };

    for (uint32_t j = 0; j < 8; j++) {
      mat4_transform(m, corners[j]);
      memcpy(points + 3 * (*pointIndex)++, corners[j], 3 * sizeof(float));
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
    float* points = malloc(pointCount * 3 * sizeof(float));
    lovrAssert(points, "Out of memory");

    uint32_t pointIndex = 0;
    boundingSphereHelper(model, model->rootNode, &pointIndex, points, (float[16]) MAT4_IDENTITY);

    // Find point furthest away from first point

    float max = 0.f;
    float* a = NULL;
    for (uint32_t i = 1; i < pointCount; i++) {
      float dx = points[3 * i + 0] - points[0];
      float dy = points[3 * i + 1] - points[1];
      float dz = points[3 * i + 2] - points[2];
      float d2 = dx * dx + dy * dy + dz * dz;

      if (d2 > max) {
        a = &points[3 * i];
        max = d2;
      }
    }

    // Find point furthest away from that point

    max = 0.f;
    float* b = NULL;
    for (uint32_t i = 0; i < pointCount; i++) {
      float dx = points[3 * i + 0] - a[0];
      float dy = points[3 * i + 1] - a[1];
      float dz = points[3 * i + 2] - a[2];
      float d2 = dx * dx + dy * dy + dz * dz;

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
    free(points);
  }

  memcpy(sphere, model->boundingSphere, sizeof(model->boundingSphere));
}

static void countVertices(ModelData* model, uint32_t nodeIndex, uint32_t* vertexCount, uint32_t* indexCount) {
  ModelNode* node = &model->nodes[nodeIndex];

  for (uint32_t i = 0; i < node->primitiveCount; i++) {
    ModelPrimitive* primitive = &model->primitives[node->primitiveIndex + i];
    ModelAttribute* positions = primitive->attributes[ATTR_POSITION];
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

  for (uint32_t i = 0; i < node->primitiveCount; i++) {
    ModelPrimitive* primitive = &model->primitives[node->primitiveIndex + i];
    ModelAttribute* positions = primitive->attributes[ATTR_POSITION];
    ModelAttribute* index = primitive->indices;
    if (!positions) continue;

    char* data = (char*) model->buffers[positions->buffer].data + positions->offset;
    size_t stride = positions->stride == 0 ? 3 * sizeof(float) : positions->stride;

    for (uint32_t j = 0; j < positions->count; j++) {
      float v[4];
      memcpy(v, data, 3 * sizeof(float));
      mat4_transform(m, v);
      memcpy(*vertices, v, 3 * sizeof(float));
      *vertices += 3;
      data += stride;
    }

    if (index) {
      lovrAssert(index->type == U16 || index->type == U32, "Unreachable");

      data = (char*) model->buffers[index->buffer].data + index->offset;
      size_t stride = index->stride == 0 ? (index->type == U16 ? 2 : 4) : index->stride;

      for (uint32_t j = 0; j < index->count; j++) {
        **indices = (index->type == U16 ? ((uint32_t) *(uint16_t*) data) : *(uint32_t*) data) + *baseIndex;
        *indices += 1;
        data += stride;
      }
    } else {
      for (uint32_t j = 0; j < positions->count; j++) {
        **indices = j + *baseIndex;
        *indices += 1;
      }
    }

    *baseIndex += positions->count;
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
    uint32_t baseIndex = 0;
    model->vertices = malloc(model->totalVertexCount * 3 * sizeof(float));
    model->indices = malloc(model->totalIndexCount * sizeof(uint32_t));
    lovrAssert(model->vertices && model->indices, "Out of memory");
    *vertices = model->vertices;
    *indices = model->indices;
    collectVertices(model, model->rootNode, vertices, indices, &baseIndex, (float[16]) MAT4_IDENTITY);
  }

  *vertexCount = model->totalVertexCount;
  *indexCount = model->totalIndexCount;

  if (vertices) {
    *vertices = model->vertices;
    *indices = model->indices;
  }
}
