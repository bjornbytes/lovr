#include "data/modelData.h"
#include "data/blob.h"
#include "data/image.h"
#include "core/maf.h"
#include "util.h"
#include <stdlib.h>
#include <float.h>
#include <ctype.h>

typedef struct {
  uint32_t material;
  int start;
  int count;
} objGroup;

typedef arr_t(ModelMaterial) arr_material_t;
typedef arr_t(Image*) arr_image_t;
typedef arr_t(objGroup) arr_group_t;

#define STARTS_WITH(a, b) !strncmp(a, b, strlen(b))

static uint32_t nomu32(char* s, char** end) {
  uint32_t n = 0;
  while (isdigit(*s)) { n = 10 * n + (*s++ - '0'); }
  *end = s;
  return n;
}

static void parseMtl(char* path, char* base, ModelDataIO* io, arr_image_t* images, arr_material_t* materials, map_t* names) {
  size_t size = 0;
  char* p = io(path, &size);
  lovrAssert(p && size > 0, "Unable to read mtl from '%s'", path);
  char* data = p;

  while (size > 0) {
    while (size > 0 && (*data == ' ' || *data == '\t')) data++, size--;
    char* newline = memchr(data, '\n', size);
    if (*data == '#') goto next;

    char line[1024];
    size_t length = newline ? (size_t) (newline - data) : size;
    while (length > 0 && (data[length - 1] == '\r' || data[length - 1] == '\t' || data[length - 1] == ' ')) length--;
    lovrAssert(length < sizeof(line), "OBJ MTL line length is too long (max is %d)", sizeof(line) - 1);
    memcpy(line, data, length);
    line[length] = '\0';

    if (STARTS_WITH(line, "newmtl ")) {
      map_set(names, hash64(line + 7, length - 7), materials->length);
      arr_push(materials, ((ModelMaterial) {
        .color = { 1.f, 1.f, 1.f, 1.f },
        .glow = { 0.f, 0.f, 0.f, 1.f },
        .uvShift = { 0.f, 0.f },
        .uvScale = { 1.f, 1.f },
        .metalness = 1.f,
        .roughness = 1.f,
        .clearcoat = 0.f,
        .clearcoatRoughness = 0.f,
        .occlusionStrength = 1.f,
        .normalScale = 1.f,
        .alphaCutoff = 0.f,
        .texture = ~0u,
        .glowTexture = ~0u,
        .occlusionTexture = ~0u,
        .metalnessTexture = ~0u,
        .roughnessTexture = ~0u,
        .clearcoatTexture = ~0u,
        .normalTexture = ~0u
      }));
    } else if (line[0] == 'K' && line[1] == 'd' && line[2] == ' ') {
      float r, g, b;
      char* s = line + 3;
      r = strtof(s, &s);
      g = strtof(s, &s);
      b = strtof(s, &s);
      ModelMaterial* material = &materials->data[materials->length - 1];
      memcpy(material->color, (float[4]) { r, g, b, 1.f }, 16);
    } else if (STARTS_WITH(line, "map_Kd ")) {
      lovrAssert(base - path + (length - 7) < 1024, "Bad OBJ: Material image filename is too long");
      memcpy(base, line + 7, length - 7);
      base[length - 7] = '\0';

      size_t imageSize = 0;
      void* pixels = io(path, &imageSize);
      lovrAssert(pixels && imageSize > 0, "Unable to read image from %s", path);
      Blob* blob = lovrBlobCreate(pixels, imageSize, NULL);

      Image* image = lovrImageCreateFromFile(blob);
      lovrAssert(materials->length > 0, "Tried to set a material property without declaring a material first");
      ModelMaterial* material = &materials->data[materials->length - 1];
      material->texture = (uint32_t) images->length;
      arr_push(images, image);
      lovrRelease(blob, lovrBlobDestroy);
    }

    next:
    if (!newline) break;
    size -= newline - data + 1;
    data = newline + 1;
  }

  free(p);
}

ModelData* lovrModelDataInitObj(ModelData* model, Blob* source, ModelDataIO* io) {
  if (source->size < 7 || (memcmp(source->data, "v ", 2) && memcmp(source->data, "o ", 2) && memcmp(source->data, "mtllib ", 7) && memcmp(source->data, "#", 1))) {
    return NULL;
  }

  char* data = (char*) source->data;
  size_t size = source->size;

  arr_group_t groups;
  arr_image_t images;
  arr_material_t materials;
  arr_t(float) vertexBlob;
  arr_t(int) indexBlob;
  map_t materialMap;
  map_t vertexMap;
  arr_t(float) positions;
  arr_t(float) normals;
  arr_t(float) uvs;

  arr_init(&groups, arr_alloc);
  arr_init(&images, arr_alloc);
  arr_init(&materials, arr_alloc);
  map_init(&materialMap, 0);
  arr_init(&vertexBlob, arr_alloc);
  arr_init(&indexBlob, arr_alloc);
  map_init(&vertexMap, 0);
  arr_init(&positions, arr_alloc);
  arr_init(&normals, arr_alloc);
  arr_init(&uvs, arr_alloc);

  arr_push(&groups, ((objGroup) { .material = -1 }));

  char path[1024];
  size_t pathLength = strlen(source->name);
  lovrAssert(pathLength < sizeof(path), "OBJ filename is too long");
  memcpy(path, source->name, pathLength);
  path[pathLength] = '\0';
  char* slash = strrchr(path, '/');
  char* base = slash ? (slash + 1) : path;
  size_t baseLength = base - path;
  *base = '\0';

  while (size > 0) {
    while (size > 0 && (*data == ' ' || *data == '\t')) data++, size--;
    char* newline = memchr(data, '\n', size);
    if (*data == '#') goto next;

    char line[1024];
    size_t length = newline ? (size_t) (newline - data) : size;
    while (length > 0 && (data[length - 1] == '\r' || data[length - 1] == '\t' || data[length - 1] == ' ')) length--;
    lovrAssert(length < sizeof(line), "OBJ line length is too long (max is %d)", sizeof(line) - 1);
    memcpy(line, data, length);
    line[length] = '\0';

    if (line[0] == 'v' && line[1] == ' ') {
      float v[3];
      char* s = line + 2;
      v[0] = strtof(s, &s);
      v[1] = strtof(s, &s);
      v[2] = strtof(s, &s);
      arr_append(&positions, v, 3);
    } else if (line[0] == 'v' && line[1] == 'n' && line[2] == ' ') {
      float vn[3];
      char* s = line + 3;
      vn[0] = strtof(s, &s);
      vn[1] = strtof(s, &s);
      vn[2] = strtof(s, &s);
      arr_append(&normals, vn, 3);
    } else if (line[0] == 'v' && line[1] == 't' && line[2] == ' ') {
      float vt[2];
      char* s = line + 3;
      vt[0] = strtof(s, &s);
      vt[1] = strtof(s, &s);
      arr_append(&uvs, vt, 2);
    } else if (line[0] == 'f' && line[1] == ' ') {
      char* s = line + 2;
      objGroup* group = &groups.data[groups.length - 1];
      for (size_t i = 0; *s; i++) {

        // Find first non-space
        while (*s && (*s == ' ' || *s == '\t')) s++;

        if (*s == '\n') {
          lovrAssert(i >= 3, "Bad OBJ: Face has no triangles");
          break;
        }

        // Find next non-number/non-slash
        char* t = s;
        while (*t && *t >= '/' && *t <= '9') t++;

        // Triangulate faces (triangle fan)
        if (i >= 3) {
          arr_push(&indexBlob, indexBlob.data[indexBlob.length - i]);
          arr_push(&indexBlob, indexBlob.data[indexBlob.length - 2]);
          group->count += 2;
        }

        // If the vertex already exists, add its index and continue
        uint64_t hash = hash64(s, t - s);
        uint64_t index = map_get(&vertexMap, hash);
        if (index != MAP_NIL) {
          arr_push(&indexBlob, index);
          group->count++;
          s = t;
          continue;
        }

        uint32_t v = 0;
        uint32_t vt = 0;
        uint32_t vn = 0;
        v = nomu32(s, &s);
        lovrAssert(v > 0, "Bad OBJ: Expected positive number for face vertex position index");

        // Handle v//vn, v/vt, v/vt/vtn, and v
        if (s[0] == '/') {
          if (s[1] == '/') {
            vn = nomu32(s + 2, &s);
          } else {
            vt = nomu32(s + 1, &s);
            if (s[0] == '/') {
              vn = nomu32(s + 1, &s);
            }
          }
        }

        float empty[3] = { 0.f };
        arr_push(&indexBlob, (int) vertexBlob.length / 8);
        map_set(&vertexMap, hash, vertexBlob.length / 8);
        arr_append(&vertexBlob, positions.data + 3 * (v - 1), 3);
        arr_append(&vertexBlob, vn > 0 ? (normals.data + 3 * (vn - 1)) : empty, 3);
        arr_append(&vertexBlob, vt > 0 ? (uvs.data + 2 * (vt - 1)) : empty, 2);
        group->count++;

        s = t;
      }
    } else if (STARTS_WITH(line, "mtllib ")) {
      const char* filename = line + 7;
      size_t filenameLength = strlen(filename);
      lovrAssert(baseLength + filenameLength < sizeof(path), "Bad OBJ: Material filename is too long");
      memcpy(path + baseLength, filename, filenameLength);
      path[baseLength + filenameLength] = '\0';
      parseMtl(path, base, io, &images, &materials, &materialMap);
    } else if (STARTS_WITH(line, "usemtl ")) {
      uint64_t index = map_get(&materialMap, hash64(line + 7, length - 7));
      uint32_t material = index == MAP_NIL ? ~0u : index;
      objGroup* group = &groups.data[groups.length - 1];
      if (group->count > 0) {
        objGroup next = { .material = material, .start = group->start + group->count };
        arr_push(&groups, next);
      } else { // If the group doesn't have any faces yet, it's safe to modify its material
        group->material = material;
      }
    }

    next:
    if (!newline) break;
    size -= newline - data + 1;
    data = newline + 1;
  }

  if (vertexBlob.length == 0 || indexBlob.length == 0) {
    model = NULL;
    goto finish;
  }

  model->blobCount = 2;
  model->bufferCount = 2;
  model->attributeCount = 3 + (uint32_t) groups.length;
  model->primitiveCount = (uint32_t) groups.length;
  model->nodeCount = 1;
  model->imageCount = (uint32_t) images.length;
  model->materialCount = (uint32_t) materials.length;
  lovrModelDataAllocate(model);

  model->blobs[0] = lovrBlobCreate(vertexBlob.data, vertexBlob.length * sizeof(float), "obj vertex data");
  model->blobs[1] = lovrBlobCreate(indexBlob.data, indexBlob.length * sizeof(int), "obj index data");

  model->buffers[0] = (ModelBuffer) {
    .blob = 0,
    .data = model->blobs[0]->data,
    .size = model->blobs[0]->size,
    .stride = 8 * sizeof(float)
  };

  model->buffers[1] = (ModelBuffer) {
    .blob = 1,
    .data = model->blobs[1]->data,
    .size = model->blobs[1]->size,
    .stride = sizeof(int)
  };

  memcpy(model->images, images.data, model->imageCount * sizeof(Image*));
  memcpy(model->materials, materials.data, model->materialCount * sizeof(ModelMaterial));
  memcpy(model->materialMap.hashes, materialMap.hashes, materialMap.size * sizeof(uint64_t));
  memcpy(model->materialMap.values, materialMap.values, materialMap.size * sizeof(uint64_t));

  float min[4] = { FLT_MAX };
  float max[4] = { FLT_MIN };

  for (size_t i = 0; i < vertexBlob.length; i += 8) {
    float* v = vertexBlob.data + i;
    min[0] = MIN(min[0], v[0]);
    max[0] = MAX(max[0], v[0]);
    min[1] = MIN(min[1], v[1]);
    max[1] = MAX(max[1], v[1]);
    min[2] = MIN(min[2], v[2]);
    max[2] = MAX(max[2], v[2]);
  }

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
        [ATTR_UV] = &model->attributes[2]
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
    .hasMatrix = true
  };

finish:
  arr_free(&groups);
  arr_free(&images);
  arr_free(&materials);
  map_free(&materialMap);
  map_free(&vertexMap);
  arr_free(&positions);
  arr_free(&normals);
  arr_free(&uvs);
  return model;
}
