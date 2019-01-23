#include "data/modelData.h"
#include "lib/math.h"
#include <stdio.h>

#define STARTS_WITH(a, b) !strncmp(a, b, strlen(b))

ModelData* lovrModelDataInitObj(ModelData* model, Blob* source, ModelDataIO io) {
  char* data = (char*) source->data;
  size_t length = source->size;

  vec_float_t vertexBuffer;
  vec_int_t indexBuffer;
  map_int_t vertexMap;
  vec_float_t vertices;
  vec_float_t normals;
  vec_float_t uvs;

  vec_init(&vertexBuffer);
  vec_init(&indexBuffer);
  map_init(&vertexMap);
  vec_init(&vertices);
  vec_init(&normals);
  vec_init(&uvs);

  while (length > 0) {
    int lineLength = 0;

    if (STARTS_WITH(data, "v ")) {
      float x, y, z;
      int count = sscanf(data + 2, "%f %f %f\n%n", &x, &y, &z, &lineLength);
      lovrAssert(count == 3, "Bad OBJ: Expected 3 coordinates for vertex position");
      vec_pusharr(&vertices, ((float[3]) { x, y, z }), 3);
    } else if (STARTS_WITH(data, "vn ")) {
      float x, y, z;
      int count = sscanf(data + 3, "%f %f %f\n%n", &x, &y, &z, &lineLength);
      lovrAssert(count == 3, "Bad OBJ: Expected 3 coordinates for vertex normal");
      vec_pusharr(&normals, ((float[3]) { x, y, z }), 3);
    } else if (STARTS_WITH(data, "vt ")) {
      float u, v;
      int count = sscanf(data + 3, "%f %f\n%n", &u, &v, &lineLength);
      lovrAssert(count == 2, "Bad OBJ: Expected 2 coordinates for texture coordinate");
      vec_pusharr(&uvs, ((float[2]) { u, v }), 2);
    } else if (STARTS_WITH(data, "f ")) {
      char* s = data + 2;
      for (int i = 0; i < 3; i++) {
        char terminator = i == 2 ? '\n' : ' ';
        char* space = strchr(s, terminator);
        if (space) {
          *space = '\0'; // I'll be back
          int* index = map_get(&vertexMap, s);
          if (index) {
            vec_push(&indexBuffer, *index);
          } else {
            int v, vt, vn;
            int newIndex = vertexBuffer.length / 8;
            vec_push(&indexBuffer, newIndex);
            map_set(&vertexMap, s, newIndex);

            // Can be improved
            if (sscanf(s, "%d/%d/%d", &v, &vt, &vn) == 3) {
              vec_pusharr(&vertexBuffer, vertices.data + 3 * (v - 1), 3);
              vec_pusharr(&vertexBuffer, normals.data + 3 * (vn - 1), 3);
              vec_pusharr(&vertexBuffer, uvs.data + 2 * (vt - 1), 2);
            } else if (sscanf(s, "%d//%d", &v, &vn) == 2) {
              vec_pusharr(&vertexBuffer, vertices.data + 3 * (v - 1), 3);
              vec_pusharr(&vertexBuffer, normals.data + 3 * (vn - 1), 3);
              vec_pusharr(&vertexBuffer, ((float[2]) { 0 }), 2);
            } else if (sscanf(s, "%d", &v) == 1) {
              vec_pusharr(&vertexBuffer, vertices.data + 3 * (v - 1), 3);
              vec_pusharr(&vertexBuffer, ((float[5]) { 0 }), 5);
            } else {
              lovrThrow("Bad OBJ: Unknown face format");
            }
          }
          *space = terminator;
          s = space + 1;
        }
      }
      lineLength = s - data;
    } else {
      char* newline = memchr(data, '\n', length);
      lineLength = newline - data + 1;
    }

    data += lineLength;
    length -= lineLength;
  }

  model->blobCount = 2;
  model->bufferCount = 2;
  model->attributeCount = 4;
  model->primitiveCount = 1;
  model->nodeCount = 1;
  lovrModelDataAllocate(model);

  model->blobs[0] = lovrBlobCreate(vertexBuffer.data, vertexBuffer.length * sizeof(float), "obj vertex data");
  model->blobs[1] = lovrBlobCreate(indexBuffer.data, indexBuffer.length * sizeof(int), "obj index data");

  model->buffers[0] = (ModelBuffer) {
    .data = model->blobs[0]->data,
    .size = model->blobs[0]->size,
    .stride = 8 * sizeof(float)
  };

  model->buffers[1] = (ModelBuffer) {
    .data = model->blobs[1]->data,
    .size = model->blobs[1]->size,
    .stride = sizeof(int)
  };

  model->attributes[0] = (ModelAttribute) {
    .buffer = 0,
    .offset = 0,
    .count = vertexBuffer.length / 8,
    .type = F32,
    .components = 3
  };

  model->attributes[1] = (ModelAttribute) {
    .buffer = 0,
    .offset = 3 * sizeof(float),
    .count = vertexBuffer.length / 8,
    .type = F32,
    .components = 3
  };

  model->attributes[2] = (ModelAttribute) {
    .buffer = 0,
    .offset = 6 * sizeof(float),
    .count = vertexBuffer.length / 8,
    .type = F32,
    .components = 2
  };

  model->attributes[3] = (ModelAttribute) {
    .buffer = 1,
    .offset = 0,
    .count = indexBuffer.length,
    .type = U32,
    .components = 1
  };

  model->primitives[0] = (ModelPrimitive) {
    .mode = DRAW_TRIANGLES,
    .attributes = {
      [ATTR_POSITION] = &model->attributes[0],
      [ATTR_NORMAL] = &model->attributes[1],
      [ATTR_TEXCOORD] = &model->attributes[2]
    },
    .indices = &model->attributes[3],
    .material = -1
  };

  model->nodes[0] = (ModelNode) {
    .transform = MAT4_IDENTITY,
    .primitiveIndex = 0,
    .primitiveCount = 1
  };

  map_deinit(&vertexMap);
  vec_deinit(&vertices);
  vec_deinit(&normals);
  vec_deinit(&uvs);
  return model;
}
