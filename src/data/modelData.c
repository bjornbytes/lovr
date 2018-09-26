#include "data/modelData.h"
#include "lib/math.h"
#include "lib/jsmn/jsmn.h"
#include <stdbool.h>

// Notes:
//   - We parse in two passes.
//     - In the first pass we figure out how much memory we need to allocate.
//     - Then we allocate the memory and do a second pass to fill everything in.
//   - Plan is to make this parser destructive, so it mutates the input Blob in order to avoid doing
//     work in some situations, for speed.  May make sense to provide a non-destructive option.
//     - Currently this is most useful for reusing string memory by changing quotes to \0's.
//   - Caveats:
//     - The IO callback must not hang onto the filenames that are passed into it.

#define MAGIC_glTF 0x46546c67
#define MAGIC_JSON 0x4e4f534a
#define MAGIC_BIN 0x004e4942

#define KEY_EQ(k, s) !strncmp(k.data, s, k.length)
#define TOK_INT(j, t) strtol(j + t->start, NULL, 10)
#define TOK_BOOL(j, t) (*(j + t->start) == 't')
#define TOK_FLOAT(j, t) strtof(j + t->start, NULL)

typedef struct {
  struct { int count; jsmntok_t* token; } accessors;
  struct { int count; jsmntok_t* token; } blobs;
  struct { int count; jsmntok_t* token; } views;
  struct { int count; jsmntok_t* token; } nodes;
  struct { int count; jsmntok_t* token; } meshes;
  int childCount;
  int primitiveCount;
} gltfInfo;

static int nomString(const char* data, jsmntok_t* token, gltfString* string) {
  lovrAssert(token->type == JSMN_STRING, "Expected string");
  string->data = (char*) data + token->start;
  string->length = token->end - token->start;
  return 1;
}

static int nomValue(const char* data, jsmntok_t* token, int count, int sum) {
  if (count == 0) { return sum; }
  switch (token->type) {
    case JSMN_OBJECT: return nomValue(data, token + 1, count - 1 + 2 * token->size, sum + 1);
    case JSMN_ARRAY: return nomValue(data, token + 1, count - 1 + token->size, sum + 1);
    default: return nomValue(data, token + 1, count - 1, sum + 1);
  }
}

// Kinda like sum(map(arr, obj => #obj[key]))
static jsmntok_t* aggregate(const char* json, jsmntok_t* token, const char* target, int* total) {
  *total = 0;
  int size = (token++)->size;
  for (int i = 0; i < size; i++) {
    if (token->size > 0) {
      int keys = (token++)->size;
      for (int k = 0; k < keys; k++) {
        gltfString key;
        token += nomString(json, token, &key);
        if (KEY_EQ(key, target)) {
          *total += token->size;
        }
        token += nomValue(json, token, 1, 0);
      }
    }
  }
  return token;
}

static void preparse(const char* json, jsmntok_t* tokens, int tokenCount, gltfInfo* info, size_t* dataSize) {
  for (jsmntok_t* token = tokens + 1; token < tokens + tokenCount;) { // +1 to skip root object
    gltfString key;
    token += nomString(json, token, &key);

    if (KEY_EQ(key, "accessors")) {
      info->accessors.token = token;
      info->accessors.count = token->size;
      *dataSize += info->accessors.count * sizeof(ModelAccessor);
      token += nomValue(json, token, 1, 0);
    } else if (KEY_EQ(key, "buffers")) {
      info->blobs.token = token;
      info->blobs.count = token->size;
      *dataSize += info->blobs.count * sizeof(ModelBlob);
      token += nomValue(json, token, 1, 0);
    } else if (KEY_EQ(key, "bufferViews")) {
      info->views.token = token;
      info->views.count = token->size;
      *dataSize += info->views.count * sizeof(ModelView);
      token += nomValue(json, token, 1, 0);
    } else if (KEY_EQ(key, "nodes")) {
      info->nodes.token = token;
      info->nodes.count = token->size;
      *dataSize += info->nodes.count * sizeof(ModelNode);
      token = aggregate(json, token, "children", &info->childCount);
    } else if (KEY_EQ(key, "meshes")) {
      info->meshes.token = token;
      info->meshes.count = token->size;
      *dataSize += info->meshes.count * sizeof(ModelMesh);
      token = aggregate(json, token, "primitives", &info->primitiveCount);
      *dataSize += info->primitiveCount * sizeof(ModelPrimitive);
    } else {
      token += nomValue(json, token, 1, 0); // Skip
    }
  }
}

static void parseAccessors(const char* json, jsmntok_t* token, ModelData* model) {
  if (!token) return;

  int count = (token++)->size;
  for (int i = 0; i < count; i++) {
    ModelAccessor* accessor = &model->accessors[i];
    gltfString key;
    int keyCount = (token++)->size;

    for (int k = 0; k < keyCount; k++) {
      token += nomString(json, token, &key);
      if (KEY_EQ(key, "bufferView")) {
        accessor->view = TOK_INT(json, token), token++;
      } else if (KEY_EQ(key, "count")) {
        accessor->count = TOK_INT(json, token), token++;
      } else if (KEY_EQ(key, "byteOffset")) {
        accessor->offset = TOK_INT(json, token), token++;
      } else if (KEY_EQ(key, "componentType")) {
        switch (TOK_INT(json, token)) {
          case 5120: accessor->type = I8; break;
          case 5121: accessor->type = U8; break;
          case 5122: accessor->type = I16; break;
          case 5123: accessor->type = U16; break;
          case 5125: accessor->type = U32; break;
          case 5126: accessor->type = F32; break;
          default: break;
        }
        token++;
      } else if (KEY_EQ(key, "type")) {
        gltfString type;
        token += nomString(json, token, &type);
        if (KEY_EQ(type, "SCALAR")) {
          accessor->components = 1;
        } else if (type.length == 4 && type.data[0] == 'V') {
          accessor->components = type.data[3] - '0';
        } else if (type.length == 4 && type.data[0] == 'M') {
          lovrThrow("Matrix accessors are not supported");
        } else {
          lovrThrow("Unknown attribute type");
        }
      } else if (KEY_EQ(key, "normalized")) {
        accessor->normalized = TOK_BOOL(json, token), token++;
      } else {
        token += nomValue(json, token, 1, 0); // Skip
      }
    }
  }
}

static void parseBlobs(const char* json, jsmntok_t* token, ModelData* model, ModelDataIO io, void* binData) {
  if (!token) return;

  int count = (token++)->size;
  for (int i = 0; i < count; i++) {
    ModelBlob* blob = &model->blobs[i];
    gltfString key;
    int keyCount = (token++)->size;
    size_t bytesRead = 0;
    bool hasUri = false;

    for (int k = 0; k < keyCount; k++) {
      token += nomString(json, token, &key);
      if (KEY_EQ(key, "byteLength")) {
        blob->size = TOK_INT(json, token), token++;
      } else if (KEY_EQ(key, "uri")) {
        hasUri = true;
        gltfString filename;
        token += nomString(json, token, &filename);
        filename.data[filename.length] = '\0'; // Change the quote into a terminator (I'll be b0k)
        blob->data = io.read(filename.data, &bytesRead);
        lovrAssert(blob->data, "Unable to read %s", filename.data);
      } else {
        token += nomValue(json, token, 1, 0); // Skip
      }
    }

    if (hasUri) {
      lovrAssert(bytesRead == blob->size, "Couldn't read all of buffer data");
    } else {
      lovrAssert(binData && i == 0, "Buffer is missing URI");
      blob->data = binData;
    }
  }
}

static void parseViews(const char* json, jsmntok_t* token, ModelData* model) {
  if (!token) return;

  int count = (token++)->size;
  for (int i = 0; i < count; i++) {
    ModelView* view = &model->views[i];
    gltfString key;
    int keyCount = (token++)->size;

    for (int k = 0; k < keyCount; k++) {
      token += nomString(json, token, &key);
      if (KEY_EQ(key, "buffer")) {
        view->blob = TOK_INT(json, token), token++;
      } else if (KEY_EQ(key, "byteOffset")) {
        view->offset = TOK_INT(json, token), token++;
      } else if (KEY_EQ(key, "byteLength")) {
        view->length = TOK_INT(json, token), token++;
      } else if (KEY_EQ(key, "byteStride")) {
        view->stride = TOK_INT(json, token), token++;
    } else {
        token += nomValue(json, token, 1, 0); // Skip
      }
    }
  }
}

static void parseNodes(const char* json, jsmntok_t* token, ModelData* model) {
  if (!token) return;

  int childIndex = 0;
  int count = (token++)->size; // Enter array
  for (int i = 0; i < count; i++) {
    ModelNode* node = &model->nodes[i];
    float translation[3] = { 0, 0, 0 };
    float rotation[4] = { 0, 0, 0, 0 };
    float scale[3] = { 1, 1, 1 };
    bool matrix = false;

    gltfString key;
    int keyCount = (token++)->size; // Enter object
    for (int k = 0; k < keyCount; k++) {
      token += nomString(json, token, &key);

      if (KEY_EQ(key, "children")) {
        node->children = &model->childMap[childIndex];
        node->childCount = (token++)->size;
        for (uint32_t j = 0; j < node->childCount; j++) {
          model->childMap[childIndex++] = TOK_INT(json, token), token++;
        }
      } else if (KEY_EQ(key, "mesh")) {
        node->mesh = TOK_INT(json, token), token++;
      } else if (KEY_EQ(key, "matrix")) {
        lovrAssert(token->size == 16, "Node matrix needs 16 elements");
        matrix = true;
        for (int j = 0; j < token->size; j++) {
          node->transform[j] = TOK_FLOAT(json, token), token++;
        }
      } else if (KEY_EQ(key, "translation")) {
        lovrAssert(token->size == 3, "Node translation needs 3 elements");
        translation[0] = TOK_FLOAT(json, token), token++;
        translation[1] = TOK_FLOAT(json, token), token++;
        translation[2] = TOK_FLOAT(json, token), token++;
      } else if (KEY_EQ(key, "rotation")) {
        lovrAssert(token->size == 4, "Node rotation needs 4 elements");
        rotation[0] = TOK_FLOAT(json, token), token++;
        rotation[1] = TOK_FLOAT(json, token), token++;
        rotation[2] = TOK_FLOAT(json, token), token++;
        rotation[3] = TOK_FLOAT(json, token), token++;
      } else if (KEY_EQ(key, "scale")) {
        lovrAssert(token->size == 3, "Node scale needs 3 elements");
        scale[0] = TOK_FLOAT(json, token), token++;
        scale[1] = TOK_FLOAT(json, token), token++;
        scale[2] = TOK_FLOAT(json, token), token++;
      } else {
        token += nomValue(json, token, 1, 0); // Skip
      }
    }

    // Fix it in post
    if (!matrix) {
      mat4_identity(node->transform);
      mat4_translate(node->transform, translation[0], translation[1], translation[2]);
      mat4_rotateQuat(node->transform, rotation);
      mat4_scale(node->transform, scale[0], scale[1], scale[2]);
    }
  }
}

static jsmntok_t* parsePrimitive(const char* json, jsmntok_t* token, int index, ModelData* model) {
  gltfString key;
  ModelPrimitive* primitive = &model->primitives[index];
  int keyCount = (token++)->size; // Enter object
  memset(primitive->attributes, 0xff, sizeof(primitive->attributes));
  primitive->indices = -1;
  primitive->mode = DRAW_TRIANGLES;

  for (int k = 0; k < keyCount; k++) {
    token += nomString(json, token, &key);

    if (KEY_EQ(key, "material")) {
      primitive->material = TOK_INT(json, token), token++;
    } else if (KEY_EQ(key, "indices")) {
      primitive->indices = TOK_INT(json, token), token++;
    } else if (KEY_EQ(key, "mode")) {
      switch (TOK_INT(json, token)) {
        case 0: primitive->mode = DRAW_POINTS; break;
        case 1: primitive->mode = DRAW_LINES; break;
        case 2: primitive->mode = DRAW_LINE_LOOP; break;
        case 3: primitive->mode = DRAW_LINE_STRIP; break;
        case 4: primitive->mode = DRAW_TRIANGLES; break;
        case 5: primitive->mode = DRAW_TRIANGLE_STRIP; break;
        case 6: primitive->mode = DRAW_TRIANGLE_FAN; break;
        default: lovrThrow("Unknown primitive mode");
      }
      token++;
    } else if (KEY_EQ(key, "attributes")) {
      int attributeCount = (token++)->size;
      for (int i = 0; i < attributeCount; i++) {
        gltfString name;
        token += nomString(json, token, &name);
        int accessor = TOK_INT(json, token);
        if (KEY_EQ(name, "POSITION")) {
          primitive->attributes[ATTR_POSITION] = accessor;
        } else if (KEY_EQ(name, "NORMAL")) {
          primitive->attributes[ATTR_NORMAL] = accessor;
        } else if (KEY_EQ(name, "TEXCOORD_0")) {
          primitive->attributes[ATTR_TEXCOORD] = accessor;
        } else if (KEY_EQ(name, "COLOR_0")) {
          primitive->attributes[ATTR_COLOR] = accessor;
        } else if (KEY_EQ(name, "TANGENT")) {
          primitive->attributes[ATTR_TANGENT] = accessor;
        } else if (KEY_EQ(name, "JOINTS_0")) {
          primitive->attributes[ATTR_BONES] = accessor;
        } else if (KEY_EQ(name, "WEIGHTS_0")) {
          primitive->attributes[ATTR_WEIGHTS] = accessor;
        }
        token++;
      }
    } else {
      token += nomValue(json, token, 1, 0); // Skip
    }
  }
  return token;
}

static void parseMeshes(const char* json, jsmntok_t* token, ModelData* model) {
  if (!token) return;

  int primitiveIndex = 0;
  int count = (token++)->size; // Enter array
  for (int i = 0; i < count; i++) {
    gltfString key;
    ModelMesh* mesh = &model->meshes[i];
    int keyCount = (token++)->size; // Enter object
    for (int k = 0; k < keyCount; k++) {
      token += nomString(json, token, &key);

      if (KEY_EQ(key, "primitives")) {
        mesh->primitives = &model->primitives[primitiveIndex];
        mesh->primitiveCount = (token++)->size;
        for (uint32_t j = 0; j < mesh->primitiveCount; j++) {
          token = parsePrimitive(json, token, primitiveIndex++, model);
        }
      } else {
        token += nomValue(json, token, 1, 0); // Skip
      }
    }
  }
}

ModelData* lovrModelDataInit(ModelData* model, Blob* blob, ModelDataIO io) {
  uint8_t* data = blob->data;
  gltfHeader* header = (gltfHeader*) data;
  bool glb = header->magic == MAGIC_glTF;
  const char *jsonData, *binData;
  size_t jsonLength, binLength;

  if (glb) {
    gltfChunkHeader* jsonHeader = (gltfChunkHeader*) &header[1];
    lovrAssert(jsonHeader->type == MAGIC_JSON, "Invalid JSON header");

    jsonData = (char*) &jsonHeader[1];
    jsonLength = jsonHeader->length;

    gltfChunkHeader* binHeader = (gltfChunkHeader*) &jsonData[jsonLength];
    lovrAssert(binHeader->type == MAGIC_BIN, "Invalid BIN header");

    binData = (char*) &binHeader[1];
    binLength = binHeader->length;

    // Hang onto the data since it's already here rather than make a copy of it
    lovrRetain(blob);
  } else {
    jsonData = (char*) data;
    jsonLength = blob->size;
    binData = NULL;
    binLength = 0;
  }

  jsmn_parser parser;
  jsmn_init(&parser);

  jsmntok_t tokens[1024]; // TODO malloc or token queue
  int tokenCount = jsmn_parse(&parser, jsonData, jsonLength, tokens, 1024);
  lovrAssert(tokenCount >= 0, "Invalid JSON");
  lovrAssert(tokens[0].type == JSMN_OBJECT, "No root object");

  gltfInfo info = { 0 };
  size_t dataSize = 0;
  preparse(jsonData, tokens, tokenCount, &info, &dataSize);

  size_t offset = 0;
  model->data = calloc(1, dataSize);
  model->glbBlob = glb ? blob : NULL;
  model->accessorCount = info.accessors.count;
  model->blobCount = info.blobs.count;
  model->viewCount = info.views.count;
  model->meshCount = info.meshes.count;
  model->nodeCount = info.nodes.count;
  model->primitiveCount = info.primitiveCount;
  model->accessors = (ModelAccessor*) (model->data + offset), offset += info.accessors.count * sizeof(ModelAccessor);
  model->blobs = (ModelBlob*) (model->data + offset), offset += info.blobs.count * sizeof(ModelBlob);
  model->views = (ModelView*) (model->data + offset), offset += info.views.count * sizeof(ModelView);
  model->meshes = (ModelMesh*) (model->data + offset), offset += info.meshes.count * sizeof(ModelMesh);
  model->nodes = (ModelNode*) (model->data + offset), offset += info.nodes.count * sizeof(ModelNode);
  model->primitives = (ModelPrimitive*) (model->data + offset), offset += info.primitiveCount * sizeof(ModelPrimitive);
  model->childMap = (uint32_t*) (model->data + offset), offset += info.childCount * sizeof(uint32_t);

  parseAccessors(jsonData, info.accessors.token, model);
  parseBlobs(jsonData, info.blobs.token, model, io, (void*) binData);
  parseViews(jsonData, info.views.token, model);
  parseNodes(jsonData, info.nodes.token, model);
  parseMeshes(jsonData, info.meshes.token, model);

  return model;
}

void lovrModelDataDestroy(void* ref) {
  //
}
