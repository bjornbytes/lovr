#include "data/modelData.h"
#include "data/blob.h"
#include "core/maf.h"
#include "core/util.h"
#include <stdlib.h>
#include <string.h>

static ModelData* lovrModelDataInitStlAscii(ModelData* model, Blob* source, ModelDataIO* io) {
  lovrThrow("ASCII STL files are not supported yet");
  return NULL;
}

// The binary format has an 80 byte header, followed by a u32 triangle count, followed by 50 byte
// triangles.  Each triangle has a vec3 normal, 3 vec3 vertices, and 2 bytes of padding.
extern ModelData* lovrModelDataInitStlBinary(ModelData* model, Blob* source, ModelDataIO* io, uint32_t triangleCount) {
  char* data = (char*) source->data + 84;

  uint32_t vertexCount = triangleCount * 3;
  size_t vertexBufferSize = vertexCount * 6 * sizeof(float);
  float* vertices = malloc(vertexBufferSize);
  lovrAssert(vertices, "Out of memory");

  model->blobCount = 1;
  model->bufferCount = 1;
  model->attributeCount = 2;
  model->primitiveCount = 1;
  model->nodeCount = 1;

  lovrModelDataAllocate(model);

  model->blobs[0] = lovrBlobCreate(vertices, vertexBufferSize, "stl vertex data");
  model->buffers[0] = (ModelBuffer) { .data = (char*) vertices, .size = vertexBufferSize, .stride = 6 * sizeof(float) };
  model->attributes[0] = (ModelAttribute) { .count = vertexCount, .components = 3, .type = F32, .offset = 0 * sizeof(float) };
  model->attributes[1] = (ModelAttribute) { .count = vertexCount, .components = 3, .type = F32, .offset = 3 * sizeof(float) };
  model->primitives[0] = (ModelPrimitive) {
    .mode = DRAW_TRIANGLES,
    .material = ~0u,
    .attributes = {
      [ATTR_POSITION] = &model->attributes[0],
      [ATTR_NORMAL] = &model->attributes[1]
    }
  };
  model->nodes[0] = (ModelNode) {
    .matrix = true,
    .transform.matrix = MAT4_IDENTITY,
    .primitiveCount = 1,
    .skin = ~0u
  };

  for (uint32_t i = 0; i < triangleCount; i++) {
    memcpy(vertices + 3, data, 12);
    memcpy(vertices + 9, data, 12);
    memcpy(vertices + 15, data, 12), data += 12;
    memcpy(vertices + 0, data, 12), data += 12;
    memcpy(vertices + 6, data, 12), data += 12;
    memcpy(vertices + 12, data, 12), data += 12;
    vertices += 18;
    data += 2;
  }

  return model;
}

ModelData* lovrModelDataInitStl(ModelData* model, Blob* source, ModelDataIO* io) {
  if (source->size > strlen("solid ") && !memcmp(source->data, "solid ", strlen("solid "))) {
    return lovrModelDataInitStlAscii(model, source, io);
  } else if (source->size > 84) {
    uint32_t triangleCount;
    memcpy(&triangleCount, (char*) source->data + 80, sizeof(triangleCount));
    if (source->size == 84 + 50 * triangleCount) {
      return lovrModelDataInitStlBinary(model, source, io, triangleCount);
    }
  }

  return NULL;
}
