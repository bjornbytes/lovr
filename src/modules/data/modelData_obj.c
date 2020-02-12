#include "data/modelData.h"
#include "data/blob.h"
#include "data/textureData.h"
#include "core/arr.h"
#include "core/hash.h"
#include "core/maf.h"
#include "core/map.h"
#include "core/ref.h"
#include "core/util.h"
#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <ctype.h>

typedef struct {
  uint32_t material;
  int start;
  int count;
} objGroup;

typedef arr_t(ModelMaterial) arr_material_t;
typedef arr_t(TextureData*) arr_texturedata_t;
typedef arr_t(objGroup) arr_group_t;

#define STARTS_WITH(a, b) !strncmp(a, b, strlen(b))

static void parseMtl(char* path, ModelDataIO* io, arr_texturedata_t* textures, arr_material_t* materials, map_t* names, char* base) {
  size_t length = 0;
  char* data = io(path, &length);
  lovrAssert(data && length > 0, "Unable to read mtl from '%s'", path);
  char* s = data;

  while (length > 0) {
    int lineLength = 0;

    if (STARTS_WITH(s, "newmtl ")) {
      char name[128];
      size_t length = sscanf(s + 7, "%s\n%n", name, &lineLength);
      lovrAssert(length > 0, "Bad OBJ: Expected a material name");
      map_set(names, hash64(name, length), materials->length);
      arr_push(materials, ((ModelMaterial) {
        .scalars[SCALAR_METALNESS] = 1.f,
        .scalars[SCALAR_ROUGHNESS] = 1.f,
        .colors[COLOR_DIFFUSE] = { 1.f, 1.f, 1.f, 1.f },
        .colors[COLOR_EMISSIVE] = { 0.f, 0.f, 0.f, 0.f }
      }));
      memset(&materials->data[materials->length - 1].textures, 0xff, MAX_MATERIAL_TEXTURES * sizeof(int));
    } else if (STARTS_WITH(s, "Kd")) {
      float r, g, b;
      int count = sscanf(s + 2, "%f %f %f\n%n", &r, &g, &b, &lineLength);
      lovrAssert(count == 3, "Bad OBJ: Expected 3 components for diffuse color");
      ModelMaterial* material = &materials->data[materials->length - 1];
      material->colors[COLOR_DIFFUSE] = (Color) { r, g, b, 1.f };
    } else if (STARTS_WITH(s, "map_Kd")) {

      // Read file
      char filename[128];
      bool hasFilename = sscanf(s + 7, "%s\n%n", filename, &lineLength);
      lovrAssert(hasFilename, "Bad OBJ: Expected a texture filename");
      char path[1024];
      snprintf(path, sizeof(path), "%s%s", base, filename);
      size_t size = 0;
      void* data = io(path, &size);
      lovrAssert(data && size > 0, "Unable to read texture from %s", path);
      Blob* blob = lovrBlobCreate(data, size, NULL);

      // Load texture, assign to material
      TextureData* texture = lovrTextureDataCreateFromBlob(blob, true);
      lovrAssert(materials->length > 0, "Tried to set a material property without declaring a material first");
      ModelMaterial* material = &materials->data[materials->length - 1];
      material->textures[TEXTURE_DIFFUSE] = (uint32_t) textures->length;
      material->filters[TEXTURE_DIFFUSE].mode = FILTER_TRILINEAR;
      material->wraps[TEXTURE_DIFFUSE] = (TextureWrap) { .s = WRAP_REPEAT, .t = WRAP_REPEAT };
      arr_push(textures, texture);
      lovrRelease(Blob, blob);
    } else {
      char* newline = memchr(s, '\n', length);
      lineLength = newline - s + 1;
    }

    s += lineLength;
    length -= lineLength;
    while (length && (*s == ' ' || *s == '\t')) length--, s++;
  }

  free(data);
}

ModelData* lovrModelDataInitObj(ModelData* model, Blob* source, ModelDataIO* io) {
  char* data = (char*) source->data;
  size_t length = source->size;

  if (!memchr(data, '\n', length)) {
    return NULL;
  }

  float min[4] = { FLT_MAX };
  float max[4] = { FLT_MIN };

  arr_group_t groups;
  arr_texturedata_t textures;
  arr_material_t materials;
  arr_t(float) vertexBlob;
  arr_t(int) indexBlob;
  map_t materialMap;
  map_t vertexMap;
  arr_t(float) positions;
  arr_t(float) normals;
  arr_t(float) uvs;

  arr_init(&groups);
  arr_init(&textures);
  arr_init(&materials);
  map_init(&materialMap, 0);
  arr_init(&vertexBlob);
  arr_init(&indexBlob);
  map_init(&vertexMap, 0);
  arr_init(&positions);
  arr_init(&normals);
  arr_init(&uvs);

  arr_push(&groups, ((objGroup) { .material = -1 }));

  char base[1024];
  lovrAssert(strlen(source->name) < sizeof(base), "OBJ filename is too long");
  strcpy(base, source->name);
  char* slash = strrchr(base, '/');
  char* root = slash ? (slash + 1) : base;
  *root = '\0';

  while (length > 0) {
    int lineLength = 0;

    if (STARTS_WITH(data, "v ")) {
      float x, y, z;
      int count = sscanf(data + 2, "%f %f %f\n%n", &x, &y, &z, &lineLength);
      lovrAssert(count == 3, "Bad OBJ: Expected 3 coordinates for vertex position");
      min[0] = MIN(min[0], x);
      max[0] = MAX(max[0], x);
      min[1] = MIN(min[1], y);
      max[1] = MAX(max[1], y);
      min[2] = MIN(min[2], z);
      max[2] = MAX(max[2], z);
      arr_append(&positions, ((float[3]) { x, y, z }), 3);
    } else if (STARTS_WITH(data, "vn ")) {
      float x, y, z;
      int count = sscanf(data + 3, "%f %f %f\n%n", &x, &y, &z, &lineLength);
      lovrAssert(count == 3, "Bad OBJ: Expected 3 coordinates for vertex normal");
      arr_append(&normals, ((float[3]) { x, y, z }), 3);
    } else if (STARTS_WITH(data, "vt ")) {
      float u, v;
      int count = sscanf(data + 3, "%f %f\n%n", &u, &v, &lineLength);
      lovrAssert(count == 2, "Bad OBJ: Expected 2 coordinates for texture coordinate");
      arr_append(&uvs, ((float[2]) { u, v }), 2);
    } else if (STARTS_WITH(data, "f ")) {
      char* s = data + 2;
      for (int i = 0; i < 3; i++) {
        char terminator = i == 2 ? '\n' : ' ';
        char* space = strchr(s, terminator);
        if (space) {
          uint64_t hash = hash64(s, space - s);
          uint64_t index = map_get(&vertexMap, hash);
          if (index != MAP_NIL) {
            arr_push(&indexBlob, index);
          } else {
            int v, vt, vn;
            uint64_t newIndex = vertexBlob.length / 8;
            arr_push(&indexBlob, newIndex);
            map_set(&vertexMap, hash, newIndex);

            // Can be improved
            if (sscanf(s, "%d/%d/%d", &v, &vt, &vn) == 3) {
              arr_append(&vertexBlob, positions.data + 3 * (v - 1), 3);
              arr_append(&vertexBlob, normals.data + 3 * (vn - 1), 3);
              arr_append(&vertexBlob, uvs.data + 2 * (vt - 1), 2);
            } else if (sscanf(s, "%d//%d", &v, &vn) == 2) {
              arr_append(&vertexBlob, positions.data + 3 * (v - 1), 3);
              arr_append(&vertexBlob, normals.data + 3 * (vn - 1), 3);
              arr_append(&vertexBlob, ((float[2]) { 0 }), 2);
            } else if (sscanf(s, "%d/%d", &v, &vt) == 2) {
              arr_append(&vertexBlob, positions.data + 3 * (v - 1), 3);
              arr_append(&vertexBlob, ((float[3]) { 0 }), 3);
              arr_append(&vertexBlob, uvs.data + 2 * (vt - 1), 2);
            } else if (sscanf(s, "%d", &v) == 1) {
              arr_append(&vertexBlob, positions.data + 3 * (v - 1), 3);
              arr_append(&vertexBlob, ((float[5]) { 0 }), 5);
            } else {
              lovrThrow("Bad OBJ: Unknown face format");
            }
          }
          s = space + 1;
        }
      }
      groups.data[groups.length - 1].count += 3;
      lineLength = s - data;
    } else if (STARTS_WITH(data, "mtllib ")) {
      char filename[1024];
      bool hasName = sscanf(data + 7, "%1024s\n%n", filename, &lineLength);
      lovrAssert(hasName, "Bad OBJ: Expected filename after mtllib");
      char path[1024];
      snprintf(path, sizeof(path), "%s%s", base, filename);
      parseMtl(path, io, &textures, &materials, &materialMap, base);
    } else if (STARTS_WITH(data, "usemtl ")) {
      char name[128];
      uint64_t length = sscanf(data + 7, "%s\n%n", name, &lineLength);
      uint64_t material = map_get(&materialMap, hash64(name, length));
      lovrAssert(length > 0, "Bad OBJ: Expected a valid material name");

      // If the last group didn't have any faces, just reuse it, otherwise make a new group
      objGroup* group = &groups.data[groups.length - 1];
      if (group->count > 0) {
        int start = group->start + group->count; // Don't put this in the compound literal (realloc)
        arr_push(&groups, ((objGroup) {
          .material = material == MAP_NIL ? -1 : material,
          .start = start,
          .count = 0
        }));
      } else {
        group->material = material == MAP_NIL ? -1 : material;
      }
    } else {
      char* newline = memchr(data, '\n', length);
      if (!newline) {
        break;
      }
      lineLength = newline - data + 1;
    }

    data += lineLength;
    length -= lineLength;
    while (length && (*data == ' ' || *data == '\t')) length--, data++;
  }

  if (vertexBlob.length == 0 || indexBlob.length == 0) {
    return NULL;
  }

  model->blobCount = 2;
  model->bufferCount = 2;
  model->attributeCount = 3 + (uint32_t) groups.length;
  model->primitiveCount = (uint32_t) groups.length;
  model->nodeCount = 1;
  model->textureCount = (uint32_t) textures.length;
  model->materialCount = (uint32_t) materials.length;
  lovrModelDataAllocate(model);

  model->blobs[0] = lovrBlobCreate(vertexBlob.data, vertexBlob.length * sizeof(float), "obj vertex data");
  model->blobs[1] = lovrBlobCreate(indexBlob.data, indexBlob.length * sizeof(int), "obj index data");

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

  memcpy(model->textures, textures.data, model->textureCount * sizeof(TextureData*));
  memcpy(model->materials, materials.data, model->materialCount * sizeof(ModelMaterial));
  model->materialMap = materialMap; // Copy by value, no need to free original, questionable

  model->attributes[0] = (ModelAttribute) {
    .buffer = 0,
    .offset = 0,
    .count = (uint32_t) vertexBlob.length / 8,
    .type = F32,
    .components = 3,
    .hasMin = true,
    .hasMax = true,
    .min[0] = min[0],
    .min[1] = min[1],
    .min[2] = min[2],
    .max[0] = max[0],
    .max[1] = max[1],
    .max[2] = max[2]
  };

  model->attributes[1] = (ModelAttribute) {
    .buffer = 0,
    .offset = 3 * sizeof(float),
    .count = (uint32_t) vertexBlob.length / 8,
    .type = F32,
    .components = 3
  };

  model->attributes[2] = (ModelAttribute) {
    .buffer = 0,
    .offset = 6 * sizeof(float),
    .count = (uint32_t) vertexBlob.length / 8,
    .type = F32,
    .components = 2
  };

  for (size_t i = 0; i < groups.length; i++) {
    objGroup* group = &groups.data[i];
    model->attributes[3 + i] = (ModelAttribute) {
      .buffer = 1,
      .offset = group->start * sizeof(int),
      .count = group->count,
      .type = U32,
      .components = 1
    };
  }

  for (size_t i = 0; i < groups.length; i++) {
    objGroup* group = &groups.data[i];
    model->primitives[i] = (ModelPrimitive) {
      .mode = DRAW_TRIANGLES,
      .attributes = {
        [ATTR_POSITION] = &model->attributes[0],
        [ATTR_NORMAL] = &model->attributes[1],
        [ATTR_TEXCOORD] = &model->attributes[2]
      },
      .indices = &model->attributes[3 + i],
      .material = group->material
    };
  }

  model->nodes[0] = (ModelNode) {
    .transform.matrix = MAT4_IDENTITY,
    .primitiveIndex = 0,
    .primitiveCount = (uint32_t) groups.length,
    .skin = ~0u,
    .matrix = true
  };

  arr_free(&groups);
  arr_free(&textures);
  arr_free(&materials);
  map_free(&vertexMap);
  arr_free(&positions);
  arr_free(&normals);
  arr_free(&uvs);
  return model;
}
