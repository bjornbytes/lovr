#include "data/modelData.h"
#include "data/blob.h"
#include "data/textureData.h"
#include "filesystem/filesystem.h"
#include "core/hash.h"
#include "core/maf.h"
#include "core/ref.h"
#include "lib/jsmn/jsmn.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_STACK_TOKENS 1024

#define MAGIC_glTF 0x46546c67
#define MAGIC_JSON 0x4e4f534a
#define MAGIC_BIN 0x004e4942

#define STR_EQ(k, s) !strncmp(k.data, s, k.length)
#define NOM_VALUE(j, t) nomValue(j, t, 1, 0)
#define NOM_INT(j, t) nomInt(j + (t++)->start)
#define NOM_STR(j, t) (gltfString) { (char* )j + t->start, t->end - t->start }; t++
#define NOM_BOOL(j, t) (*(j + (t++)->start) == 't')
#define NOM_FLOAT(j, t) atof(j + (t++)->start)

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
  uint32_t input;
  uint32_t output;
  SmoothMode smoothing;
} gltfAnimationSampler;

typedef struct {
  uint32_t primitiveIndex;
  uint32_t primitiveCount;
} gltfMesh;

typedef struct {
  TextureFilter filter;
  TextureWrap wrap;
} gltfSampler;

typedef struct {
  uint32_t image;
  uint32_t sampler;
} gltfTexture;

typedef struct {
  uint32_t node;
  uint32_t nodeCount;
} gltfScene;

static uint32_t nomInt(const char* s) {
  uint32_t n = 0;
  lovrAssert(*s != '-', "Expected a positive number");
  while (isdigit(*s)) { n = 10 * n + (*s++ - '0'); }
  return n;
}

static int nomValue(const char* data, jsmntok_t* token, int count, int sum) {
  if (count == 0) { return sum; }
  switch (token->type) {
    case JSMN_OBJECT: return nomValue(data, token + 1, count - 1 + 2 * token->size, sum + 1);
    case JSMN_ARRAY: return nomValue(data, token + 1, count - 1 + token->size, sum + 1);
    default: return nomValue(data, token + 1, count - 1, sum + 1);
  }
}

static void* decodeBase64(char* str, size_t length, size_t decodedSize) {
  str = memchr(str, ',', length);
  if (!str) {
    return NULL;
  } else {
    str++;
  }

  uint8_t* data = malloc(decodedSize);
  if (!data) {
    return NULL;
  }

  uint32_t num = 0;
  uint32_t bits = 0;
  for (size_t i = 0; i < decodedSize; i++) {
    while (bits < 8) {
      char c = *str++;

      uint32_t n;
      if (c >= 'A' && c <= 'Z') {
        n = c - 'A';
      } else if (c >= 'a' && c <= 'z') {
        n = c - 'a' + 26;
      } else if (c >= '0' && c <= '9') {
        n = c - '0' + 52;
      } else if (c == '+') {
        n = 62;
      } else if (c == '/') {
        n = 63;
      } else {
        free(data);
        return NULL;
      }

      num <<= 6;
      num |= n;
      bits += 6;
    }

    data[i] = num >> (bits - 8);
    bits -= 8;
  }

  return data;
}

static jsmntok_t* resolveTexture(const char* json, jsmntok_t* token, ModelMaterial* material, MaterialTexture textureType, gltfTexture* textures, gltfSampler* samplers) {
  for (int k = (token++)->size; k > 0; k--) {
    gltfString key = NOM_STR(json, token);
    if (STR_EQ(key, "index")) {
      uint32_t index = NOM_INT(json, token);
      gltfTexture* texture = &textures[index];
      gltfSampler* sampler = texture->sampler == ~0u ? NULL : &samplers[texture->sampler];
      material->textures[textureType] = texture->image;
      material->filters[textureType] = sampler ? sampler->filter : (TextureFilter) { .mode = FILTER_BILINEAR };
      material->wraps[textureType] = sampler ? sampler->wrap : (TextureWrap) { .s = WRAP_REPEAT, .t = WRAP_REPEAT, .r = WRAP_REPEAT };
    } else if (STR_EQ(key, "texCoord")) {
      lovrAssert(NOM_INT(json, token) == 0, "Only one set of texture coordinates is supported");
    } else {
      token += NOM_VALUE(json, token);
    }
  }
  return token;
}

ModelData* lovrModelDataInitGltf(ModelData* model, Blob* source) {
  uint8_t* data = source->data;
  gltfHeader* header = (gltfHeader*) data;
  bool glb = header->magic == MAGIC_glTF;
  const char *json, *binData;
  size_t jsonLength;
  ptrdiff_t binOffset;

  char filename[1024];
  lovrAssert(strlen(source->name) < sizeof(filename), "glTF filename is too long");
  strcpy(filename, source->name);
  char* slash = strrchr(filename, '/');
  char* root = slash ? (slash + 1) : filename;
  size_t maxPathLength = sizeof(filename) - (root - filename);
  *root = '\0';

  if (glb) {
    gltfChunkHeader* jsonHeader = (gltfChunkHeader*) &header[1];
    lovrAssert(jsonHeader->type == MAGIC_JSON, "Invalid JSON header");

    json = (char*) &jsonHeader[1];
    jsonLength = jsonHeader->length;

    gltfChunkHeader* binHeader = (gltfChunkHeader*) &json[jsonLength];
    lovrAssert(binHeader->type == MAGIC_BIN, "Invalid BIN header");

    binData = (char*) &binHeader[1];
    binOffset = (char*) binData - (char*) source->data;
  } else {
    json = (char*) data;
    jsonLength = source->size;
    binData = NULL;
    binOffset = 0;
  }

  // Parse JSON
  jsmn_parser parser;
  jsmn_init(&parser);

  jsmntok_t stackTokens[MAX_STACK_TOKENS];
  jsmntok_t* heapTokens = NULL;
  jsmntok_t* tokens = &stackTokens[0];
  int tokenCount = 0;

  if ((tokenCount = jsmn_parse(&parser, json, jsonLength, stackTokens, MAX_STACK_TOKENS)) == JSMN_ERROR_NOMEM) {
    int capacity = MAX_STACK_TOKENS;
    jsmn_init(&parser); // This shouldn't be necessary but not doing it caused an infinite loop?

    do {
      capacity *= 2;
      heapTokens = realloc(heapTokens, capacity * sizeof(jsmntok_t));
      lovrAssert(heapTokens, "Out of memory");
      tokenCount = jsmn_parse(&parser, json, jsonLength, heapTokens, capacity);
    } while (tokenCount == JSMN_ERROR_NOMEM);

    tokens = heapTokens;
  }

  if (tokenCount <= 0 || tokens[0].type != JSMN_OBJECT) {
    free(heapTokens);
    return NULL;
  }

  // Prepass: Basically we iterate over the tokens once and figure out how much memory we need and
  // record the locations of tokens that we'll use later to fill in the memory once it's allocated.

  struct {
    jsmntok_t* animations;
    jsmntok_t* attributes;
    jsmntok_t* buffers;
    jsmntok_t* bufferViews;
    jsmntok_t* images;
    jsmntok_t* materials;
    jsmntok_t* meshes;
    jsmntok_t* nodes;
    jsmntok_t* scenes;
    jsmntok_t* skins;
    int sceneCount;
  } info;

  memset(&info, 0, sizeof(info));

  gltfAnimationSampler* animationSamplers = NULL;
  gltfMesh* meshes = NULL;
  gltfSampler* samplers = NULL;
  gltfTexture* textures = NULL;
  gltfScene* scenes = NULL;
  int rootScene = 0;

  for (jsmntok_t* token = tokens + 1; token < tokens + tokenCount;) {
    gltfString key = NOM_STR(json, token);

    if (STR_EQ(key, "accessors")) {
      info.attributes = token;
      model->attributeCount = token->size;
      token += NOM_VALUE(json, token);

    } else if (STR_EQ(key, "animations")){
      info.animations = token;
      model->animationCount = token->size;
      size_t samplerCount = 0;
      jsmntok_t* t = token;
      for (int i = (t++)->size; i > 0; i--) {
        if (t->size > 0) {
          for (int k = (t++)->size; k > 0; k--) {
            gltfString key = NOM_STR(json, t);
            if (STR_EQ(key, "channels")) { model->channelCount += t->size; }
            else if (STR_EQ(key, "samplers")) { samplerCount += t->size; }
            else if (STR_EQ(key, "name")) { model->charCount += t->end - t->start + 1; }
            t += NOM_VALUE(json, t);
          }
        }
      }

      animationSamplers = malloc(samplerCount * sizeof(gltfAnimationSampler));
      lovrAssert(animationSamplers, "Out of memory");
      gltfAnimationSampler* sampler = animationSamplers;
      for (int i = (token++)->size; i > 0; i--) {
        for (int k = (token++)->size; k > 0; k--) {
          gltfString key = NOM_STR(json, token);
          if (STR_EQ(key, "samplers")) {
            for (int j = (token++)->size; j > 0; j--, sampler++) {
              sampler->input = ~0u;
              sampler->output = ~0u;
              sampler->smoothing = SMOOTH_LINEAR;
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

    } else if (STR_EQ(key, "buffers")) {
      info.buffers = token;
      model->blobCount = token->size;
      token += NOM_VALUE(json, token);

    } else if (STR_EQ(key, "bufferViews")) {
      info.bufferViews = token;
      model->bufferCount = token->size;
      token += NOM_VALUE(json, token);

    } else if (STR_EQ(key, "images")) {
      info.images = token;
      model->textureCount = token->size;
      token += NOM_VALUE(json, token);

    } else if (STR_EQ(key, "samplers")) {
      samplers = malloc(token->size * sizeof(gltfSampler));
      lovrAssert(samplers, "Out of memory");
      gltfSampler* sampler = samplers;
      for (int i = (token++)->size; i > 0; i--, sampler++) {
        sampler->filter.mode = FILTER_BILINEAR;
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
            case 9729: sampler->filter.mode = FILTER_BILINEAR; break;
            case 9985: sampler->filter.mode = FILTER_BILINEAR; break;
            case 9987: sampler->filter.mode = FILTER_TRILINEAR; break;
          }
        }
      }

    } else if (STR_EQ(key, "textures")) {
      textures = malloc(token->size * sizeof(gltfTexture));
      lovrAssert(textures, "Out of memory");
      gltfTexture* texture = textures;
      for (int i = (token++)->size; i > 0; i--, texture++) {
        texture->image = ~0u;
        texture->sampler = ~0u;
        for (int k = (token++)->size; k > 0; k--) {
          gltfString key = NOM_STR(json, token);
          if (STR_EQ(key, "source")) {
            texture->image = NOM_INT(json, token);
          } else if (STR_EQ(key, "sampler")) {
            texture->sampler = NOM_INT(json, token);
          } else {
            token += NOM_VALUE(json, token);
          }
        }
        lovrAssert(texture->image != ~0u, "Texture is missing an image (maybe an unsupported extension is used?)");
      }

    } else if (STR_EQ(key, "materials")) {
      info.materials = token;
      model->materialCount = token->size;
      for (int i = (token++)->size; i > 0; i--) {
        for (int k = (token++)->size; k > 0; k--) {
          gltfString key = NOM_STR(json, token);
          if (STR_EQ(key, "name")) { model->charCount += token->end - token->start + 1; }
          token += NOM_VALUE(json, token);
        }
      }

    } else if (STR_EQ(key, "meshes")) {
      info.meshes = token;
      meshes = malloc(token->size * sizeof(gltfMesh));
      lovrAssert(meshes, "Out of memory");
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

    } else if (STR_EQ(key, "nodes")) {
      info.nodes = token;
      model->nodeCount = token->size;
      for (int i = (token++)->size; i > 0; i--) {
        if (token->size > 0) {
          for (int k = (token++)->size; k > 0; k--) {
            gltfString key = NOM_STR(json, token);
            if (STR_EQ(key, "children")) { model->childCount += token->size; }
            else if (STR_EQ(key, "name")) { model->charCount += token->end - token->start + 1; }
            token += NOM_VALUE(json, token);
          }
        }
      }

    } else if (STR_EQ(key, "scene")) {
      rootScene = NOM_INT(json, token);

    } else if (STR_EQ(key, "scenes")) {
      info.scenes = token;
      info.sceneCount = token->size;
      scenes = malloc(info.sceneCount * sizeof(gltfScene));
      lovrAssert(scenes, "Out of memory");
      gltfScene* scene = scenes;
      for (int i = (token++)->size; i > 0; i--, scene++) {
        for (int k = (token++)->size; k > 0; k--) {
          gltfString key = NOM_STR(json, token);
          if (STR_EQ(key, "nodes")) {
            scene->nodeCount = token->size;
            jsmntok_t* t = token + 1;
            scene->node = NOM_INT(json, t);
          }
          token += NOM_VALUE(json, token);
        }
      }

    } else if (STR_EQ(key, "skins")) {
      info.skins = token;
      model->skinCount = token->size;
      for (int i = (token++)->size; i > 0; i--) {
        for (int k = (token++)->size; k > 0; k--) {
          gltfString key = NOM_STR(json, token);
          if (STR_EQ(key, "joints")) { model->jointCount += token->size; }
          token += NOM_VALUE(json, token);
        }
      }

    } else {
      token += NOM_VALUE(json, token);
    }
  }

  // We only support a single root node, so if there is more than one root node in the scene then
  // we create a fake "super root" node and add all the scene's root nodes as its children.
  if (info.sceneCount > 0 && scenes[rootScene].nodeCount > 1) {
    model->childCount += model->nodeCount;
    model->nodeCount++;
  }

  // Allocate memory, then revisit all of the tokens that were recorded during the prepass and write
  // their data into this memory.
  lovrModelDataAllocate(model);

  // Blobs
  if (model->blobCount > 0) {
    jsmntok_t* token = info.buffers;
    Blob** blob = model->blobs;
    for (int i = (token++)->size; i > 0; i--, blob++) {
      gltfString uri;
      memset(&uri, 0, sizeof(uri));
      size_t size = 0;

      for (int k = (token++)->size; k > 0; k--) {
        gltfString key = NOM_STR(json, token);
        if (STR_EQ(key, "byteLength")) { size = NOM_INT(json, token); }
        else if (STR_EQ(key, "uri")) { uri = NOM_STR(json, token); }
        else { token += NOM_VALUE(json, token); }
      }

      if (uri.data) {
        size_t bytesRead;
        if (uri.length >= 5 && !strncmp("data:", uri.data, 5)) {
          void* bufferData = decodeBase64(uri.data, uri.length, size);
          lovrAssert(bufferData, "Could not decode base64 buffer");
          *blob = lovrBlobCreate(bufferData, size, NULL);
        } else {
          lovrAssert(uri.length < maxPathLength, "Buffer filename is too long");
          strncat(filename, uri.data, uri.length);
          *blob = lovrBlobCreate(lovrFilesystemRead(filename, -1, &bytesRead), size, NULL);
          lovrAssert((*blob)->data && bytesRead == size, "Unable to read %s", filename);
          *root = '\0';
        }
      } else {
        lovrAssert(glb, "Buffer is missing URI");
        lovrRetain(source);
        *blob = source;
      }
    }
  }

  // Buffers
  if (model->bufferCount > 0) {
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

      // If this is the glb binary data, increment the offset to account for the file header
      if (buffer->data && buffer->data == source->data && glb) {
        offset += binOffset;
      }

      buffer->data = (char*) buffer->data + offset;
    }
  }

  // Attributes
  if (model->attributeCount > 0) {
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
  if (model->animationCount > 0) {
    int channelIndex = 0;
    int baseSampler = 0;
    jsmntok_t* token = info.animations;
    ModelAnimation* animation = model->animations;
    for (int i = (token++)->size; i > 0; i--, animation++) {
      int samplerCount = 0;
      for (int k = (token++)->size; k > 0; k--) {
        gltfString key = NOM_STR(json, token);
        if (STR_EQ(key, "channels")) {
          animation->channelCount = (token++)->size;
          animation->channels = model->channels + channelIndex;
          channelIndex += animation->channelCount;
          for (uint32_t j = 0; j < animation->channelCount; j++) {
            ModelAnimationChannel* channel = &animation->channels[j];
            ModelAttribute* times = NULL;
            ModelAttribute* data = NULL;

            for (int kk = (token++)->size; kk > 0; kk--) {
              gltfString key = NOM_STR(json, token);
              if (STR_EQ(key, "sampler")) {
                gltfAnimationSampler* sampler = animationSamplers + baseSampler + NOM_INT(json, token);
                times = &model->attributes[sampler->input];
                data = &model->attributes[sampler->output];
                channel->smoothing = sampler->smoothing;
                channel->keyframeCount = times->count;
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

            lovrAssert(times, "Missing keyframe times");
            lovrAssert(data, "Missing keyframe data");

            ModelBuffer* buffer;
            buffer = &model->buffers[times->buffer];
            lovrAssert(times->type == F32 && (buffer->stride == 0 || buffer->stride == sizeof(float)), "Keyframe times must be tightly-packed floats");
            channel->times = (float*) (buffer->data + times->offset);

            buffer = &model->buffers[data->buffer];
            uint8_t components = data->components;
            lovrAssert(data->type == F32 && (buffer->stride == 0 || buffer->stride == sizeof(float) * components), "Keyframe data must be tightly-packed floats");
            channel->data = (float*) (buffer->data + data->offset);

            animation->duration = MAX(animation->duration, channel->times[channel->keyframeCount - 1]);
          }
        } else if (STR_EQ(key, "samplers")) {
          samplerCount = token->size;
          token += NOM_VALUE(json, token);
        } else if (STR_EQ(key, "name")) {
          gltfString name = NOM_STR(json, token);
          map_set(&model->animationMap, hash64(name.data, name.length), model->animationCount - i);
          memcpy(model->chars, name.data, name.length);
          animation->name = model->chars;
          model->chars += name.length + 1;
        } else {
          token += NOM_VALUE(json, token);
        }
      }
      baseSampler += samplerCount;
    }
  }

  // Textures (glTF images)
  if (model->textureCount > 0) {
    jsmntok_t* token = info.images;
    TextureData** texture = model->textures;
    for (int i = (token++)->size; i > 0; i--, texture++) {
      for (int k = (token++)->size; k > 0; k--) {
        gltfString key = NOM_STR(json, token);
        if (STR_EQ(key, "bufferView")) {
          ModelBuffer* buffer = &model->buffers[NOM_INT(json, token)];
          Blob* blob = lovrBlobCreate(buffer->data, buffer->size, NULL);
          *texture = lovrTextureDataCreateFromBlob(blob, false);
          blob->data = NULL; // XXX Blob data ownership
          lovrRelease(Blob, blob);
        } else if (STR_EQ(key, "uri")) {
          size_t size = 0;
          gltfString uri = NOM_STR(json, token);
          lovrAssert(uri.length < 5 || strncmp("data:", uri.data, 5), "Base64 images aren't supported yet");
          lovrAssert(uri.length < maxPathLength, "Image filename is too long");
          strncat(filename, uri.data, uri.length);
          void* data = lovrFilesystemRead(filename, -1, &size);
          lovrAssert(data && size > 0, "Unable to read texture from '%s'", filename);
          Blob* blob = lovrBlobCreate(data, size, NULL);
          *texture = lovrTextureDataCreateFromBlob(blob, false);
          lovrRelease(Blob, blob);
          *root = '\0';
        } else {
          token += NOM_VALUE(json, token);
        }
      }
    }
  }

  // Materials
  if (model->materialCount > 0) {
    jsmntok_t* token = info.materials;
    ModelMaterial* material = model->materials;
    for (int i = (token++)->size; i > 0; i--, material++) {
      material->scalars[SCALAR_METALNESS] = 1.f;
      material->scalars[SCALAR_ROUGHNESS] = 1.f;
      material->colors[COLOR_DIFFUSE] = (Color) { 1.f, 1.f, 1.f, 1.f };
      material->colors[COLOR_EMISSIVE] = (Color) { 0.f, 0.f, 0.f, 0.f };
      memset(material->textures, 0xff, MAX_MATERIAL_TEXTURES * sizeof(uint32_t));

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
              token = resolveTexture(json, token, material, TEXTURE_DIFFUSE, textures, samplers);
            } else if (STR_EQ(key, "metallicFactor")) {
              material->scalars[SCALAR_METALNESS] = NOM_FLOAT(json, token);
            } else if (STR_EQ(key, "roughnessFactor")) {
              material->scalars[SCALAR_ROUGHNESS] = NOM_FLOAT(json, token);
            } else if (STR_EQ(key, "metallicRoughnessTexture")) {
              token = resolveTexture(json, token, material, TEXTURE_METALNESS, textures, samplers);
              material->textures[TEXTURE_ROUGHNESS] = material->textures[TEXTURE_METALNESS];
              material->filters[TEXTURE_ROUGHNESS] = material->filters[TEXTURE_METALNESS];
              material->wraps[TEXTURE_ROUGHNESS] = material->wraps[TEXTURE_METALNESS];
            } else {
              token += NOM_VALUE(json, token);
            }
          }
        } else if (STR_EQ(key, "normalTexture")) {
          token = resolveTexture(json, token, material, TEXTURE_NORMAL, textures, samplers);
        } else if (STR_EQ(key, "occlusionTexture")) {
          token = resolveTexture(json, token, material, TEXTURE_OCCLUSION, textures, samplers);
        } else if (STR_EQ(key, "emissiveTexture")) {
          token = resolveTexture(json, token, material, TEXTURE_EMISSIVE, textures, samplers);
        } else if (STR_EQ(key, "emissiveFactor")) {
          token++; // Enter array
          material->colors[COLOR_EMISSIVE].r = NOM_FLOAT(json, token);
          material->colors[COLOR_EMISSIVE].g = NOM_FLOAT(json, token);
          material->colors[COLOR_EMISSIVE].b = NOM_FLOAT(json, token);
        } else if (STR_EQ(key, "name")) {
          gltfString name = NOM_STR(json, token);
          map_set(&model->materialMap, hash64(name.data, name.length), model->materialCount - i);
          memcpy(model->chars, name.data, name.length);
          material->name = model->chars;
          model->chars += name.length + 1;
        } else {
          token += NOM_VALUE(json, token);
        }
      }
    }
  }

  // Primitives
  if (model->primitiveCount > 0) {
    jsmntok_t* token = info.meshes;
    ModelPrimitive* primitive = model->primitives;
    for (int i = (token++)->size; i > 0; i--) {
      for (int k = (token++)->size; k > 0; k--) {
        gltfString key = NOM_STR(json, token);
        if (STR_EQ(key, "primitives")) {
          for (uint32_t j = (token++)->size; j > 0; j--, primitive++) {
            primitive->mode = DRAW_TRIANGLES;
            primitive->material = ~0u;

            for (int kk = (token++)->size; kk > 0; kk--) {
              gltfString key = NOM_STR(json, token);
              if (STR_EQ(key, "material")) {
                primitive->material = NOM_INT(json, token);
              } else if (STR_EQ(key, "indices")) {
                primitive->indices = &model->attributes[NOM_INT(json, token)];
                lovrAssert(primitive->indices->type != U8, "Unsigned byte indices are not supported (must be unsigned shorts or unsigned ints)");
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
                  DefaultAttribute attributeType = ~0;
                  gltfString name = NOM_STR(json, token);
                  uint32_t attributeIndex = NOM_INT(json, token);
                  if (STR_EQ(name, "POSITION")) { attributeType = ATTR_POSITION; }
                  else if (STR_EQ(name, "NORMAL")) { attributeType = ATTR_NORMAL; }
                  else if (STR_EQ(name, "TEXCOORD_0")) { attributeType = ATTR_TEXCOORD; }
                  else if (STR_EQ(name, "COLOR_0")) { attributeType = ATTR_COLOR; }
                  else if (STR_EQ(name, "TANGENT")) { attributeType = ATTR_TANGENT; }
                  else if (STR_EQ(name, "JOINTS_0")) { attributeType = ATTR_BONES; }
                  else if (STR_EQ(name, "WEIGHTS_0")) { attributeType = ATTR_WEIGHTS; }
                  if (attributeType != (DefaultAttribute) ~0) {
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
  uint32_t childIndex = 0;
  if (model->nodeCount > 0) {
    jsmntok_t* token = info.nodes;
    ModelNode* node = model->nodes;
    for (int i = (token++)->size; i > 0; i--, node++) {
      vec3 translation = vec3_set(node->translation, 0.f, 0.f, 0.f);
      quat rotation = quat_set(node->rotation, 0.f, 0.f, 0.f, 1.f);
      vec3 scale = vec3_set(node->scale, 1.f, 1.f, 1.f);
      node->matrix = false;
      node->primitiveCount = 0;
      node->skin = ~0u;

      for (int k = (token++)->size; k > 0; k--) {
        gltfString key = NOM_STR(json, token);
        if (STR_EQ(key, "mesh")) {
          gltfMesh* mesh = &meshes[NOM_INT(json, token)];
          node->primitiveIndex = mesh->primitiveIndex;
          node->primitiveCount = mesh->primitiveCount;
        } else if (STR_EQ(key, "skin")) {
          node->skin = NOM_INT(json, token);
        } else if (STR_EQ(key, "children")) {
          node->children = &model->children[childIndex];
          node->childCount = (token++)->size;
          for (uint32_t j = 0; j < node->childCount; j++) {
            model->children[childIndex++] = NOM_INT(json, token);
          }
        } else if (STR_EQ(key, "matrix")) {
          lovrAssert((token++)->size == 16, "Node matrix needs 16 elements");
          node->matrix = true;
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
        } else if (STR_EQ(key, "name")) {
          gltfString name = NOM_STR(json, token);
          map_set(&model->nodeMap, hash64(name.data, name.length), model->nodeCount - i);
          memcpy(model->chars, name.data, name.length);
          node->name = model->chars;
          model->chars += name.length + 1;
        } else {
          token += NOM_VALUE(json, token);
        }
      }
    }
  }

  // Skins
  if (model->skinCount > 0) {
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
        } else if (STR_EQ(key, "joints")) {
          skin->joints = &model->joints[jointIndex];
          skin->jointCount = (token++)->size;
          for (uint32_t j = 0; j < skin->jointCount; j++) {
            model->joints[jointIndex++] = NOM_INT(json, token);
          }
        } else {
          token += NOM_VALUE(json, token);
        }
      }
    }
  }

  // Scenes
  if (info.sceneCount == 0) {
    model->rootNode = 0;
  } else if (scenes[rootScene].nodeCount > 1) {
    model->rootNode = model->nodeCount - 1;
    ModelNode* lastNode = &model->nodes[model->rootNode];
    lastNode->childCount = scenes[rootScene].nodeCount;
    lastNode->children = &model->children[childIndex];
    mat4_identity(lastNode->transform);
    lastNode->matrix = true;
    lastNode->primitiveCount = 0;
    lastNode->skin = ~0u;

    jsmntok_t* token = info.scenes;
    int sceneCount = (token++)->size;
    for (int i = 0; i < sceneCount; i++) {
      if (i == rootScene) {
        for (int k = (token++)->size; k > 0; k--) {
          gltfString key = NOM_STR(json, token);
          if (STR_EQ(key, "nodes")) {
            for (int j = (token++)->size; j > 0; j--) {
              lastNode->children[lastNode->childCount - j] = NOM_INT(json, token);
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
  free(samplers);
  free(textures);
  free(scenes);
  free(heapTokens);
  return model;
}
