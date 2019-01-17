#include "data/modelData.h"
#include "lib/math.h"
#include "lib/jsmn/jsmn.h"
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#define MAGIC_glTF 0x46546c67
#define MAGIC_JSON 0x4e4f534a
#define MAGIC_BIN 0x004e4942

#define STR_EQ(k, s) !strncmp(k.data, s, k.length)
#define NOM_VALUE(j, t) nomValue(j, t, 1, 0)
#define NOM_INT(j, t) strtol(j + (t++)->start, NULL, 10)
#define NOM_STR(j, t) (gltfString) { (char* )j + t->start, t->end - t->start }; t++
#define NOM_BOOL(j, t) (*(j + (t++)->start) == 't')
#define NOM_FLOAT(j, t) strtof(j + (t++)->start, NULL)

typedef struct {
  char* data;
  size_t length;
} gltfString;

typedef struct {
  uint32_t magic;
  uint32_t version;
  uint32_t length;
} gltfHeader;

typedef struct {
  uint32_t length;
  uint32_t type;
} gltfChunkHeader;

typedef struct {
  int input;
  int output;
  SmoothMode smoothing;
} gltfAnimationSampler;

typedef struct {
  uint32_t primitiveIndex;
  uint32_t primitiveCount;
} gltfMesh;

typedef struct {
  TextureFilter filter;
  TextureWrap wrap;
  bool mipmaps;
} gltfSampler;

typedef struct {
  uint32_t node;
  uint32_t nodeCount;
} gltfScene;

static int nomValue(const char* data, jsmntok_t* token, int count, int sum) {
  if (count == 0) { return sum; }
  switch (token->type) {
    case JSMN_OBJECT: return nomValue(data, token + 1, count - 1 + 2 * token->size, sum + 1);
    case JSMN_ARRAY: return nomValue(data, token + 1, count - 1 + token->size, sum + 1);
    default: return nomValue(data, token + 1, count - 1, sum + 1);
  }
}

// Kinda like total += sum(map(arr, obj => #obj[key]))
static jsmntok_t* aggregate(const char* json, jsmntok_t* token, const char* target, int* total) {
  for (int i = (token++)->size; i > 0; i--) {
    if (token->size > 0) {
      for (int k = (token++)->size; k > 0; k--) {
        gltfString key = NOM_STR(json, token);
        if (STR_EQ(key, target)) {
          *total += token->size;
        }
        token += NOM_VALUE(json, token);
      }
    }
  }
  return token;
}

static jsmntok_t* parseAnimationSamplers(const char* json, jsmntok_t* token, gltfAnimationSampler* sampler) {
  for (int i = (token++)->size; i > 0; i--) {
    for (int k = (token++)->size; k > 0; k--) {
      gltfString key = NOM_STR(json, token);
      if (STR_EQ(key, "samplers")) {
        for (int j = (token++)->size; j > 0; j--, sampler++) {
          for (int kk = (token++)->size; kk > 0; kk--) {
            gltfString key = NOM_STR(json, token);
            if (STR_EQ(key, "input")) { sampler->input = NOM_INT(json, token); }
            else if (STR_EQ(key, "output")) { sampler->output = NOM_INT(json, token); }
            else if (STR_EQ(key, "interpolation")) {
              gltfString smoothing = NOM_STR(json, token);
              if (STR_EQ(smoothing, "LINEAR")) { sampler->smoothing = SMOOTH_LINEAR; }
              else if (STR_EQ(smoothing, "STEP")) { sampler->smoothing = SMOOTH_STEP; }
              else if (STR_EQ(smoothing, "CUBICSPLINE")) { sampler->smoothing = SMOOTH_CUBIC; }
              else { lovrThrow("Unknown animation sampler interpolation"); }
            } else {
              token += NOM_VALUE(json, token);
            }
          }
        }
      } else {
        token += NOM_VALUE(json, token);
      }
    }
  }
  return token;
}

static jsmntok_t* parseSamplers(const char* json, jsmntok_t* token, gltfSampler* sampler) {
  for (int i = (token++)->size; i > 0; i--, sampler++) {
    sampler->wrap.s = sampler->wrap.t = sampler->wrap.r = WRAP_REPEAT;
    int min = -1;
    int mag = -1;

    for (int k = (token++)->size; k > 0; k--) {
      gltfString key = NOM_STR(json, token);
      if (STR_EQ(key, "minFilter")) { min = NOM_INT(json, token); }
      else if (STR_EQ(key, "magFilter")) { mag = NOM_INT(json, token); }
      else if (STR_EQ(key, "wrapS")) {
        switch (NOM_INT(json, token)) {
          case 33071: sampler->wrap.s = WRAP_CLAMP; break;
          case 33648: sampler->wrap.s = WRAP_MIRRORED_REPEAT; break;
          case 10497: sampler->wrap.s = WRAP_REPEAT; break;
          default: lovrThrow("Unknown sampler wrapS mode for sampler %d", i);
        }
      } else if (STR_EQ(key, "wrapT")) {
        switch (NOM_INT(json, token)) {
          case 33071: sampler->wrap.t = WRAP_CLAMP; break;
          case 33648: sampler->wrap.t = WRAP_MIRRORED_REPEAT; break;
          case 10497: sampler->wrap.t = WRAP_REPEAT; break;
          default: lovrThrow("Unknown sampler wrapT mode for sampler %d", i);
        }
      } else {
        token += NOM_VALUE(json, token);
      }
    }

    if (min == 9728 || min == 9984 || min == 9986 || mag == 9728) {
      sampler->filter.mode = FILTER_NEAREST;
    } else {
      switch (min) {
        case 9729: sampler->filter.mode = FILTER_BILINEAR, sampler->mipmaps = false; break;
        case 9985: sampler->filter.mode = FILTER_BILINEAR, sampler->mipmaps = true; break;
        case 9987: sampler->filter.mode = FILTER_TRILINEAR, sampler->mipmaps = true; break;
      }
    }
  }
  return token;
}

static jsmntok_t* parseTextureInfo(const char* json, jsmntok_t* token, int* dest) {
  for (int k = (token++)->size; k > 0; k--) {
    gltfString key = NOM_STR(json, token);
    if (STR_EQ(key, "index")) { *dest = NOM_INT(json, token); }
    else if (STR_EQ(key, "texCoord")) {
      lovrAssert(NOM_INT(json, token) == 0, "Only one set of texture coordinates is supported");
    } else {
      token += NOM_VALUE(json, token);
    }
  }
  return token;
}

ModelData* lovrModelDataInit(ModelData* model, Blob* source, ModelDataIO io) {
  uint8_t* data = source->data;
  gltfHeader* header = (gltfHeader*) data;
  bool glb = header->magic == MAGIC_glTF;
  const char *json, *binData;
  size_t jsonLength, binLength;
  ptrdiff_t binOffset;

  char basePath[1024];
  strncpy(basePath, source->name, 1023);
  char* slash = strrchr(basePath, '/');
  if (slash) *slash = 0;

  if (glb) {
    gltfChunkHeader* jsonHeader = (gltfChunkHeader*) &header[1];
    lovrAssert(jsonHeader->type == MAGIC_JSON, "Invalid JSON header");

    json = (char*) &jsonHeader[1];
    jsonLength = jsonHeader->length;

    gltfChunkHeader* binHeader = (gltfChunkHeader*) &json[jsonLength];
    lovrAssert(binHeader->type == MAGIC_BIN, "Invalid BIN header");

    binData = (char*) &binHeader[1];
    binLength = binHeader->length;
    binOffset = (char*) binData - (char*) source->data;
  } else {
    json = (char*) data;
    jsonLength = source->size;
    binData = NULL;
    binLength = 0;
    binOffset = 0;
  }

  jsmn_parser parser;
  jsmn_init(&parser);
  int tokenCount = jsmn_parse(&parser, json, jsonLength, NULL, 0);
  jsmntok_t* tokens = malloc(tokenCount * sizeof(jsmntok_t));
  jsmn_init(&parser);
  tokenCount = jsmn_parse(&parser, json, jsonLength, tokens, tokenCount);
  lovrAssert(tokenCount >= 0, "Invalid JSON");
  lovrAssert(tokens[0].type == JSMN_OBJECT, "No root object");

  // Preparse
  struct {
    size_t totalSize;
    jsmntok_t* animations;
    jsmntok_t* attributes;
    jsmntok_t* buffers;
    jsmntok_t* bufferViews;
    jsmntok_t* images;
    jsmntok_t* textures;
    jsmntok_t* materials;
    jsmntok_t* meshes;
    jsmntok_t* nodes;
    jsmntok_t* scenes;
    jsmntok_t* skins;
    int animationChannelCount;
    int childCount;
    int jointCount;
    int charCount;
  } info = { 0 };

  gltfAnimationSampler* animationSamplers = NULL;
  gltfMesh* meshes = NULL;
  gltfSampler* samplers = NULL;
  gltfScene* scenes = NULL;
  int rootScene = -1;

  for (jsmntok_t* token = tokens + 1; token < tokens + tokenCount;) {
    gltfString key = NOM_STR(json, token);

    if (STR_EQ(key, "accessors")) {
      info.attributes = token;
      model->attributeCount = token->size;
      info.totalSize += token->size * sizeof(ModelAttribute);
      token += NOM_VALUE(json, token);

    } else if (STR_EQ(key, "animations")){
      info.animations = token;
      model->animationCount = token->size;
      info.totalSize += token->size * sizeof(ModelAnimation);
      size_t samplerCount = 0;
      jsmntok_t* t = token;
      for (int i = (t++)->size; i > 0; i--) {
        if (t->size > 0) {
          for (int k = (t++)->size; k > 0; k--) {
            gltfString key = NOM_STR(json, t);
            if (STR_EQ(key, "channels")) { info.animationChannelCount += t->size; }
            else if (STR_EQ(key, "samplers")) { samplerCount += t->size; }
            else if (STR_EQ(key, "name")) {
              info.charCount += token->end - token->start + 1;
              info.totalSize += token->end - token->start + 1;
            }
            t += NOM_VALUE(json, t);
          }
        }
      }
      info.totalSize += info.animationChannelCount * sizeof(ModelAnimationChannel);
      animationSamplers = malloc(samplerCount * sizeof(gltfAnimationSampler));
      token = parseAnimationSamplers(json, token, animationSamplers);

    } else if (STR_EQ(key, "buffers")) {
      info.buffers = token;
      model->blobCount = token->size;
      info.totalSize += token->size * sizeof(Blob*);
      token += NOM_VALUE(json, token);

    } else if (STR_EQ(key, "bufferViews")) {
      info.bufferViews = token;
      model->bufferCount = token->size;
      info.totalSize += token->size * sizeof(ModelBuffer);
      token += NOM_VALUE(json, token);

    } else if (STR_EQ(key, "images")) {
      info.images = token;
      model->imageCount = token->size;
      info.totalSize += token->size * sizeof(TextureData*);
      token += NOM_VALUE(json, token);

    } else if (STR_EQ(key, "samplers")) {
      samplers = malloc(token->size * sizeof(gltfSampler));
      token = parseSamplers(json, token, samplers);

    } else if (STR_EQ(key, "textures")) {
      info.textures = token;
      model->textureCount = token->size;
      info.totalSize += token->size * sizeof(ModelTexture);
      token += NOM_VALUE(json, token);

    } else if (STR_EQ(key, "materials")) {
      info.materials = token;
      model->materialCount = token->size;
      info.totalSize += token->size * sizeof(ModelMaterial);
      token += NOM_VALUE(json, token);

    } else if (STR_EQ(key, "meshes")) {
      info.meshes = token;
      meshes = malloc(token->size * sizeof(gltfMesh));
      gltfMesh* mesh = meshes;
      model->primitiveCount = 0;
      for (int i = (token++)->size; i > 0; i--, mesh++) {
        mesh->primitiveCount = 0;
        for (int k = (token++)->size; k > 0; k--) {
          gltfString key = NOM_STR(json, token);
          if (STR_EQ(key, "primitives")) {
            mesh->primitiveIndex = model->primitiveCount;
            mesh->primitiveCount += token->size;
            model->primitiveCount += token->size;
          }
          token += NOM_VALUE(json, token);
        }
      }
      info.totalSize += model->primitiveCount * sizeof(ModelPrimitive);

    } else if (STR_EQ(key, "nodes")) {
      info.nodes = token;
      model->nodeCount += token->size;
      info.totalSize += token->size * sizeof(ModelNode);
      token = aggregate(json, token, "children", &info.childCount);
      info.totalSize += info.childCount * sizeof(uint32_t);

    } else if (STR_EQ(key, "scene")) {
      rootScene = NOM_INT(json, token);

    } else if (STR_EQ(key, "scenes")) {
      info.scenes = token;
      scenes = malloc(token->size * sizeof(gltfScene));
      gltfScene* scene = scenes;
      for (int i = (token++)->size; i > 0; i--, scene++) {
        for (int k = (token++)->size; k > 0; k--) {
          gltfString key = NOM_STR(json, token);
          if (STR_EQ(key, "nodes")) {
            scene->nodeCount = token->size;
          }
          token += NOM_VALUE(json, token);
        }
      }

    } else if (STR_EQ(key, "skins")) {
      info.skins = token;
      model->skinCount = token->size;
      info.totalSize += token->size * sizeof(ModelSkin);
      token = aggregate(json, token, "joints", &info.jointCount);
      info.totalSize += info.jointCount * sizeof(uint32_t);

    } else {
      token += NOM_VALUE(json, token);
    }
  }

  // Make space for fake root node if the root scene has multiple root nodes
  if (rootScene >= 0 && scenes[rootScene].nodeCount > 1) {
    model->nodeCount++;
    info.childCount += model->nodeCount;
  }

  size_t offset = 0;
  int childIndex = 0;
  model->data = calloc(1, info.totalSize);
  model->animations = (ModelAnimation*) (model->data + offset), offset += model->animationCount * sizeof(ModelAnimation);
  model->attributes = (ModelAttribute*) (model->data + offset), offset += model->attributeCount * sizeof(ModelAttribute);
  model->blobs = (Blob**) (model->data + offset), offset += model->blobCount * sizeof(Blob*);
  model->buffers = (ModelBuffer*) (model->data + offset), offset += model->bufferCount * sizeof(ModelBuffer);
  model->images = (TextureData**) (model->data + offset), offset += model->imageCount * sizeof(TextureData*);
  model->textures = (ModelTexture*) (model->data + offset), offset += model->textureCount * sizeof(ModelTexture);
  model->materials = (ModelMaterial*) (model->data + offset), offset += model->materialCount * sizeof(ModelMaterial);
  model->primitives = (ModelPrimitive*) (model->data + offset), offset += model->primitiveCount * sizeof(ModelPrimitive);
  model->nodes = (ModelNode*) (model->data + offset), offset += model->nodeCount * sizeof(ModelNode);
  model->skins = (ModelSkin*) (model->data + offset), offset += model->skinCount * sizeof(ModelSkin);

  ModelAnimationChannel* channels = (ModelAnimationChannel*) (model->data + offset); offset += info.animationChannelCount * sizeof(ModelAnimationChannel);
  uint32_t* nodeChildren = (uint32_t*) (model->data + offset); offset += info.childCount * sizeof(uint32_t);
  uint32_t* skinJoints = (uint32_t*) (model->data + offset); offset += info.jointCount * sizeof(uint32_t);
  char* chars = (char*) (model->data + offset); offset += info.charCount * sizeof(char);

  // Blobs
  if (info.buffers) {
    jsmntok_t* token = info.buffers;
    Blob** blob = model->blobs;
    for (int i = (token++)->size; i > 0; i--, blob++) {
      gltfString uri = { 0 };
      size_t size = 0;

      for (int k = (token++)->size; k > 0; k--) {
        gltfString key = NOM_STR(json, token);
        if (STR_EQ(key, "byteLength")) { size = NOM_INT(json, token); }
        else if (STR_EQ(key, "uri")) { uri = NOM_STR(json, token); }
        else { token += NOM_VALUE(json, token); }
      }

      if (uri.data) {
        size_t bytesRead;
        char filename[1024];
        lovrAssert(uri.length < 1024, "Buffer filename is too long");
        snprintf(filename, 1023, "%s/%.*s", basePath, (int) uri.length, uri.data);
        *blob = lovrBlobCreate(io.read(filename, &bytesRead), size, NULL);
        lovrAssert((*blob)->data && bytesRead == size, "Unable to read %s", filename);
      } else {
        lovrAssert(glb, "Buffer is missing URI");
        lovrRetain(source);
        *blob = source;
      }
    }
  }

  // Buffers
  if (info.bufferViews) {
    jsmntok_t* token = info.bufferViews;
    ModelBuffer* buffer = model->buffers;
    for (int i = (token++)->size; i > 0; i--, buffer++) {
      size_t offset = 0;

      for (int k = (token++)->size; k > 0; k--) {
        gltfString key = NOM_STR(json, token);
        if (STR_EQ(key, "buffer")) { buffer->data = model->blobs[NOM_INT(json, token)]->data; }
        else if (STR_EQ(key, "byteOffset")) { offset = NOM_INT(json, token); }
        else if (STR_EQ(key, "byteLength")) { buffer->size = NOM_INT(json, token); }
        else if (STR_EQ(key, "byteStride")) { buffer->stride = NOM_INT(json, token); }
        else { token += NOM_VALUE(json, token); }
      }

      if (buffer->data && buffer->data == source->data && glb) {
        offset += binOffset;
      }

      buffer->data = (char*) buffer->data + offset;
    }
  }

  // Attributes
  if (info.attributes) {
    jsmntok_t* token = info.attributes;
    ModelAttribute* attribute = model->attributes;
    for (int i = (token++)->size; i > 0; i--, attribute++) {
      for (int k = (token++)->size; k > 0; k--) {
        gltfString key = NOM_STR(json, token);
        if (STR_EQ(key, "bufferView")) { attribute->buffer = NOM_INT(json, token); }
        else if (STR_EQ(key, "count")) { attribute->count = NOM_INT(json, token); }
        else if (STR_EQ(key, "byteOffset")) { attribute->offset = NOM_INT(json, token); }
        else if (STR_EQ(key, "normalized")) { attribute->normalized = NOM_BOOL(json, token); }
        else if (STR_EQ(key, "componentType")) {
          switch (NOM_INT(json, token)) {
            case 5120: attribute->type = I8; break;
            case 5121: attribute->type = U8; break;
            case 5122: attribute->type = I16; break;
            case 5123: attribute->type = U16; break;
            case 5125: attribute->type = U32; break;
            case 5126: attribute->type = F32; break;
            default: break;
          }
        } else if (STR_EQ(key, "type")) {
          gltfString type = NOM_STR(json, token);
          if (STR_EQ(type, "SCALAR")) {
            attribute->components = 1;
          } else if (type.length == 4) {
            attribute->components = type.data[3] - '0';
            attribute->matrix = type.data[0] == 'M';
          }
        } else if (STR_EQ(key, "min") && token->size <= 4) {
          int count = (token++)->size;
          attribute->hasMin = true;
          for (int j = 0; j < count; j++) {
            attribute->min[j] = NOM_FLOAT(json, token);
          }
        } else if (STR_EQ(key, "max") && token->size <= 4) {
          int count = (token++)->size;
          attribute->hasMax = true;
          for (int j = 0; j < count; j++) {
            attribute->max[j] = NOM_FLOAT(json, token);
          }
        } else {
          token += NOM_VALUE(json, token);
        }
      }
    }
  }

  // Animations
  if (info.animations) {
    int channelIndex = 0;
    int baseSampler = 0;
    jsmntok_t* token = info.animations;
    ModelAnimation* animation = model->animations;
    for (int i = (token++)->size; i > 0; i--, animation++) {
      int samplerCount = 0;
      animation->name = NULL;
      for (int k = (token++)->size; k > 0; k--) {
        gltfString key = NOM_STR(json, token);
        if (STR_EQ(key, "channels")) {
          animation->channelCount = (token++)->size;
          animation->channels = channels + channelIndex;
          channelIndex += animation->channelCount;
          for (int j = 0; j < animation->channelCount; j++) {
            ModelAnimationChannel* channel = &animation->channels[j];
            for (int kk = (token++)->size; kk > 0; kk--) {
              gltfString key = NOM_STR(json, token);
              if (STR_EQ(key, "sampler")) {
                gltfAnimationSampler* sampler = animationSamplers + baseSampler + NOM_INT(json, token);
                channel->times = &model->attributes[sampler->input];
                channel->data = &model->attributes[sampler->output];
                channel->keyframeCount = channel->times->count;
                channel->smoothing = sampler->smoothing;
                ModelBuffer* buffer = &model->buffers[channel->times->buffer];
                size_t stride = buffer->stride ? buffer->stride : (channel->times->components * sizeof(float));
                float lastTime = *(float*) (buffer->data + (channel->times->count - 1) * stride + channel->times->offset);
                animation->duration = MAX(lastTime, animation->duration);
              } else if (STR_EQ(key, "target")) {
                for (int kkk = (token++)->size; kkk > 0; kkk--) {
                  gltfString key = NOM_STR(json, token);
                  if (STR_EQ(key, "node")) { channel->nodeIndex = NOM_INT(json, token); }
                  else if (STR_EQ(key, "path")) {
                    gltfString property = NOM_STR(json, token);
                    if (STR_EQ(property, "translation")) { channel->property = PROP_TRANSLATION; }
                    else if (STR_EQ(property, "rotation")) { channel->property = PROP_ROTATION; }
                    else if (STR_EQ(property, "scale")) { channel->property = PROP_SCALE; }
                    else { lovrThrow("Unknown animation channel property"); }
                  } else {
                    token += NOM_VALUE(json, token);
                  }
                }
              } else {
                token += NOM_VALUE(json, token);
              }
            }
          }
        } else if (STR_EQ(key, "samplers")) {
          samplerCount = token->size;
          token += NOM_VALUE(json, token);
        } else if (STR_EQ(key, "name")) {
          gltfString name = NOM_STR(json, token);
          memcpy(chars, name.data, name.length);
          chars[name.length] = '\0';
          animation->name = chars;
          chars += name.length;
        } else {
          token += NOM_VALUE(json, token);
        }
      }
      baseSampler += samplerCount;
    }
  }

  // Images
  if (info.images) {
    jsmntok_t* token = info.images;
    TextureData** image = model->images;
    for (int i = (token++)->size; i > 0; i--, image++) {
      for (int k = (token++)->size; k > 0; k--) {
        gltfString key = NOM_STR(json, token);
        if (STR_EQ(key, "bufferView")) {
          ModelBuffer* buffer = &model->buffers[NOM_INT(json, token)];
          Blob* blob = lovrBlobCreate(buffer->data, buffer->size, NULL);
          *image = lovrTextureDataCreateFromBlob(blob, false);
          blob->data = NULL; // FIXME
          lovrRelease(blob);
        } else if (STR_EQ(key, "uri")) {
          size_t size = 0;
          char filename[1024];
          gltfString path = NOM_STR(json, token);
          snprintf(filename, 1024, "%s/%.*s%c", basePath, (int) path.length, path.data, 0);
          void* data = io.read(filename, &size);
          lovrAssert(data && size > 0, "Unable to read image from '%s'", filename);
          Blob* blob = lovrBlobCreate(data, size, NULL);
          *image = lovrTextureDataCreateFromBlob(blob, false);
          lovrRelease(blob);
        } else {
          token += NOM_VALUE(json, token);
        }
      }
    }
  }

  // Textures
  if (info.textures) {
    jsmntok_t* token = info.textures;
    ModelTexture* texture = model->textures;
    for (int i = (token++)->size; i > 0; i--, texture++) {
      for (int k = (token++)->size; k > 0; k--) {
        gltfString key = NOM_STR(json, token);
        if (STR_EQ(key, "source")) { texture->imageIndex = NOM_INT(json, token); }
        else if (STR_EQ(key, "sampler")) {
          gltfSampler* sampler = &samplers[NOM_INT(json, token)];
          texture->filter = sampler->filter;
          texture->wrap = sampler->wrap;
          texture->mipmaps = sampler->mipmaps;
        }
        else { token += NOM_VALUE(json, token); }
      }
    }
  }

  // Materials
  if (info.materials) {
    jsmntok_t* token = info.materials;
    ModelMaterial* material = model->materials;
    for (int i = (token++)->size; i > 0; i--, material++) {
      material->scalars[SCALAR_METALNESS] = 1.f;
      material->scalars[SCALAR_ROUGHNESS] = 1.f;
      material->colors[COLOR_DIFFUSE] = (Color) { 1.f, 1.f, 1.f, 1.f };
      material->colors[COLOR_EMISSIVE] = (Color) { 1.f, 1.f, 1.f, 1.f };
      for (int k = (token++)->size; k > 0; k--) {
        gltfString key = NOM_STR(json, token);
        if (STR_EQ(key, "pbrMetallicRoughness")) {
          for (int j = (token++)->size; j > 0; j--) {
            gltfString key = NOM_STR(json, token);
            if (STR_EQ(key, "baseColorFactor")) {
              token++; // Enter array
              material->colors[COLOR_DIFFUSE].r = NOM_FLOAT(json, token);
              material->colors[COLOR_DIFFUSE].g = NOM_FLOAT(json, token);
              material->colors[COLOR_DIFFUSE].b = NOM_FLOAT(json, token);
              material->colors[COLOR_DIFFUSE].a = NOM_FLOAT(json, token);
            } else if (STR_EQ(key, "baseColorTexture")) {
              token = parseTextureInfo(json, token, &material->textures[TEXTURE_DIFFUSE]);
            } else if (STR_EQ(key, "metallicFactor")) {
              material->scalars[SCALAR_METALNESS] = NOM_FLOAT(json, token);
            } else if (STR_EQ(key, "roughnessFactor")) {
              material->scalars[SCALAR_ROUGHNESS] = NOM_FLOAT(json, token);
            } else if (STR_EQ(key, "metallicRoughnessTexture")) {
              token = parseTextureInfo(json, token, &material->textures[TEXTURE_METALNESS]);
              material->textures[TEXTURE_ROUGHNESS] = material->textures[TEXTURE_METALNESS];
            } else {
              token += NOM_VALUE(json, token);
            }
          }
        } else if (STR_EQ(key, "normalTexture")) {
          token = parseTextureInfo(json, token, &material->textures[TEXTURE_NORMAL]);
        } else if (STR_EQ(key, "occlusionTexture")) {
          token = parseTextureInfo(json, token, &material->textures[TEXTURE_OCCLUSION]);
        } else if (STR_EQ(key, "emissiveTexture")) {
          token = parseTextureInfo(json, token, &material->textures[TEXTURE_EMISSIVE]);
        } else if (STR_EQ(key, "emissiveFactor")) {
          token++; // Enter array
          material->colors[COLOR_EMISSIVE].r = NOM_FLOAT(json, token);
          material->colors[COLOR_EMISSIVE].g = NOM_FLOAT(json, token);
          material->colors[COLOR_EMISSIVE].b = NOM_FLOAT(json, token);
        } else {
          token += NOM_VALUE(json, token);
        }
      }
    }
  }

  // Primitives
  if (info.meshes) {
    int primitiveIndex = 0;
    jsmntok_t* token = info.meshes;
    ModelPrimitive* primitive = model->primitives;
    for (int i = (token++)->size; i > 0; i--) {
      for (int k = (token++)->size; k > 0; k--) {
        gltfString key = NOM_STR(json, token);
        if (STR_EQ(key, "primitives")) {
          for (uint32_t j = (token++)->size; j > 0; j--, primitive++) {
            primitive->mode = DRAW_TRIANGLES;

            for (int kk = (token++)->size; kk > 0; kk--) {
              gltfString key = NOM_STR(json, token);
              if (STR_EQ(key, "material")) {
                primitive->material = NOM_INT(json, token);
              } else if (STR_EQ(key, "indices")) {
                primitive->indices = &model->attributes[NOM_INT(json, token)];
              } else if (STR_EQ(key, "mode")) {
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
              } else if (STR_EQ(key, "attributes")) {
                int attributeCount = (token++)->size;
                for (int a = 0; a < attributeCount; a++) {
                  DefaultAttribute attributeType = -1;
                  gltfString name = NOM_STR(json, token);
                  int attributeIndex = NOM_INT(json, token);
                  if (STR_EQ(name, "POSITION")) { attributeType = ATTR_POSITION; }
                  else if (STR_EQ(name, "NORMAL")) { attributeType = ATTR_NORMAL; }
                  else if (STR_EQ(name, "TEXCOORD_0")) { attributeType = ATTR_TEXCOORD; }
                  else if (STR_EQ(name, "COLOR_0")) { attributeType = ATTR_COLOR; }
                  else if (STR_EQ(name, "TANGENT")) { attributeType = ATTR_TANGENT; }
                  else if (STR_EQ(name, "JOINTS_0")) { attributeType = ATTR_BONES; }
                  else if (STR_EQ(name, "WEIGHTS_0")) { attributeType = ATTR_WEIGHTS; }
                  if (attributeType >= 0) {
                    primitive->attributes[attributeType] = &model->attributes[attributeIndex];
                  }
                }
              } else {
                token += NOM_VALUE(json, token);
              }
            }
          }
        } else {
          token += NOM_VALUE(json, token);
        }
      }
    }
  }

  // Nodes
  if (info.nodes) {
    jsmntok_t* token = info.nodes;
    ModelNode* node = model->nodes;
    for (int i = (token++)->size; i > 0; i--, node++) {
      float translation[3] = { 0, 0, 0 };
      float rotation[4] = { 0, 0, 0, 0 };
      float scale[3] = { 1, 1, 1 };
      bool matrix = false;
      node->primitiveCount = 0;
      node->skin = -1;

      for (int k = (token++)->size; k > 0; k--) {
        gltfString key = NOM_STR(json, token);
        if (STR_EQ(key, "mesh")) {
          gltfMesh* mesh = &meshes[NOM_INT(json, token)];
          node->primitiveIndex = mesh->primitiveIndex;
          node->primitiveCount = mesh->primitiveCount;
        } else if (STR_EQ(key, "skin")) {
          node->skin = NOM_INT(json, token);
        } else if (STR_EQ(key, "children")) {
          node->children = &nodeChildren[childIndex];
          node->childCount = (token++)->size;
          for (uint32_t j = 0; j < node->childCount; j++) {
            nodeChildren[childIndex++] = NOM_INT(json, token);
          }
        } else if (STR_EQ(key, "matrix")) {
          lovrAssert((token++)->size == 16, "Node matrix needs 16 elements");
          matrix = true;
          for (int j = 0; j < 16; j++) {
            node->transform[j] = NOM_FLOAT(json, token);
          }
        } else if (STR_EQ(key, "translation")) {
          lovrAssert((token++)->size == 3, "Node translation needs 3 elements");
          translation[0] = NOM_FLOAT(json, token);
          translation[1] = NOM_FLOAT(json, token);
          translation[2] = NOM_FLOAT(json, token);
        } else if (STR_EQ(key, "rotation")) {
          lovrAssert((token++)->size == 4, "Node rotation needs 4 elements");
          rotation[0] = NOM_FLOAT(json, token);
          rotation[1] = NOM_FLOAT(json, token);
          rotation[2] = NOM_FLOAT(json, token);
          rotation[3] = NOM_FLOAT(json, token);
        } else if (STR_EQ(key, "scale")) {
          lovrAssert((token++)->size == 3, "Node scale needs 3 elements");
          scale[0] = NOM_FLOAT(json, token);
          scale[1] = NOM_FLOAT(json, token);
          scale[2] = NOM_FLOAT(json, token);
        } else {
          token += NOM_VALUE(json, token);
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

  // Skins
  if (info.skins) {
    int jointIndex = 0;
    jsmntok_t* token = info.skins;
    ModelSkin* skin = model->skins;
    for (int i = (token++)->size; i > 0; i--, skin++) {
      for (int k = (token++)->size; k > 0; k--) {
        gltfString key = NOM_STR(json, token);
        if (STR_EQ(key, "inverseBindMatrices")) {
          ModelAttribute* attribute = &model->attributes[NOM_INT(json, token)];
          ModelBuffer* buffer = &model->buffers[attribute->buffer];
          skin->inverseBindMatrices = (float*) ((uint8_t*) buffer->data + attribute->offset);
        } else if (STR_EQ(key, "skeleton")) {
          skin->skeleton = NOM_INT(json, token);
        } else if (STR_EQ(key, "joints")) {
          skin->joints = &skinJoints[jointIndex];
          skin->jointCount = (token++)->size;
          for (uint32_t j = 0; j < skin->jointCount; j++) {
            skinJoints[jointIndex++] = NOM_INT(json, token);
          }
        } else {
          token += NOM_VALUE(json, token);
        }
      }
    }
  }

  // Scenes
  if (scenes[rootScene].nodeCount > 1) {
    model->rootNode = model->nodeCount - 1;
    ModelNode* lastNode = &model->nodes[model->rootNode];
    lastNode->childCount = scenes[rootScene].nodeCount;
    lastNode->children = &nodeChildren[childIndex];
    mat4_identity(lastNode->transform);
    lastNode->primitiveCount = 0;
    lastNode->skin = -1;

    jsmntok_t* token = info.scenes;
    int sceneCount = (token++)->size;
    for (int i = 0; i < sceneCount; i++) {
      if (i == rootScene) {
        for (int k = (token++)->size; k > 0; k--) {
          gltfString key = NOM_STR(json, token);
          if (STR_EQ(key, "nodes")) {
            for (int j = (token++)->size; j > 0; j--) {
              lastNode->children[i] = NOM_INT(json, token);
            }
          } else {
            token += NOM_VALUE(json, token);
          }
        }
      } else {
        token += NOM_VALUE(json, token);
      }
    }
  } else {
    model->rootNode = scenes[rootScene].node;
  }

  free(animationSamplers);
  free(meshes);
  free(scenes);
  free(tokens);
  return model;
}

void lovrModelDataDestroy(void* ref) {
  ModelData* model = ref;
  free(model->data);
}
