#include "data/modelData.h"
#include "data/blob.h"
#include "util.h"
#include <string.h>
#include <float.h>

static bool lovrModelDataInitStlAscii(ModelData** result, Blob* source, ModelDataIO* io) {
  return lovrSetError("ASCII STL files are not supported yet");
}

// The binary format has an 80 byte header, followed by a u32 triangle count, followed by 50 byte
// triangles.  Each triangle has a vec3 normal, 3 vec3 vertices, and 2 bytes of padding.
static bool lovrModelDataInitStlBinary(ModelData** result, Blob* source, ModelDataIO* io, uint32_t triangleCount) {
  char* data = (char*) source->data + 84;

  uint32_t vertexCount = triangleCount * 3;

  ModelData* model = lovrCalloc(sizeof(ModelData));
  model->ref = 1;
  model->meta.meshCount = 1;
  model->meta.vertexCount = vertexCount;
  model->meta.meshCount = 1;
  model->meta.nodeCount = 1;

  lovrModelDataAllocate(model);

  model->meta.meshes[0].vertexOffset = 0;
  model->meta.meshes[0].vertexCount = vertexCount;
  model->meta.nodes[0].mesh = 0;

  ModelVertex* vertices = model->vertices;
  float* bounds = model->meta.parts[0].bounds;

  for (uint32_t i = 0; i < triangleCount; i++) {
    float* f = (float*) data;
    float* v[3] = { f + 3, f + 6, f + 9 };

    uint32_t normal =
      ((((uint32_t) (int32_t) (f[0] * 511.f)) & 0x3ff) <<  0) |
      ((((uint32_t) (int32_t) (f[1] * 511.f)) & 0x3ff) << 10) |
      ((((uint32_t) (int32_t) (f[2] * 511.f)) & 0x3ff) << 20);

    for (uint32_t j = 0; j < 3; j++) {
      *vertices++ = (ModelVertex) {
        .position = { v[j][0], v[j][1], v[j][2] },
        .normal = normal,
        .color = { 0xff, 0xff, 0xff, 0xff }
      };
      bounds[0] = MIN(bounds[0], v[j][0]);
      bounds[1] = MAX(bounds[1], v[j][0]);
      bounds[2] = MIN(bounds[2], v[j][1]);
      bounds[3] = MAX(bounds[3], v[j][1]);
      bounds[4] = MIN(bounds[4], v[j][2]);
      bounds[5] = MAX(bounds[5], v[j][2]);
    }

    data += 50;
  }

  *result = model;
  return true;
}

bool lovrModelDataInitStl(ModelData** result, Blob* source, ModelDataIO* io) {
  if (source->size > strlen("solid ") && !memcmp(source->data, "solid ", strlen("solid "))) {
    return lovrModelDataInitStlAscii(result, source, io);
  } else if (source->size > 84) {
    uint32_t triangleCount;
    memcpy(&triangleCount, (char*) source->data + 80, sizeof(triangleCount));
    if (source->size == 84 + 50 * triangleCount) {
      return lovrModelDataInitStlBinary(result, source, io, triangleCount);
    }
  }

  return true;
}
