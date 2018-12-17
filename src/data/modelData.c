#include "data/modelData.h"
#include "lib/math.h"
#include "lib/jsmn/jsmn.h"
#include <stdbool.h>

#define MAGIC_glTF 0x46546c67
#define MAGIC_JSON 0x4e4f534a
#define MAGIC_BIN 0x004e4942

#define KEY_EQ(k, s) !strncmp(k.data, s, k.length)
#define NOM_KEY(j, t) hashKey((char*) j + (t++)->start)
#define NOM_INT(j, t) strtol(j + (t++)->start, NULL, 10)
#define NOM_BOOL(j, t) (*(j + (t++)->start) == 't')
#define NOM_FLOAT(j, t) strtof(j + (t++)->start, NULL)

typedef struct {
  size_t totalSize;
  jsmntok_t* accessors;
  jsmntok_t* animations;
  jsmntok_t* blobs;
  jsmntok_t* views;
  jsmntok_t* images;
  jsmntok_t* nodes;
  jsmntok_t* meshes;
  jsmntok_t* skins;
  int childCount;
  int jointCount;
} gltfInfo;

static uint32_t hashKey(char* key) {
  uint32_t hash = 0;
  for (int i = 0; key[i] != '"'; i++) {
    hash = (hash * 65599) + key[i];
  }
  return hash ^ (hash >> 16);
}

static int nomValue(const char* data, jsmntok_t* token, int count, int sum) {
  if (count == 0) { return sum; }
  switch (token->type) {
    case JSMN_OBJECT: return nomValue(data, token + 1, count - 1 + 2 * token->size, sum + 1);
    case JSMN_ARRAY: return nomValue(data, token + 1, count - 1 + token->size, sum + 1);
    default: return nomValue(data, token + 1, count - 1, sum + 1);
  }
}

// Kinda like total += sum(map(arr, obj => #obj[key]))
static jsmntok_t* aggregate(const char* json, jsmntok_t* token, uint32_t hash, int* total) {
  int size = (token++)->size;
  for (int i = 0; i < size; i++) {
    if (token->size > 0) {
      int keys = (token++)->size;
      for (int k = 0; k < keys; k++) {
        if (NOM_KEY(json, token) == hash) {
          *total += token->size;
        }
        token += nomValue(json, token, 1, 0);
      }
    }
  }
  return token;
}

static void preparse(const char* json, jsmntok_t* tokens, int tokenCount, ModelData* model, gltfInfo* info) {
  for (jsmntok_t* token = tokens + 1; token < tokens + tokenCount;) { // +1 to skip root object
    switch (NOM_KEY(json, token)) {
      case HASH16("accessors"):
        info->accessors = token;
        model->accessorCount = token->size;
        info->totalSize += token->size * sizeof(ModelAccessor);
        token += nomValue(json, token, 1, 0);
        break;
      case HASH16("animations"):
        info->animations = token;
        model->animationCount = token->size;
        info->totalSize += token->size * sizeof(ModelAnimation);
        // Almost like aggregate, but we gotta aggregate 2 keys in a single pass
        int size = (token++)->size;
        for (int i = 0; i < size; i++) {
          if (token->size > 0) {
            int keyCount = (token++)->size;
            for (int k = 0; k < keyCount; k++) {
              switch (NOM_KEY(json, token)) {
                case HASH16("channels"): model->animationChannelCount += token->size; break;
                case HASH16("samplers"): model->animationSamplerCount += token->size; break;
                default: break;
              }
              token += nomValue(json, token, 1, 0);
            }
          }
        }
        break;
      case HASH16("buffers"):
        info->blobs = token;
        model->blobCount = token->size;
        info->totalSize += token->size * sizeof(ModelBlob);
        token += nomValue(json, token, 1, 0);
        break;
      case HASH16("bufferViews"):
        info->views = token;
        model->viewCount = token->size;
        info->totalSize += token->size * sizeof(ModelView);
        token += nomValue(json, token, 1, 0);
        break;
      case HASH16("images"):
        info->images = token;
        model->imageCount = token->size;
        info->totalSize += token->size * sizeof(TextureData);
        token += nomValue(json, token, 1, 0);
        break;
      case HASH16("meshes"):
        info->meshes = token;
        model->meshCount = token->size;
        info->totalSize += token->size * sizeof(ModelMesh);
        token = aggregate(json, token, HASH16("primitives"), &model->primitiveCount);
        info->totalSize += model->primitiveCount * sizeof(ModelPrimitive);
        break;
      case HASH16("nodes"):
        info->nodes = token;
        model->nodeCount = token->size;
        info->totalSize += token->size * sizeof(ModelNode);
        token = aggregate(json, token, HASH16("children"), &info->childCount);
        info->totalSize += info->childCount * sizeof(uint32_t);
        break;
      case HASH16("skins"):
        info->skins = token;
        model->skinCount = token->size;
        info->totalSize += token->size * sizeof(ModelSkin);
        token = aggregate(json, token, HASH16("joints"), &info->jointCount);
        info->totalSize += info->jointCount * sizeof(uint32_t);
        break;
      default: token += nomValue(json, token, 1, 0); break;
    }
  }
}

static void parseAccessors(const char* json, jsmntok_t* token, ModelData* model) {
  if (!token) return;

  int count = (token++)->size;
  for (int i = 0; i < count; i++) {
    ModelAccessor* accessor = &model->accessors[i];

    int keyCount = (token++)->size;
    for (int k = 0; k < keyCount; k++) {
      switch (NOM_KEY(json, token)) {
        case HASH16("bufferView"): accessor->view = NOM_INT(json, token); break;
        case HASH16("count"): accessor->count = NOM_INT(json, token); break;
        case HASH16("byteOffset"): accessor->offset = NOM_INT(json, token); break;
        case HASH16("normalized"): accessor->normalized = NOM_BOOL(json, token); break;
        case HASH16("componentType"):
          switch (NOM_INT(json, token)) {
            case 5120: accessor->type = I8; break;
            case 5121: accessor->type = U8; break;
            case 5122: accessor->type = I16; break;
            case 5123: accessor->type = U16; break;
            case 5125: accessor->type = U32; break;
            case 5126: accessor->type = F32; break;
            default: break;
          }
          break;
        case HASH16("type"):
          switch (NOM_KEY(json, token)) {
            case HASH16("SCALAR"): accessor->components += 1; break;
            case HASH16("VEC2"): accessor->components += 2; break;
            case HASH16("VEC3"): accessor->components += 3; break;
            case HASH16("VEC4"): accessor->components += 4; break;
            default: lovrThrow("Unsupported accessor type"); break;
          }
          break;
        default: token += nomValue(json, token, 1, 0); break;
      }
    }
  }
}

static jsmntok_t* parseAnimationChannel(const char* json, jsmntok_t* token, int index, ModelData* model) {
  ModelAnimationChannel* channel = &model->animationChannels[index];
  int keyCount = (token++)->size;
  for (int k = 0; k < keyCount; k++) {
    switch (NOM_KEY(json, token)) {
      case HASH16("sampler"): channel->sampler = NOM_INT(json, token); break;
      case HASH16("target"): {
        int targetKeyCount = (token++)->size;
        for (int tk = 0; tk < targetKeyCount; tk++) {
          switch (NOM_KEY(json, token)) {
            case HASH16("node"): channel->nodeIndex = NOM_INT(json, token); break;
            case HASH16("path"):
              switch (NOM_KEY(json, token)) {
                case HASH16("translation"): channel->property = PROP_TRANSLATION; break;
                case HASH16("rotation"): channel->property = PROP_ROTATION; break;
                case HASH16("scale"): channel->property = PROP_SCALE; break;
                default: lovrThrow("Unknown animation target path"); break;
              }
              break;
            default: token += nomValue(json, token, 1, 0); break;
          }
        }
      }
      default: token += nomValue(json, token, 1, 0); break;
    }
  }
  return token;
}

static jsmntok_t* parseAnimationSampler(const char* json, jsmntok_t* token, int index, ModelData* model) {
  ModelAnimationSampler* sampler = &model->animationSamplers[index];
  int keyCount = (token++)->size;
  for (int k = 0; k < keyCount; k++) {
    switch (NOM_KEY(json, token)) {
      case HASH16("input"): sampler->times = NOM_INT(json, token); break;
      case HASH16("output"): sampler->values = NOM_INT(json, token); break;
      case HASH16("interpolation"):
        switch (NOM_KEY(json, token)) {
          case HASH16("LINEAR"): sampler->smoothing = SMOOTH_LINEAR; break;
          case HASH16("STEP"): sampler->smoothing = SMOOTH_STEP; break;
          case HASH16("CUBICSPLINE"): sampler->smoothing = SMOOTH_CUBIC; break;
          default: lovrThrow("Unknown animation sampler interpolation"); break;
        }
        break;
      default: token += nomValue(json, token, 1, 0);
    }
  }
  return token;
}

static void parseAnimations(const char* json, jsmntok_t* token, ModelData* model) {
  if (!token) return;

  int channelIndex = 0;
  int samplerIndex = 0;
  int count = (token++)->size;
  for (int i = 0; i < count; i++) {
    ModelAnimation* animation = &model->animations[i];

    int keyCount = (token++)->size;
    for (int k = 0; k < keyCount; k++) {
      switch (NOM_KEY(json, token)) {
        case HASH16("channels"):
          animation->channelCount = token->size;
          animation->channels = &model->animationChannels[channelIndex];
          for (int j = 0; j < animation->channelCount; j++) {
            token = parseAnimationChannel(json, token, channelIndex++, model);
          }
          break;
        case HASH16("samplers"):
          animation->samplerCount = token->size;
          animation->samplers = &model->animationSamplers[samplerIndex];
          for (int j = 0; j < animation->samplerCount; j++) {
            token = parseAnimationSampler(json, token, samplerIndex++, model);
          }
          break;
        default: token += nomValue(json, token, 1, 0); break;
      }
    }
  }
}

static void parseBlobs(const char* json, jsmntok_t* token, ModelData* model, ModelDataIO io, void* binData) {
  if (!token) return;

  int count = (token++)->size;
  for (int i = 0; i < count; i++) {
    ModelBlob* blob = &model->blobs[i];
    size_t bytesRead = 0;
    bool hasUri = false;

    int keyCount = (token++)->size;
    for (int k = 0; k < keyCount; k++) {
      switch (NOM_KEY(json, token)) {
        case HASH16("byteLength"): blob->size = NOM_INT(json, token); break;
        case HASH16("uri"):
          hasUri = true;
          char* filename = (char*) json + token->start;
          size_t length = token->end - token->start;
          filename[length] = '\0'; // Change the quote into a terminator (I'll be b0k)
          blob->data = io.read(filename, &bytesRead);
          lovrAssert(blob->data, "Unable to read %s", filename);
          filename[length] = '"';
          break;
        default: token += nomValue(json, token, 1, 0); break;
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

    int keyCount = (token++)->size;
    for (int k = 0; k < keyCount; k++) {
      switch (NOM_KEY(json, token)) {
        case HASH16("buffer"): view->blob = NOM_INT(json, token); break;
        case HASH16("byteOffset"): view->offset = NOM_INT(json, token); break;
        case HASH16("byteLength"): view->length = NOM_INT(json, token); break;
        case HASH16("byteStride"): view->stride = NOM_INT(json, token); break;
        default: token += nomValue(json, token, 1, 0); break;
      }
    }
  }
}

static void parseImages(const char* json, jsmntok_t* token, ModelData* model, ModelDataIO io) {
  if (!token) return;

  int count = (token++)->size;
  for (int i = 0; i < count; i++) {
    TextureData** image = &model->images[i];
    int keyCount = (token++)->size;
    for (int k = 0; k < keyCount; k++) {
      switch (NOM_KEY(json, token)) {
        case HASH16("bufferView"): {
          int viewIndex = NOM_INT(json, token);
          ModelView* view = &model->views[viewIndex];
          void* data = (uint8_t*) model->blobs[view->blob].data + view->offset;
          Blob* blob = lovrBlobCreate(data, view->length, NULL);
          *image = lovrTextureDataCreateFromBlob(blob, true);
          blob->data = NULL; // FIXME
          lovrRelease(blob);
          break;
        }
        case HASH16("uri"): {
          size_t size = 0;
          char* uri = (char*) json + token->start;
          size_t length = token->end - token->start;
          uri[length] = '\0'; // Change the quote into a terminator (I'll be b0k)
          void* data = io.read(uri, &size);
          lovrAssert(data && size > 0, "Unable to read image at '%s'", uri);
          uri[length] = '"';
          Blob* blob = lovrBlobCreate(data, size, NULL);
          *image = lovrTextureDataCreateFromBlob(blob, true);
          lovrRelease(blob);
          break;
        }
        default: token += nomValue(json, token, 1, 0); break;
      }
    }
  }
}

static jsmntok_t* parsePrimitive(const char* json, jsmntok_t* token, int index, ModelData* model) {
  ModelPrimitive* primitive = &model->primitives[index];
  memset(primitive->attributes, 0xff, sizeof(primitive->attributes));
  primitive->indices = -1;
  primitive->mode = DRAW_TRIANGLES;

  int keyCount = (token++)->size; // Enter object
  for (int k = 0; k < keyCount; k++) {
    switch (NOM_KEY(json, token)) {
      case HASH16("material"): primitive->material = NOM_INT(json, token); break;
      case HASH16("indices"): primitive->indices = NOM_INT(json, token); break;
      case HASH16("mode"):
        switch (NOM_INT(json, token)) {
          case 0: primitive->mode = DRAW_POINTS; break;
          case 1: primitive->mode = DRAW_LINES; break;
          case 2: primitive->mode = DRAW_LINE_LOOP; break;
          case 3: primitive->mode = DRAW_LINE_STRIP; break;
          case 4: primitive->mode = DRAW_TRIANGLES; break;
          case 5: primitive->mode = DRAW_TRIANGLE_STRIP; break;
          case 6: primitive->mode = DRAW_TRIANGLE_FAN; break;
          default: lovrThrow("Unknown primitive mode");
        }
        break;
      case HASH16("attributes"): {
        int attributeCount = (token++)->size;
        for (int i = 0; i < attributeCount; i++) {
          switch (NOM_KEY(json, token)) {
            case HASH16("POSITION"): primitive->attributes[ATTR_POSITION] = NOM_INT(json, token); break;
            case HASH16("NORMAL"): primitive->attributes[ATTR_NORMAL] = NOM_INT(json, token); break;
            case HASH16("TEXCOORD_0"): primitive->attributes[ATTR_TEXCOORD] = NOM_INT(json, token); break;
            case HASH16("COLOR_0"): primitive->attributes[ATTR_COLOR] = NOM_INT(json, token); break;
            case HASH16("TANGENT"): primitive->attributes[ATTR_TANGENT] = NOM_INT(json, token); break;
            case HASH16("JOINTS_0"): primitive->attributes[ATTR_BONES] = NOM_INT(json, token); break;
            case HASH16("WEIGHTS_0"): primitive->attributes[ATTR_WEIGHTS] = NOM_INT(json, token); break;
            default: break;
          }
        }
        break;
      }
      default: token += nomValue(json, token, 1, 0); break;
    }
  }
  return token;
}

static void parseMeshes(const char* json, jsmntok_t* token, ModelData* model) {
  if (!token) return;

  int primitiveIndex = 0;
  int count = (token++)->size; // Enter array
  for (int i = 0; i < count; i++) {
    ModelMesh* mesh = &model->meshes[i];

    int keyCount = (token++)->size; // Enter object
    for (int k = 0; k < keyCount; k++) {
      switch (NOM_KEY(json, token)) {
        case HASH16("primitives"):
          mesh->primitives = &model->primitives[primitiveIndex];
          mesh->primitiveCount = (token++)->size;
          for (uint32_t j = 0; j < mesh->primitiveCount; j++) {
            token = parsePrimitive(json, token, primitiveIndex++, model);
          }
          break;
        default: token += nomValue(json, token, 1, 0); break;
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

    int keyCount = (token++)->size; // Enter object
    for (int k = 0; k < keyCount; k++) {
      switch (NOM_KEY(json, token)) {
        case HASH16("mesh"): node->mesh = NOM_INT(json, token); break;
        case HASH16("skin"): node->skin = NOM_INT(json, token); break;
        case HASH16("children"):
          node->children = &model->nodeChildren[childIndex];
          node->childCount = (token++)->size;
          for (uint32_t j = 0; j < node->childCount; j++) {
            model->nodeChildren[childIndex++] = NOM_INT(json, token);
          }
          break;
        case HASH16("matrix"):
          lovrAssert(token->size == 16, "Node matrix needs 16 elements");
          matrix = true;
          for (int j = 0; j < token->size; j++) {
            node->transform[j] = NOM_FLOAT(json, token);
          }
          break;
        case HASH16("translation"):
          lovrAssert(token->size == 3, "Node translation needs 3 elements");
          translation[0] = NOM_FLOAT(json, token);
          translation[1] = NOM_FLOAT(json, token);
          translation[2] = NOM_FLOAT(json, token);
          break;
        case HASH16("rotation"):
          lovrAssert(token->size == 4, "Node rotation needs 4 elements");
          rotation[0] = NOM_FLOAT(json, token);
          rotation[1] = NOM_FLOAT(json, token);
          rotation[2] = NOM_FLOAT(json, token);
          rotation[3] = NOM_FLOAT(json, token);
          break;
        case HASH16("scale"):
          lovrAssert(token->size == 3, "Node scale needs 3 elements");
          scale[0] = NOM_FLOAT(json, token);
          scale[1] = NOM_FLOAT(json, token);
          scale[2] = NOM_FLOAT(json, token);
          break;
        default: token += nomValue(json, token, 1, 0); break;
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

static void parseSkins(const char* json, jsmntok_t* token, ModelData* model) {
  if (!token) return;

  int jointIndex = 0;
  int count = (token++)->size; // Enter array
  for (int i = 0; i < count; i++) {
    ModelSkin* skin = &model->skins[i];

    int keyCount = (token++)->size;
    for (int k = 0; k < keyCount; k++) {
      switch (NOM_KEY(json, token)) {
        case HASH64("inverseBindMatrices"): skin->inverseBindMatrices = NOM_INT(json, token); break;
        case HASH16("skeleton"): skin->skeleton = NOM_INT(json, token); break;
        case HASH16("joints"):
          skin->joints = &model->skinJoints[jointIndex];
          skin->jointCount = (token++)->size;
          for (uint32_t j = 0; j < skin->jointCount; j++) {
            model->skinJoints[jointIndex++] = NOM_INT(json, token);
          }
          break;
        default: token += nomValue(json, token, 1, 0); break;
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
  preparse(jsonData, tokens, tokenCount, model, &info);

  size_t offset = 0;
  model->data = calloc(1, info.totalSize);
  model->binaryBlob = glb ? blob : NULL;
  model->accessors = (ModelAccessor*) (model->data + offset), offset += model->accessorCount * sizeof(ModelAccessor);
  model->animationChannels = (ModelAnimationChannel*) (model->data + offset), offset += model->animationChannelCount * sizeof(ModelAnimationChannel);
  model->animationSamplers = (ModelAnimationSampler*) (model->data + offset), offset += model->animationSamplerCount * sizeof(ModelAnimationSampler);
  model->animations = (ModelAnimation*) (model->data + offset), offset += model->animationCount * sizeof(ModelAnimation);
  model->blobs = (ModelBlob*) (model->data + offset), offset += model->blobCount * sizeof(ModelBlob);
  model->views = (ModelView*) (model->data + offset), offset += model->viewCount * sizeof(ModelView);
  model->images = (TextureData**) (model->data + offset), offset += model->imageCount * sizeof(TextureData*);
  model->primitives = (ModelPrimitive*) (model->data + offset), offset += model->primitiveCount * sizeof(ModelPrimitive);
  model->meshes = (ModelMesh*) (model->data + offset), offset += model->meshCount * sizeof(ModelMesh);
  model->nodes = (ModelNode*) (model->data + offset), offset += model->nodeCount * sizeof(ModelNode);
  model->skins = (ModelSkin*) (model->data + offset), offset += model->skinCount * sizeof(ModelSkin);
  model->nodeChildren = (uint32_t*) (model->data + offset), offset += info.childCount * sizeof(uint32_t);
  model->skinJoints = (uint32_t*) (model->data + offset), offset += info.jointCount * sizeof(uint32_t);

  parseAccessors(jsonData, info.accessors, model);
  parseAnimations(jsonData, info.animations, model);
  parseBlobs(jsonData, info.blobs, model, io, (void*) binData);
  parseViews(jsonData, info.views, model);
  parseImages(jsonData, info.images, model, io);
  parseMeshes(jsonData, info.meshes, model);
  parseNodes(jsonData, info.nodes, model);
  parseSkins(jsonData, info.skins, model);

  return model;
}

void lovrModelDataDestroy(void* ref) {
  //
}
