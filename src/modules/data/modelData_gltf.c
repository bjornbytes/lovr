#include "data/modelData.h"
#include "data/blob.h"
#include "data/image.h"
#include "util.h"
#include "lib/jsmn/jsmn.h"
#include <stdlib.h>
#include <string.h>

#define MAX_STACK_TOKENS 1024

#define MAGIC_glTF 0x46546c67
#define MAGIC_JSON 0x4e4f534a
#define MAGIC_BIN 0x004e4942

#define STR_EQ(k, s) !strncmp(k.data, s, k.length)
#define NOM(t) nomToken(t)
#define NOM_U32(j, t) nomU32(j + (t++)->start)
#define NOM_STR(j, t) (gltfString) { (char* )j + t->start, t->end - t->start }; t++
#define NOM_BOOL(j, t) (*(j + (t++)->start) == 't')
#define NOM_FLOAT(j, t) atof(j + (t++)->start)
#define ASSERT(c, ...) do { if (!(c)) { lovrSetError(__VA_ARGS__); goto fail; } } while (0)

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
  uint32_t blendShapeIndex;
  uint32_t blendShapeCount;
} gltfMesh;

typedef struct {
  uint32_t bufferView;
  gltfString uri;
} gltfImage;

typedef struct {
  uint32_t image;
} gltfTexture;

typedef struct {
  uint32_t node;
  uint32_t nodeCount;
} gltfScene;

static uint32_t nomU32(const char* s) {
  uint32_t n = 0;
  if (*s == '-') return 0;
  while (*s >= '0' && *s <= '9') { n = 10 * n + (*s++ - '0'); }
  return n;
}

static jsmntok_t* nomToken(jsmntok_t* token) {
  for (uint32_t remaining = 1; remaining > 0; remaining--, token++) {
    switch (token->type) {
      case JSMN_OBJECT: remaining += 2 * token->size; break;
      case JSMN_ARRAY: remaining += token->size; break;
      default: break;
    }
  }
  return token;
}

static void* decodeBase64(char* str, size_t length, size_t* decodedLength) {
  char* s = memchr(str, ',', length);
  if (!s) {
    return NULL;
  } else {
    s++;
  }

  length -= s - str;
  int padding = (s[length - 1] == '=') + (s[length - 2] == '=');
  *decodedLength = length / 4 * 3 - padding;
  uint8_t* data = lovrMalloc(*decodedLength);

  uint32_t num = 0;
  uint32_t bits = 0;
  for (size_t i = 0; i < *decodedLength; i++) {
    while (bits < 8) {
      char c = *s++;

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
      } else if (c == '=') {
        break;
      } else {
        lovrFree(data);
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

static jsmntok_t* nomTexture(const char* json, jsmntok_t* token, uint32_t* imageIndex, gltfTexture* textures, ModelMaterial* material) {
  for (int k = (token++)->size; k > 0; k--) {
    gltfString key = NOM_STR(json, token);
    if (STR_EQ(key, "index")) {
      uint32_t index = NOM_U32(json, token);
      gltfTexture* texture = &textures[index];
      *imageIndex = texture->image;
    } else if (material && STR_EQ(key, "extensions")) {
      for (int j = (token++)->size; j > 0; j--) {
        gltfString key = NOM_STR(json, token);
        if (STR_EQ(key, "KHR_texture_transform")) {
          for (int i = (token++)->size; i > 0; i--) {
            gltfString key = NOM_STR(json, token);
            if (STR_EQ(key, "offset")) {
              token++; // Enter array
              material->uvShift[0] = NOM_FLOAT(json, token);
              material->uvShift[1] = NOM_FLOAT(json, token);
            } else if (STR_EQ(key, "scale")) {
              token++; // Enter array
              material->uvScale[0] = NOM_FLOAT(json, token);
              material->uvScale[1] = NOM_FLOAT(json, token);
            } else {
              token = NOM(token);
            }
          }
        } else {
          token = NOM(token);
        }
      }
    } else {
      token = NOM(token);
    }
  }
  return token;
}

static bool loadImage(ModelData* model, gltfImage* images, uint32_t index, ModelDataIO* io, char* filename, size_t maxLength) {
  if (model->images[index]) {
    return true; // Already loaded
  }

  gltfImage* image = &images[index];
  if (image->bufferView != ~0u) {
    ModelBuffer* buffer = &model->buffers[image->bufferView];
    Blob* blob = lovrBlobCreate(buffer->data, buffer->size, NULL);
    model->images[index] = lovrImageCreateFromFile(blob);
    blob->data = NULL; // XXX Blob data ownership
    lovrRelease(blob, lovrBlobDestroy);
  } else if (image->uri.data) {
    void* data;
    Blob* blob;
    size_t size;
    if (image->uri.length >= 5 && !strncmp("data:", image->uri.data, 5)) {
      data = decodeBase64(image->uri.data, image->uri.length, &size);
      lovrAssert(data, "Could not decode base64 image");
      blob = lovrBlobCreate(data, size, NULL);
    } else {
      char* path = image->uri.data;
      size_t length = image->uri.length;
      lovrAssert(length < maxLength, "Image filename is too long");
      lovrAssert(path[0] != '/', "Absolute paths in models are not supported");
      if (path[0] && path[1] && !memcmp(path, "./", 2)) path += 2;
      strncat(filename, path, length);
      data = io(filename, &size);
      lovrAssert(data && size > 0, "Unable to read image from '%s'", filename);
      blob = lovrBlobCreate(data, size, NULL);
    }
    model->images[index] = lovrImageCreateFromFile(blob);
    lovrRelease(blob, lovrBlobDestroy);
  }

  return !!model->images[index];
}

bool lovrModelDataInitGltf(ModelData** result, Blob* source, ModelDataIO* io) {
  uint8_t* data = source->data;
  gltfHeader* header = (gltfHeader*) data;
  bool glb = source->size >= sizeof(gltfHeader) && header->magic == MAGIC_glTF;
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

    if (source->size > sizeof(gltfHeader) + sizeof(gltfChunkHeader) + jsonLength + 4) {
      gltfChunkHeader* binHeader = (gltfChunkHeader*) &json[jsonLength];
      lovrAssert(binHeader->type == MAGIC_BIN, "Invalid BIN header");

      binData = (char*) &binHeader[1];
      binOffset = (char*) binData - (char*) source->data;
    } else {
      binData = NULL;
      binOffset = 0;
    }
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
      heapTokens = lovrRealloc(heapTokens, capacity * sizeof(jsmntok_t));
      tokenCount = jsmn_parse(&parser, json, jsonLength, heapTokens, capacity);
    } while (tokenCount == JSMN_ERROR_NOMEM);

    tokens = heapTokens;
  }

  if (tokenCount <= 0 || tokens[0].type != JSMN_OBJECT) {
    lovrFree(heapTokens);
    return true;
  }

  ModelData* model = lovrCalloc(sizeof(ModelData));
  model->ref = 1;

  model->metadata = lovrMalloc(jsonLength);
  memcpy(model->metadata, json, jsonLength);
  model->metadataSize = jsonLength;
  model->metadataType = META_GLTF_JSON;

  // Prepass: Basically we iterate over the tokens once and figure out how much memory we need and
  // record the locations of tokens that we'll use later to fill in the memory once it's allocated.

  struct {
    jsmntok_t* animations;
    jsmntok_t* attributes;
    jsmntok_t* buffers;
    jsmntok_t* bufferViews;
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
  gltfImage* images = NULL;
  gltfTexture* textures = NULL;
  gltfScene* scenes = NULL;
  int rootScene = 0;

  for (jsmntok_t* token = tokens + 1; token < tokens + tokenCount;) {
    gltfString key = NOM_STR(json, token);

    if (STR_EQ(key, "accessors")) {
      info.attributes = token;
      model->attributeCount = token->size;
      token = NOM(token);

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
            t = NOM(t);
          }
        }
      }

      animationSamplers = lovrMalloc(samplerCount * sizeof(gltfAnimationSampler));
      gltfAnimationSampler* sampler = animationSamplers;
      for (int i = (token++)->size; i > 0; i--) {
        for (int k = (token++)->size; k > 0; k--) {
          gltfString key = NOM_STR(json, token);
          if (STR_EQ(key, "samplers")) {
            for (int j = (token++)->size; j > 0; j--, sampler++) {
              sampler->input = ~0u;
              sampler->output = ~0u;
              sampler->smoothing = SMOOTH_LINEAR;
              for (int k2 = (token++)->size; k2 > 0; k2--) {
                gltfString key = NOM_STR(json, token);
                if (STR_EQ(key, "input")) { sampler->input = NOM_U32(json, token); }
                else if (STR_EQ(key, "output")) { sampler->output = NOM_U32(json, token); }
                else if (STR_EQ(key, "interpolation")) {
                  gltfString smoothing = NOM_STR(json, token);
                  if (STR_EQ(smoothing, "LINEAR")) { sampler->smoothing = SMOOTH_LINEAR; }
                  else if (STR_EQ(smoothing, "STEP")) { sampler->smoothing = SMOOTH_STEP; }
                  else if (STR_EQ(smoothing, "CUBICSPLINE")) { sampler->smoothing = SMOOTH_CUBIC; }
                  else { ASSERT(false, "Unknown animation sampler interpolation"); }
                } else {
                  token = NOM(token);
                }
              }
            }
          } else {
            token = NOM(token);
          }
        }
      }

    } else if (STR_EQ(key, "buffers")) {
      info.buffers = token;
      model->blobCount = token->size;
      token = NOM(token);

    } else if (STR_EQ(key, "bufferViews")) {
      info.bufferViews = token;
      model->bufferCount = token->size;
      token = NOM(token);

    } else if (STR_EQ(key, "images")) {
      model->imageCount = token->size;
      images = lovrMalloc(model->imageCount * sizeof(gltfImage));
      gltfImage* image = images;
      for (int i = (token++)->size; i > 0; i--, image++) {
        image->bufferView = ~0u;
        for (int k = (token++)->size; k > 0; k--) {
          gltfString key = NOM_STR(json, token);
          if (STR_EQ(key, "bufferView")) {
            image->bufferView = NOM_U32(json, token);
          } else if (STR_EQ(key, "uri")) {
            image->uri = NOM_STR(json, token);
          } else {
            token = NOM(token);
          }
        }
        ASSERT(image->bufferView != ~0u || image->uri.data, "Image is missing data");
      }

    } else if (STR_EQ(key, "textures")) {
      textures = lovrMalloc(token->size * sizeof(gltfTexture));
      gltfTexture* texture = textures;
      for (int i = (token++)->size; i > 0; i--, texture++) {
        texture->image = ~0u;
        for (int k = (token++)->size; k > 0; k--) {
          gltfString key = NOM_STR(json, token);
          if (STR_EQ(key, "source") && texture->image == ~0u) {
            texture->image = NOM_U32(json, token);
          } else if (STR_EQ(key, "extensions")) {
            for (int k2 = (token++)->size; k2 > 0; k2--) {
              gltfString key = NOM_STR(json, token);
              if (STR_EQ(key, "KHR_texture_basisu")) {
                for (int k3 = (token++)->size; k3 > 0; k3--) {
                  gltfString key = NOM_STR(json, token);
                  if (STR_EQ(key, "source")) {
                    texture->image = NOM_U32(json, token);
                  } else {
                    token = NOM(token);
                  }
                }
              } else {
                token = NOM(token);
              }
            }
          } else {
            token = NOM(token);
          }
        }
        ASSERT(texture->image != ~0u, "Texture is missing an image (maybe an unsupported extension is used?)");
      }

    } else if (STR_EQ(key, "materials")) {
      info.materials = token;
      model->materialCount = token->size;
      for (int i = (token++)->size; i > 0; i--) {
        for (int k = (token++)->size; k > 0; k--) {
          gltfString key = NOM_STR(json, token);
          if (STR_EQ(key, "name")) { model->charCount += token->end - token->start + 1; }
          token = NOM(token);
        }
      }

    } else if (STR_EQ(key, "meshes")) {
      info.meshes = token;
      meshes = lovrMalloc(token->size * sizeof(gltfMesh));
      gltfMesh* mesh = meshes;
      model->primitiveCount = 0;
      model->blendShapeCount = 0;
      for (int i = (token++)->size; i > 0; i--, mesh++) {
        mesh->primitiveCount = 0;
        mesh->blendShapeCount = 0;
        for (int k = (token++)->size; k > 0; k--) {
          gltfString key = NOM_STR(json, token);
          if (STR_EQ(key, "primitives")) {
            mesh->primitiveIndex = model->primitiveCount;
            mesh->primitiveCount = token->size;
            model->primitiveCount += token->size;
            // Gotta look at targets of a primitive to truly know blend shape situation :')
            for (int p = (token++)->size; p > 0; p--) {
              for (int k2 = (token++)->size; k2 > 0; k2--) {
                gltfString key = NOM_STR(json, token);
                if (STR_EQ(key, "targets")) {
                  if (p == 1) {
                    mesh->blendShapeIndex = model->blendShapeCount;
                    mesh->blendShapeCount = token->size;
                    model->blendShapeCount += token->size;
                  }
                  model->blendDataCount += token->size;
                }
                token = NOM(token);
              }
            }
          } else if (STR_EQ(key, "extras")) {
            for (int k2 = (token++)->size; k2 > 0; k2--) {
              gltfString key = NOM_STR(json, token);
              if (STR_EQ(key, "targetNames")) {
                for (int j = (token++)->size; j > 0; j--) {
                  model->charCount += token->end - token->start + 1;
                  token++;
                }
              } else {
                token = NOM(token);
              }
            }
          } else {
            token = NOM(token);
          }
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
            token = NOM(token);
          }
        }
      }

    } else if (STR_EQ(key, "scene")) {
      rootScene = NOM_U32(json, token);

    } else if (STR_EQ(key, "scenes")) {
      info.scenes = token;
      info.sceneCount = token->size;
      scenes = lovrMalloc(info.sceneCount * sizeof(gltfScene));
      gltfScene* scene = scenes;
      for (int i = (token++)->size; i > 0; i--, scene++) {
        for (int k = (token++)->size; k > 0; k--) {
          gltfString key = NOM_STR(json, token);
          if (STR_EQ(key, "nodes")) {
            scene->nodeCount = token->size;
            jsmntok_t* t = token + 1;
            scene->node = NOM_U32(json, t);
          }
          token = NOM(token);
        }
      }

    } else if (STR_EQ(key, "skins")) {
      info.skins = token;
      model->skinCount = token->size;
      for (int i = (token++)->size; i > 0; i--) {
        for (int k = (token++)->size; k > 0; k--) {
          gltfString key = NOM_STR(json, token);
          if (STR_EQ(key, "joints")) { model->jointCount += token->size; }
          token = NOM(token);
        }
      }

    } else {
      token = NOM(token);
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
        if (STR_EQ(key, "byteLength")) { size = NOM_U32(json, token); }
        else if (STR_EQ(key, "uri")) { uri = NOM_STR(json, token); }
        else { token = NOM(token); }
      }

      if (uri.data) {
        if (uri.length >= 5 && !strncmp("data:", uri.data, 5)) {
          size_t decodedLength;
          void* bufferData = decodeBase64(uri.data, uri.length, &decodedLength);
          ASSERT(bufferData && decodedLength == size, "Could not decode base64 buffer");
          *blob = lovrBlobCreate(bufferData, size, NULL);
        } else {
          size_t bytesRead;
          ASSERT(uri.length < maxPathLength, "Buffer filename is too long");
          ASSERT(uri.data[0] != '/', "Absolute paths in models are not supported");
          if (uri.data[0] && uri.data[1] && !memcmp(uri.data, "./", 2)) uri.data += 2;
          strncat(filename, uri.data, uri.length);
          void* data = io(filename, &bytesRead);
          ASSERT(data && bytesRead == size, "Unable to read '%s'", filename);
          *blob = lovrBlobCreate(data, size, NULL);
          *root = '\0';
        }
      } else {
        ASSERT(glb, "Buffer is missing URI");
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
      for (int k = (token++)->size; k > 0; k--) {
        gltfString key = NOM_STR(json, token);
        if (STR_EQ(key, "buffer")) { buffer->blob = NOM_U32(json, token); }
        else if (STR_EQ(key, "byteOffset")) { buffer->offset = NOM_U32(json, token); }
        else if (STR_EQ(key, "byteLength")) { buffer->size = NOM_U32(json, token); }
        else if (STR_EQ(key, "byteStride")) { buffer->stride = NOM_U32(json, token); }
        else { token = NOM(token); }
      }

      Blob* blob = model->blobs[buffer->blob];

      // If this is the glb binary data, increment the offset to account for the file header
      if (glb && blob == source) {
        buffer->offset += binOffset;
      }

      buffer->data = (char*) blob->data + buffer->offset;
    }
  }

  // Attributes
  if (model->attributeCount > 0) {
    jsmntok_t* token = info.attributes;
    ModelAttribute* attribute = model->attributes;
    for (int i = (token++)->size; i > 0; i--, attribute++) {
      for (int k = (token++)->size; k > 0; k--) {
        gltfString key = NOM_STR(json, token);
        if (STR_EQ(key, "bufferView")) { attribute->buffer = NOM_U32(json, token); }
        else if (STR_EQ(key, "count")) { attribute->count = NOM_U32(json, token); }
        else if (STR_EQ(key, "byteOffset")) { attribute->offset = NOM_U32(json, token); }
        else if (STR_EQ(key, "normalized")) { attribute->normalized = NOM_BOOL(json, token); }
        else if (STR_EQ(key, "componentType")) {
          switch (NOM_U32(json, token)) {
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
          token = NOM(token);
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

            for (int k2 = (token++)->size; k2 > 0; k2--) {
              gltfString key = NOM_STR(json, token);
              if (STR_EQ(key, "sampler")) {
                gltfAnimationSampler* sampler = animationSamplers + baseSampler + NOM_U32(json, token);
                times = &model->attributes[sampler->input];
                data = &model->attributes[sampler->output];
                channel->smoothing = sampler->smoothing;
                channel->keyframeCount = times->count;
              } else if (STR_EQ(key, "target")) {
                for (int k3 = (token++)->size; k3 > 0; k3--) {
                  gltfString key = NOM_STR(json, token);
                  if (STR_EQ(key, "node")) { channel->nodeIndex = NOM_U32(json, token); }
                  else if (STR_EQ(key, "path")) {
                    gltfString property = NOM_STR(json, token);
                    if (STR_EQ(property, "translation")) { channel->property = PROP_TRANSLATION; }
                    else if (STR_EQ(property, "rotation")) { channel->property = PROP_ROTATION; }
                    else if (STR_EQ(property, "scale")) { channel->property = PROP_SCALE; }
                    else if (STR_EQ(property, "weights")) { channel->property = PROP_WEIGHTS; }
                    else { ASSERT(false, "Unknown animation channel property"); }
                  } else {
                    token = NOM(token);
                  }
                }
              } else {
                token = NOM(token);
              }
            }

            ASSERT(times, "Missing keyframe times");
            ASSERT(data, "Missing keyframe data");

            ModelBuffer* buffer;
            buffer = &model->buffers[times->buffer];
            ASSERT(times->type == F32 && (buffer->stride == 0 || buffer->stride == sizeof(float)), "Keyframe times must be tightly-packed floats");
            channel->times = (float*) (buffer->data + times->offset);

            buffer = &model->buffers[data->buffer];
            uint8_t components = data->components;
            ASSERT(data->type == F32 && (buffer->stride == 0 || buffer->stride == sizeof(float) * components), "Keyframe data must be tightly-packed floats");
            channel->data = (float*) (buffer->data + data->offset);

            animation->duration = MAX(animation->duration, channel->times[channel->keyframeCount - 1]);
          }
        } else if (STR_EQ(key, "samplers")) {
          samplerCount = token->size;
          token = NOM(token);
        } else if (STR_EQ(key, "name")) {
          gltfString name = NOM_STR(json, token);
          map_set(model->animationMap, hash64(name.data, name.length), animation - model->animations);
          memcpy(model->chars, name.data, name.length);
          animation->name = model->chars;
          model->chars += name.length + 1;
        } else {
          token = NOM(token);
        }
      }
      baseSampler += samplerCount;
    }
  }

  // Materials
  if (model->materialCount > 0) {
    jsmntok_t* token = info.materials;
    ModelMaterial* material = model->materials;
    for (int i = (token++)->size; i > 0; i--, material++) {
      memcpy(material->color, (float[4]) { 1.f, 1.f, 1.f, 1.f }, 16);
      memcpy(material->glow, (float[4]) { 0.f, 0.f, 0.f, 1.f }, 16);
      material->uvShift[0] = 0.f;
      material->uvShift[1] = 0.f;
      material->uvScale[0] = 1.f;
      material->uvScale[1] = 1.f;
      material->metalness = 1.f;
      material->roughness = 1.f;
      material->clearcoat = 0.f;
      material->clearcoatRoughness = 0.f;
      material->occlusionStrength = 1.f;
      material->normalScale = 1.f;
      material->alphaCutoff = 0.f;
      material->texture = ~0u;
      material->glowTexture = ~0u;
      material->metalnessTexture = ~0u;
      material->roughnessTexture = ~0u;
      material->clearcoatTexture = ~0u;
      material->occlusionTexture = ~0u;
      material->normalTexture = ~0u;

      for (int k = (token++)->size; k > 0; k--) {
        gltfString key = NOM_STR(json, token);
        if (STR_EQ(key, "pbrMetallicRoughness")) {
          for (int j = (token++)->size; j > 0; j--) {
            gltfString key = NOM_STR(json, token);
            if (STR_EQ(key, "baseColorFactor")) {
              token++; // Enter array
              material->color[0] = NOM_FLOAT(json, token);
              material->color[1] = NOM_FLOAT(json, token);
              material->color[2] = NOM_FLOAT(json, token);
              material->color[3] = NOM_FLOAT(json, token);
            } else if (STR_EQ(key, "baseColorTexture")) {
              token = nomTexture(json, token, &material->texture, textures, material);
              if (!loadImage(model, images, material->texture, io, filename, maxPathLength)) goto fail;
              *root = '\0';
            } else if (STR_EQ(key, "metallicFactor")) {
              material->metalness = NOM_FLOAT(json, token);
            } else if (STR_EQ(key, "roughnessFactor")) {
              material->roughness = NOM_FLOAT(json, token);
            } else if (STR_EQ(key, "metallicRoughnessTexture")) {
              token = nomTexture(json, token, &material->metalnessTexture, textures, NULL);
              if (!loadImage(model, images, material->metalnessTexture, io, filename, maxPathLength)) goto fail;
              material->roughnessTexture = material->metalnessTexture;
              *root = '\0';
            } else {
              token = NOM(token);
            }
          }
        } else if (STR_EQ(key, "normalTexture")) {
          token = nomTexture(json, token, &material->normalTexture, textures, NULL);
          if (!loadImage(model, images, material->normalTexture, io, filename, maxPathLength)) goto fail;
          *root = '\0';
        } else if (STR_EQ(key, "occlusionTexture")) {
          token = nomTexture(json, token, &material->occlusionTexture, textures, NULL);
          if (!loadImage(model, images, material->occlusionTexture, io, filename, maxPathLength)) goto fail;
          *root = '\0';
        } else if (STR_EQ(key, "emissiveTexture")) {
          token = nomTexture(json, token, &material->glowTexture, textures, NULL);
          if (!loadImage(model, images, material->glowTexture, io, filename, maxPathLength)) goto fail;
          *root = '\0';
        } else if (STR_EQ(key, "emissiveFactor")) {
          token++; // Enter array
          material->glow[0] = NOM_FLOAT(json, token);
          material->glow[1] = NOM_FLOAT(json, token);
          material->glow[2] = NOM_FLOAT(json, token);
        } else if (STR_EQ(key, "alphaCutoff")) {
          material->alphaCutoff = NOM_FLOAT(json, token);
        } else if (STR_EQ(key, "name")) {
          gltfString name = NOM_STR(json, token);
          map_set(model->materialMap, hash64(name.data, name.length), material - model->materials);
          memcpy(model->chars, name.data, name.length);
          material->name = model->chars;
          model->chars += name.length + 1;
        } else {
          token = NOM(token);
        }
      }
    }
  }

  // Primitives
  if (model->primitiveCount > 0) {
    gltfMesh* mesh = meshes;
    jsmntok_t* token = info.meshes;
    ModelBlendData* blendData = model->blendData;
    ModelPrimitive* primitive = model->primitives;
    for (int i = (token++)->size; i > 0; i--, mesh++) {
      for (int k = (token++)->size; k > 0; k--) {
        gltfString key = NOM_STR(json, token);
        if (STR_EQ(key, "primitives")) {
          for (uint32_t j = (token++)->size; j > 0; j--, primitive++) {
            primitive->mode = DRAW_TRIANGLE_LIST;
            primitive->material = ~0u;

            for (int k2 = (token++)->size; k2 > 0; k2--) {
              gltfString key = NOM_STR(json, token);
              if (STR_EQ(key, "material")) {
                primitive->material = NOM_U32(json, token);
              } else if (STR_EQ(key, "indices")) {
                primitive->indices = &model->attributes[NOM_U32(json, token)];
                ASSERT(primitive->indices->type != U8, "Unsigned byte indices are not supported (must be unsigned shorts or unsigned ints)");
              } else if (STR_EQ(key, "mode")) {
                switch (NOM_U32(json, token)) {
                  case 0: primitive->mode = DRAW_POINT_LIST; break;
                  case 1: primitive->mode = DRAW_LINE_LIST; break;
                  case 2: primitive->mode = DRAW_LINE_LOOP; break;
                  case 3: primitive->mode = DRAW_LINE_STRIP; break;
                  case 4: primitive->mode = DRAW_TRIANGLE_LIST; break;
                  case 5: primitive->mode = DRAW_TRIANGLE_STRIP; break;
                  case 6: primitive->mode = DRAW_TRIANGLE_FAN; break;
                  default: ASSERT(false, "Unknown primitive mode");
                }
              } else if (STR_EQ(key, "attributes")) {
                for (int a = (token++)->size; a > 0; a--) {
                  DefaultAttribute attributeType = ~0;
                  gltfString name = NOM_STR(json, token);
                  uint32_t attributeIndex = NOM_U32(json, token);
                  if (STR_EQ(name, "POSITION")) { attributeType = ATTR_POSITION; }
                  else if (STR_EQ(name, "NORMAL")) { attributeType = ATTR_NORMAL; }
                  else if (STR_EQ(name, "TEXCOORD_0")) { attributeType = ATTR_UV; }
                  else if (STR_EQ(name, "COLOR_0")) { attributeType = ATTR_COLOR; }
                  else if (STR_EQ(name, "TANGENT")) { attributeType = ATTR_TANGENT; }
                  else if (STR_EQ(name, "JOINTS_0")) { attributeType = ATTR_JOINTS; }
                  else if (STR_EQ(name, "WEIGHTS_0")) { attributeType = ATTR_WEIGHTS; }
                  if (attributeType != (DefaultAttribute) ~0) {
                    primitive->attributes[attributeType] = &model->attributes[attributeIndex];
                  }
                }
              } else if (STR_EQ(key, "targets")) {
                primitive->blendShapes = blendData;
                primitive->blendShapeCount = token->size;
                for (int t = (token++)->size; t > 0; t--, blendData++) {
                  for (int a = (token++)->size; a > 0; a--) {
                    gltfString name = NOM_STR(json, token);
                    ModelAttribute* attribute = &model->attributes[NOM_U32(json, token)];
                    if (STR_EQ(name, "POSITION")) { blendData->positions = attribute; }
                    else if (STR_EQ(name, "NORMAL")) { blendData->normals = attribute; }
                    else if (STR_EQ(name, "TANGENT")) { blendData->tangents = attribute; }
                  }
                }
              } else {
                token = NOM(token);
              }
            }
          }
        } else if (STR_EQ(key, "weights")) {
          ASSERT((uint32_t) token->size == mesh->blendShapeCount, "Inconsistent blend shape counts");
          for (int w = (token++)->size, index = mesh->blendShapeIndex; w > 0; w--, index++) {
            model->blendShapes[index].weight = NOM_FLOAT(json, token);
          }
        } else if (STR_EQ(key, "extras")) {
          for (int k2 = (token++)->size; k2 > 0; k2--) {
            gltfString key = NOM_STR(json, token);
            if (STR_EQ(key, "targetNames")) {
              ASSERT((uint32_t) token->size == mesh->blendShapeCount, "Inconsistent blend shape counts");
              for (int k3 = (token++)->size, index = mesh->blendShapeIndex; k3 > 0; k3--, index++) {
                gltfString name = NOM_STR(json, token);
                uint64_t hash = hash64(name.data, name.length);
                if (map_get(model->blendShapeMap, hash) == MAP_NIL) {
                  map_set(model->blendShapeMap, hash, index);
                }
                memcpy(model->chars, name.data, name.length);
                model->chars += name.length + 1;
              }
            } else {
              token = NOM(token);
            }
          }
        } else {
          token = NOM(token);
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
      float* translation = node->transform.translation;
      float* rotation = node->transform.rotation;
      float* scale = node->transform.scale;
      memcpy(translation, (float[3]) { 0.f, 0.f, 0.f }, 3 * sizeof(float));
      memcpy(rotation, (float[4]) { 0.f, 0.f, 0.f, 1.f }, 4 * sizeof(float));
      memcpy(scale, (float[3]) { 1.f, 1.f, 1.f }, 3 * sizeof(float));
      node->hasMatrix = false;
      node->primitiveCount = 0;
      node->skin = ~0u;

      jsmntok_t* weights = NULL;
      for (int k = (token++)->size; k > 0; k--) {
        gltfString key = NOM_STR(json, token);
        if (STR_EQ(key, "mesh")) {
          gltfMesh* mesh = &meshes[NOM_U32(json, token)];
          node->primitiveIndex = mesh->primitiveIndex;
          node->primitiveCount = mesh->primitiveCount;
          node->blendShapeIndex = mesh->blendShapeIndex;
          node->blendShapeCount = mesh->blendShapeCount;
          for (uint32_t i = 0, index = node->blendShapeIndex; i < node->blendShapeCount; i++, index++) {
            model->blendShapes[index].node = node - model->nodes;
          }
        } else if (STR_EQ(key, "weights")) {
          weights = token; // Deferred due to order dependency
        } else if (STR_EQ(key, "skin")) {
          node->skin = NOM_U32(json, token);
        } else if (STR_EQ(key, "children")) {
          node->children = &model->children[childIndex];
          node->childCount = (token++)->size;
          for (uint32_t j = 0; j < node->childCount; j++) {
            model->children[childIndex++] = NOM_U32(json, token);
          }
        } else if (STR_EQ(key, "matrix")) {
          ASSERT((token++)->size == 16, "Node matrix needs 16 elements");
          node->hasMatrix = true;
          for (int j = 0; j < 16; j++) {
            node->transform.matrix[j] = NOM_FLOAT(json, token);
          }
        } else if (STR_EQ(key, "translation")) {
          ASSERT((token++)->size == 3, "Node translation needs 3 elements");
          translation[0] = NOM_FLOAT(json, token);
          translation[1] = NOM_FLOAT(json, token);
          translation[2] = NOM_FLOAT(json, token);
        } else if (STR_EQ(key, "rotation")) {
          ASSERT((token++)->size == 4, "Node rotation needs 4 elements");
          rotation[0] = NOM_FLOAT(json, token);
          rotation[1] = NOM_FLOAT(json, token);
          rotation[2] = NOM_FLOAT(json, token);
          rotation[3] = NOM_FLOAT(json, token);
        } else if (STR_EQ(key, "scale")) {
          ASSERT((token++)->size == 3, "Node scale needs 3 elements");
          scale[0] = NOM_FLOAT(json, token);
          scale[1] = NOM_FLOAT(json, token);
          scale[2] = NOM_FLOAT(json, token);
        } else if (STR_EQ(key, "name")) {
          gltfString name = NOM_STR(json, token);
          map_set(model->nodeMap, hash64(name.data, name.length), node - model->nodes);
          memcpy(model->chars, name.data, name.length);
          node->name = model->chars;
          model->chars += name.length + 1;
        } else {
          token = NOM(token);
        }
      }

      if (node->blendShapeCount > 0 && weights) {
        ASSERT((uint32_t) weights->size == node->blendShapeCount, "Inconsistent blend shape counts");
        for (int w = (weights++)->size, index = node->blendShapeIndex; w > 0; w--, index++) {
          model->blendShapes[index].weight = NOM_FLOAT(json, token);
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
          ModelAttribute* attribute = &model->attributes[NOM_U32(json, token)];
          ModelBuffer* buffer = &model->buffers[attribute->buffer];
          skin->inverseBindMatrices = (float*) ((uint8_t*) buffer->data + attribute->offset);
        } else if (STR_EQ(key, "joints")) {
          skin->joints = &model->joints[jointIndex];
          skin->jointCount = (token++)->size;
          for (uint32_t j = 0; j < skin->jointCount; j++) {
            model->joints[jointIndex++] = NOM_U32(json, token);
          }
        } else {
          token = NOM(token);
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
    lastNode->primitiveCount = 0;
    lastNode->skin = ~0u;

    float* matrix = lastNode->transform.matrix;
    memset(matrix, 0, 16 * sizeof(float));
    matrix[0] = matrix[5] = matrix[10] = matrix[15] = 1.f;
    lastNode->hasMatrix = true;

    jsmntok_t* token = info.scenes;
    int sceneCount = (token++)->size;
    for (int i = 0; i < sceneCount; i++) {
      if (i == rootScene) {
        for (int k = (token++)->size; k > 0; k--) {
          gltfString key = NOM_STR(json, token);
          if (STR_EQ(key, "nodes")) {
            for (int j = (token++)->size; j > 0; j--) {
              lastNode->children[lastNode->childCount - j] = NOM_U32(json, token);
            }
          } else {
            token = NOM(token);
          }
        }
      } else {
        token = NOM(token);
      }
    }
  } else {
    model->rootNode = scenes[rootScene].node;
  }

  lovrFree(animationSamplers);
  lovrFree(meshes);
  lovrFree(images);
  lovrFree(textures);
  lovrFree(scenes);
  lovrFree(heapTokens);
  *result = model;
  return true;

fail:
  lovrFree(animationSamplers);
  lovrFree(meshes);
  lovrFree(images);
  lovrFree(textures);
  lovrFree(scenes);
  lovrFree(heapTokens);
  lovrModelDataDestroy(model);
  return false;
}
