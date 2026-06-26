#include "data/modelData.h"
#include "data/blob.h"
#include "data/image.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>
#include <float.h>

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
  while (*s >= '0' && *s <= '9') { n = 10 * n + (*s++ - '0'); }
  *end = s;
  return n;
}

uint32_t packNormal(float* v) {
  return
    ((((uint32_t) (int32_t) (v[0] * 511.f)) & 0x3ff) <<  0) |
    ((((uint32_t) (int32_t) (v[1] * 511.f)) & 0x3ff) << 10) |
    ((((uint32_t) (int32_t) (v[2] * 511.f)) & 0x3ff) << 20);
}

static bool parseMtl(char* path, char* base, ModelDataIO* io, arr_image_t* images, arr_material_t* materials, map_t* names) {
  size_t size = 0;
  char* p = io(path, &size);
  lovrAssert(p, "Unable to read mtl from '%s'", path);
  char* data = p;

  while (size > 0) {
    while (size > 0 && (*data == ' ' || *data == '\t')) data++, size--;
    char* newline = memchr(data, '\n', size);
    if (*data == '#') goto next;

    char line[1024];
    size_t length = newline ? (size_t) (newline - data) : size;
    while (length > 0 && (data[length - 1] == '\r' || data[length - 1] == '\t' || data[length - 1] == ' ')) length--;

    if (length >= sizeof(line)) {
      lovrFree(p);
      return lovrSetError("OBJ MTL line length is too long (max is %d)", sizeof(line) - 1);
    }

    memcpy(line, data, length);
    line[length] = '\0';

    if (STARTS_WITH(line, "newmtl ")) {
      map_set(names, hash64(line + 7, length - 7), materials->length);
      arr_push(materials, ((ModelMaterial) {
        .color = { 1.f, 1.f, 1.f, 1.f },
        .glow = { 0.f, 0.f, 0.f, 1.f },
        .uvShift = { 0.f, 1.f },
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
        .metalnessTexture = ~0u,
        .roughnessTexture = ~0u,
        .clearcoatTexture = ~0u,
        .occlusionTexture = ~0u,
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
      if (materials->length == 0) {
        lovrFree(p);
        return lovrSetError("Tried to set a material property without declaring a material first");
      }

      const char* subpath = line + 7;

      if (subpath[0] == '/') {
        lovrFree(p);
        return lovrSetError("Absolute paths in models are not supported");
      }

      if (subpath[0] && subpath[1] && !memcmp(subpath, "./", 2)) subpath += 2;

      if (base - path + (length - 7) >= 1024) {
        lovrFree(p);
        return lovrSetError("Bad OBJ: Material image filename is too long");
      }

      memcpy(base, subpath, length - 7);
      base[length - 7] = '\0';

      size_t imageSize = 0;
      void* pixels = io(path, &imageSize);

      if (!pixels) {
        lovrFree(p);
        return lovrSetError("Unable to read image from %s", path);
      }

      Blob* blob = lovrBlobCreate(pixels, imageSize, NULL);
      Image* image = lovrImageCreateFromFile(blob);
      lovrRelease(blob, lovrBlobDestroy);

      if (!image) {
        lovrFree(p);
        return false;
      }

      ModelMaterial* material = &materials->data[materials->length - 1];
      material->texture = (uint32_t) images->length;
      arr_push(images, image);
    }

    next:
    if (!newline) break;
    size -= newline - data + 1;
    data = newline + 1;
  }

  lovrFree(p);
  return true;
}

bool lovrModelDataInitObj(ModelData** result, Blob* source, ModelDataIO* io) {
  if (source->size < 7 || (memcmp(source->data, "v ", 2) && memcmp(source->data, "o ", 2) && memcmp(source->data, "mtllib ", 7) && memcmp(source->data, "#", 1))) {
    return true;
  }

  char path[1024];
  size_t pathLength = strlen(source->name);
  lovrAssert(pathLength < sizeof(path), "OBJ filename is too long");
  memcpy(path, source->name, pathLength);
  path[pathLength] = '\0';
  char* slash = strrchr(path, '/');
  char* base = slash ? (slash + 1) : path;
  size_t baseLength = base - path;
  *base = '\0';

  ModelData* model = NULL;
  char* data = (char*) source->data;
  size_t size = source->size;

  arr_group_t groups;
  arr_image_t images;
  arr_material_t materials;
  arr_t(ModelVertex) vertices;
  arr_t(int) indices;
  map_t materialMap;
  map_t vertexMap;
  arr_t(float) positions;
  arr_t(float) normals;
  arr_t(float) uvs;

  arr_init(&groups);
  arr_init(&images);
  arr_init(&materials);
  map_init(&materialMap, 0);
  arr_init(&vertices);
  arr_init(&indices);
  map_init(&vertexMap, 0);
  arr_init(&positions);
  arr_init(&normals);
  arr_init(&uvs);

  arr_push(&groups, ((objGroup) { .material = -1 }));

  while (size > 0) {
    while (size > 0 && (*data == ' ' || *data == '\t')) data++, size--;
    char* newline = memchr(data, '\n', size);
    if (*data == '#') goto next;

    char line[1024];
    size_t length = newline ? (size_t) (newline - data) : size;
    while (length > 0 && (data[length - 1] == '\r' || data[length - 1] == '\t' || data[length - 1] == ' ')) length--;
    lovrAssertGoto(fail, length < sizeof(line), "OBJ line length is too long (max is %d)", sizeof(line) - 1);

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
      vt[1] = 1.0f - strtof(s, &s);
      arr_append(&uvs, vt, 2);
    } else if (line[0] == 'f' && line[1] == ' ') {
      char* s = line + 2;
      objGroup* group = &groups.data[groups.length - 1];
      for (size_t i = 0; *s; i++) {

        // Find first non-space
        while (*s && (*s == ' ' || *s == '\t')) s++;

        if (*s == '\n') {
          lovrCheckGoto(fail, i >= 3, "Bad OBJ: Face has no triangles");
          break;
        }

        // Find next non-number/non-slash
        char* t = s;
        while (*t && *t >= '/' && *t <= '9') t++;

        // Triangulate faces (triangle fan)
        if (i >= 3) {
          arr_push(&indices, indices.data[indices.length - (3 * (i - 2))]);
          arr_push(&indices, indices.data[indices.length - 2]);
          group->count += 2;
        }

        // If the vertex already exists, add its index and continue
        uint64_t hash = hash64(s, t - s);
        uint64_t index = map_get(&vertexMap, hash);
        if (index != MAP_NIL) {
          arr_push(&indices, index);
          group->count++;
          s = t;
          continue;
        }

        uint32_t v = 0;
        uint32_t vt = 0;
        uint32_t vn = 0;
        v = nomu32(s, &s);
        lovrCheckGoto(fail, v > 0, "Bad OBJ: Expected positive number for face vertex position index");

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
        arr_push(&indices, (int) vertices.length);
        map_set(&vertexMap, hash, vertices.length);

        float* position = &positions.data[3 * (v - 1)];
        float* normal = vn > 0 ? &normals.data[3 * (vn - 1)] : empty;
        float* uv = vt > 0 ? &uvs.data[2 * (vt - 1)] : empty;

        ModelVertex vertex = {
          .position = { position[0], position[1], position[2] },
#ifdef LOVR_WEBPU
          .normal = { normal[0], normal[1], normal[2] },
#else
          .normal = packNormal(normal),
#endif
          .uv = { uv[0], uv[1] },
          .color = { 0xff, 0xff, 0xff, 0xff }
        };

        arr_push(&vertices, vertex);
        group->count++;

        s = t;
      }
    } else if (STARTS_WITH(line, "mtllib ")) {
      const char* filename = line + 7;
      size_t filenameLength = strlen(filename);
      lovrCheckGoto(fail, filename[0] != '/', "Absolute paths in models are not supported");
      if (filenameLength > 2 && !memcmp(filename, "./", 2)) filename += 2;
      lovrCheckGoto(fail, baseLength + filenameLength < sizeof(path), "Bad OBJ: Material filename is too long");
      memcpy(path + baseLength, filename, filenameLength);
      path[baseLength + filenameLength] = '\0';

      if (!parseMtl(path, base, io, &images, &materials, &materialMap)) {
        goto fail;
      }
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

  if (vertices.length == 0 || indices.length == 0) {
    goto finish;
  }

  model = lovrCalloc(sizeof(ModelData));
  model->ref = 1;

  ModelMetadata* meta = &model->meta;
  meta->meshCount = 1;
  meta->vertexCount = (uint32_t) vertices.length;
  meta->indexCount = (uint32_t) indices.length;
  meta->indexSize = 4;
  meta->partCount = (uint32_t) groups.length;
  meta->nodeCount = 1;
  meta->imageCount = (uint32_t) images.length;
  meta->materialCount = (uint32_t) materials.length;

  lovrModelDataAllocate(model);

  memcpy(model->vertices, vertices.data, meta->vertexCount * sizeof(ModelVertex));
  memcpy(model->indices, indices.data, meta->indexCount * sizeof(uint32_t));

  if (meta->imageCount > 0) {
    memcpy(model->images, images.data, meta->imageCount * sizeof(Image*));
  }

  if (meta->materialCount > 0) {
    memcpy(meta->materials, materials.data, meta->materialCount * sizeof(ModelMaterial));
  }

  for (size_t i = 0; i < groups.length; i++) {
    objGroup* group = &groups.data[i];

    meta->parts[i] = (ModelPart) {
      .start = group->start,
      .count = group->count,
      .material = group->material,
      .mode = DRAW_TRIANGLE_LIST
    };

    float* bounds = meta->parts[i].bounds;

    for (size_t j = group->start; j < group->start + group->count; j++) {
      ModelVertex* vertex = &vertices.data[indices.data[j]];
      bounds[0] = MIN(bounds[0], vertex->position.x);
      bounds[1] = MAX(bounds[1], vertex->position.x);
      bounds[2] = MIN(bounds[2], vertex->position.y);
      bounds[3] = MAX(bounds[3], vertex->position.y);
      bounds[4] = MIN(bounds[4], vertex->position.z);
      bounds[5] = MAX(bounds[5], vertex->position.z);
    }
  }

  meta->meshes[0] = (ModelMesh) {
    .parts = meta->parts,
    .partCount = (uint32_t) groups.length,
    .vertexCount = meta->vertexCount,
    .indexCount = meta->indexCount,
    .skinDataOffset = ~0u,
    .blendDataOffset = ~0u
  };

  meta->nodes[0].mesh = 0;

finish:
  arr_free(&groups);
  arr_free(&images);
  arr_free(&materials);
  map_free(&materialMap);
  map_free(&vertexMap);
  arr_free(&vertices);
  arr_free(&indices);
  arr_free(&positions);
  arr_free(&normals);
  arr_free(&uvs);
  *result = model;
  return true;

fail:
  arr_free(&groups);
  arr_free(&images);
  arr_free(&materials);
  map_free(&materialMap);
  map_free(&vertexMap);
  arr_free(&vertices);
  arr_free(&indices);
  arr_free(&positions);
  arr_free(&normals);
  arr_free(&uvs);
  if (model) lovrModelDataDestroy(model);
  return false;
}
