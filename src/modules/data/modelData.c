#include "data/modelData.h"
#include "data/blob.h"
#include "data/image.h"
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

  // Precomputed properties and validation

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
