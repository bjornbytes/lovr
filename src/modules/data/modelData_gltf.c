#include "data/modelData.h"
#include "data/blob.h"
#include "data/image.h"
#include "util.h"
#include "core/job.h"
#include "lib/jsmn/jsmn.h"
#include <stdatomic.h>
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

typedef enum { I8, U8, I16, U16, I32, U32, F32, SN10x3 } ComponentType;

typedef struct {
  uint32_t blob;
  size_t offset;
  size_t size;
  size_t stride;
  char* data;
} gltfBufferView;

typedef struct {
  char* data;
  size_t stride;
  uint32_t bufferView;
  uint32_t offset;
  uint32_t count;
  ComponentType type;
  uint32_t components;
  bool normalized;
  bool matrix;
  bool hasMin;
  bool hasMax;
  float min[4];
  float max[4];
} gltfAccessor;

typedef struct {
  uint32_t input;
  uint32_t output;
  SmoothMode smoothing;
} gltfAnimationSampler;

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

typedef struct {
  atomic_uint done;
  Image* result;
  gltfImage* image;
  gltfBufferView* buffers;
  ModelDataIO* io;
  char* basePath;
  char* error;
} ImageJob;

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

static void loadImage(void* arg) {
  ImageJob* ctx = arg;
  Blob* blob;

  if (ctx->image->bufferView != ~0u) {
    gltfBufferView* buffer = &ctx->buffers[ctx->image->bufferView];
    blob = lovrBlobCreate(buffer->data, buffer->size, NULL);
    ctx->result = lovrImageCreateFromFile(blob);
    blob->data = NULL; // XXX Blob data ownership
    lovrRelease(blob, lovrBlobDestroy);
  } else if (ctx->image->uri.data) {
    if (ctx->image->uri.length >= 5 && !strncmp("data:", ctx->image->uri.data, 5)) {
      size_t size;
      void* data = decodeBase64(ctx->image->uri.data, ctx->image->uri.length, &size);
      if (!data) {
        ctx->error = lovrStrdup("Could not decode base64 image");
        atomic_store(&ctx->done, 1);
        return;
      }
      blob = lovrBlobCreate(data, size, NULL);
    } else {
      char* path = ctx->image->uri.data;
      size_t length = ctx->image->uri.length;

      if (path[0] == '/') {
        ctx->error = lovrStrdup("Absolute paths in models are not supported");
        atomic_store(&ctx->done, 1);
        return;
      }

      // Remove ./ prefix
      if (path[0] && path[1] && !memcmp(path, "./", 2)) {
        path += 2;
        length -= 2;
      }

      // basePath/path
      size_t baseLength = strlen(ctx->basePath);
      size_t totalLength = baseLength + 1 + length + 1;
      char* fullpath = lovrMalloc(totalLength);

      memcpy(fullpath, ctx->basePath, baseLength);
      fullpath[baseLength] = '/';

      memcpy(fullpath + baseLength + 1, path, length);
      fullpath[baseLength + 1 + length] = '\0';

      size_t size;
      void* data = ctx->io(fullpath, &size);
      lovrFree(fullpath);

      if (!data || size <= 0) {
        const char* message = "Unable to read image from ";
        size_t messageLength = strlen(message);
        ctx->error = lovrMalloc(messageLength + length + 1);
        memcpy(ctx->error, message, messageLength);
        memcpy(ctx->error + messageLength, path, length);
        ctx->error[messageLength + length + 1] = '\0';
        atomic_store(&ctx->done, 1);
        return;
      }

      blob = lovrBlobCreate(data, size, NULL);
    }

    ctx->result = lovrImageCreateFromFile(blob);
    lovrRelease(blob, lovrBlobDestroy);
  }

  atomic_store(&ctx->done, 1);
}

static void startImageJob(ModelData* model, ImageJob* jobs, uint32_t index, gltfBufferView* buffers, gltfImage* images, ModelDataIO* io, char* basePath) {
  if (jobs[index].image) {
    return;
  }

  jobs[index].image = &images[index];
  jobs[index].buffers = buffers;
  jobs[index].basePath = basePath;
  jobs[index].io = io;

  if (!job_start(loadImage, &jobs[index])) {
    loadImage(&jobs[index]);
    jobs[index].done = 1;
  }
}

static uint32_t typeSizes[] = {
  [I8] = 1,
  [U8] = 1,
  [I16] = 2,
  [U16] = 2,
  [I32] = 4,
  [U32] = 4,
  [F32] = 4,
  [SN10x3] = 4
};

void copyAttribute(void* destination, gltfAccessor* accessor, ComponentType type, uint32_t components, bool normalized, size_t offset, size_t stride, uint32_t count, uint8_t clear) {
  char* src = accessor ? accessor->data : NULL;
  size_t size = components * typeSizes[type];
  size_t srcStride = accessor && accessor->stride ? accessor->stride : size;
  char* dst = (char*) destination + offset;

  if (!src) {
    if (stride == size) {
      memset(dst, clear, count * size);
    } else {
      for (uint32_t i = 0; i < count; i++, dst += stride) {
        memset(dst, clear, size);
      }
    }
  } else if (accessor->type == type && accessor->components == components && accessor->normalized == normalized && srcStride == stride && stride == size) {
    memcpy(dst, src, count * stride);
  } else if (accessor->type == type && accessor->components >= components) {
    for (uint32_t i = 0; i < count; i++, src += srcStride, dst += stride) {
      memcpy(dst, src, size);
    }
  } else if (type == F32) {
    if (accessor->type == U8 && accessor->normalized) {
      for (uint32_t i = 0; i < count; i++, src += srcStride, dst += stride) {
        for (uint32_t j = 0; j < components; j++) {
          ((float*) dst)[j] = ((uint8_t*) src)[j] / 255.f;
        }
      }
    } else if (accessor->type == U16 && accessor->normalized) {
      for (uint32_t i = 0; i < count; i++, src += srcStride, dst += stride) {
        for (uint32_t j = 0; j < components; j++) {
          ((float*) dst)[j] = ((uint16_t*) src)[j] / 65535.f;
        }
      }
    } else {
      lovrUnreachable();
    }
  } else if (type == U8) {
    if (accessor->type == U16 && accessor->normalized && normalized) {
      for (uint32_t i = 0; i < count; i++, src += srcStride, dst += stride) {
        for (uint32_t j = 0; j < components; j++) {
          ((uint8_t*) dst)[j] = ((uint16_t*) src)[j] >> 8;
        }
        if (components == 4 && accessor->components == 3) {
          ((uint8_t*) dst)[3] = 255;
        }
      }
    } else if (accessor->type == U16 && !accessor->normalized && !normalized) {
      for (uint32_t i = 0; i < count; i++, src += srcStride, dst += stride) {
        for (uint32_t j = 0; j < components; j++) {
          ((uint8_t*) dst)[j] = (uint8_t) ((uint16_t*) src)[j];
        }
      }
    } else if (accessor->type == I16 && !accessor->normalized && !normalized) {
      for (uint32_t i = 0; i < count; i++, src += srcStride, dst += stride) {
        for (uint32_t j = 0; j < components; j++) {
          ((uint8_t*) dst)[j] = (uint8_t) ((int16_t*) src)[j];
        }
      }
    } else if (accessor->type == F32 && normalized) {
      for (uint32_t i = 0; i < count; i++, src += srcStride, dst += stride) {
        for (uint32_t j = 0; j < components; j++) {
          ((uint8_t*) dst)[j] = ((float*) src)[j] * 255.f + .5f;
        }
        if (components == 4 && accessor->components == 3) {
          ((uint8_t*) dst)[3] = 255;
        }
      }
    } else {
      lovrUnreachable();
    }
  } else if (type == U16) {
    if (accessor->type == U8 && !accessor->normalized && !normalized) {
      for (uint32_t i = 0; i < count; i++, src += srcStride, dst += stride) {
        *((uint16_t*) dst) = *(uint8_t*) src;
      }
    } else if (accessor->type == U8 && accessor->normalized && normalized) {
      for (uint32_t i = 0; i < count; i++, src += srcStride, dst += stride) {
        for (uint32_t j = 0; j < components; j++) {
          ((uint16_t*) dst)[j] = ((uint16_t) ((uint8_t*) src)[j]) * 257; // Yes, 257
        }
      }
    } else if (accessor->type == F32 && normalized) {
      for (uint32_t i = 0; i < count; i++, src += srcStride, dst += stride) {
        for (uint32_t j = 0; j < components; j++) {
          ((uint16_t*) dst)[j] = (uint16_t) (((float*) src)[j] * 65535.f + .5f);
        }
      }
    } else {
      lovrUnreachable();
    }
  } else if (type == U32 && components == 1 && !normalized && !accessor->normalized) {
    if (accessor->type == U8) {
      for (uint32_t i = 0; i < count; i++, src += srcStride, dst += stride) {
        *((uint32_t*) dst) = *(uint8_t*) src;
      }
    } else if (accessor->type == U16) {
      for (uint32_t i = 0; i < count; i++, src += srcStride, dst += stride) {
        *((uint32_t*) dst) = *(uint16_t*) src;
      }
    } else {
      lovrUnreachable();
    }
  } else if (type == SN10x3) {
    if (accessor->type == F32) {
      for (uint32_t i = 0; i < count; i++, src += srcStride, dst += stride) {
        float x = ((float*) src)[0];
        float y = ((float*) src)[1];
        float z = ((float*) src)[2];
        float w = accessor->components == 4 ? ((float*) src)[3] : 0.f;
        *(uint32_t*) dst =
          ((((uint32_t) (int32_t) (x * 511.f)) & 0x3ff) <<  0) |
          ((((uint32_t) (int32_t) (y * 511.f)) & 0x3ff) << 10) |
          ((((uint32_t) (int32_t) (z * 511.f)) & 0x3ff) << 20) |
          ((((uint32_t) (int32_t) (w * 2.f)) & 0x003) << 30);
      }
    } else {
      lovrUnreachable();
    }
  } else {
    lovrUnreachable();
  }
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
  ModelMetadata* meta = &model->meta;
  model->ref = 1;

  // Prepass: Basically we iterate over the tokens once and figure out how much memory we need and
  // record the locations of tokens that we'll use later to fill in the memory once it's allocated.

  struct {
    jsmntok_t* buffers;
    jsmntok_t* bufferViews;
    jsmntok_t* accessors;
    jsmntok_t* animations;
    jsmntok_t* materials;
    jsmntok_t* meshes;
    jsmntok_t* nodes;
    jsmntok_t* scenes;
    jsmntok_t* skins;
    int sceneCount;
  } info;

  memset(&info, 0, sizeof(info));

  Blob** blobs = NULL;
  gltfBufferView* buffers = NULL;
  gltfAccessor* accessors = NULL;
  gltfAnimationSampler* animationSamplers = NULL;
  gltfImage* images = NULL;
  ImageJob* imageJobs = NULL;
  gltfTexture* textures = NULL;
  uint32_t* meshVertexCounts = NULL;
  gltfScene* scenes = NULL;
  int animationSamplerCount = 0;
  int rootScene = 0;

  meta->commentLength = jsonLength;

  for (jsmntok_t* token = tokens + 1; token < tokens + tokenCount;) {
    gltfString key = NOM_STR(json, token);

    if (STR_EQ(key, "accessors")) {
      info.accessors = token;
      accessors = lovrCalloc(token->size * sizeof(gltfAccessor));
      gltfAccessor* accessor = accessors;
      for (int i = (token++)->size; i > 0; i--, accessor++) {
        for (int k = (token++)->size; k > 0; k--) {
          gltfString key = NOM_STR(json, token);
          if (STR_EQ(key, "bufferView")) { accessor->bufferView = NOM_U32(json, token); }
          else if (STR_EQ(key, "count")) { accessor->count = NOM_U32(json, token); }
          else if (STR_EQ(key, "byteOffset")) { accessor->offset = NOM_U32(json, token); }
          else if (STR_EQ(key, "normalized")) { accessor->normalized = NOM_BOOL(json, token); }
          else if (STR_EQ(key, "componentType")) {
            switch (NOM_U32(json, token)) {
              case 5120: accessor->type = I8; break;
              case 5121: accessor->type = U8; break;
              case 5122: accessor->type = I16; break;
              case 5123: accessor->type = U16; break;
              case 5125: accessor->type = U32; break;
              case 5126: accessor->type = F32; break;
              default: break;
            }
          } else if (STR_EQ(key, "type")) {
            gltfString type = NOM_STR(json, token);
            if (STR_EQ(type, "SCALAR")) {
              accessor->components = 1;
            } else if (type.length == 4) {
              accessor->components = type.data[3] - '0';
              accessor->matrix = type.data[0] == 'M';
            }
          } else if (STR_EQ(key, "min") && token->size <= 4) {
            int n = (token++)->size;
            accessor->hasMin = true;
            for (int j = 0; j < n && j < 4; j++) {
              accessor->min[j] = NOM_FLOAT(json, token);
            }
          } else if (STR_EQ(key, "max") && token->size <= 4) {
            int n = (token++)->size;
            accessor->hasMax = true;
            for (int j = 0; j < n && j < 4; j++) {
              accessor->max[j] = NOM_FLOAT(json, token);
            }
          } else {
            token = NOM(token);
          }
        }
      }

    } else if (STR_EQ(key, "animations")){
      info.animations = token;
      meta->animationCount = token->size;
      jsmntok_t* t = token;
      for (int i = (t++)->size; i > 0; i--) {
        if (t->size > 0) {
          for (int k = (t++)->size; k > 0; k--) {
            gltfString key = NOM_STR(json, t);
            if (STR_EQ(key, "channels")) { meta->channelCount += t->size; }
            else if (STR_EQ(key, "samplers")) { animationSamplerCount += t->size; }
            else if (STR_EQ(key, "name")) { meta->charCount += t->end - t->start + 1; }
            t = NOM(t);
          }
        }
      }

      animationSamplers = lovrMalloc(animationSamplerCount * sizeof(gltfAnimationSampler));
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
                  else { lovrAssertGoto(fail, false, "Unknown animation sampler interpolation"); }
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
      blobs = lovrCalloc(token->size * sizeof(Blob*));
      token = NOM(token);

    } else if (STR_EQ(key, "bufferViews")) {
      info.bufferViews = token;
      buffers = lovrCalloc(token->size * sizeof(gltfBufferView));
      token = NOM(token);

    } else if (STR_EQ(key, "images")) {
      meta->imageCount = token->size;
      images = lovrMalloc(meta->imageCount * sizeof(gltfImage));
      imageJobs = lovrCalloc(meta->imageCount * sizeof(ImageJob));
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
        lovrAssertGoto(fail, image->bufferView != ~0u || image->uri.data, "Image is missing data");
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
        lovrAssertGoto(fail, texture->image != ~0u, "Texture is missing an image (maybe an unsupported extension is used?)");
      }

    } else if (STR_EQ(key, "materials")) {
      info.materials = token;
      meta->materialCount = token->size;
      for (int i = (token++)->size; i > 0; i--) {
        for (int k = (token++)->size; k > 0; k--) {
          gltfString key = NOM_STR(json, token);
          if (STR_EQ(key, "name")) { meta->charCount += token->end - token->start + 1; }
          token = NOM(token);
        }
      }

    } else if (STR_EQ(key, "meshes")) {
      info.meshes = token;
      meta->meshCount = token->size;
      meshVertexCounts = lovrCalloc(meta->meshCount * sizeof(uint32_t));
      token = NOM(token);

    } else if (STR_EQ(key, "nodes")) {
      info.nodes = token;
      meta->nodeCount = token->size;
      for (int i = (token++)->size; i > 0; i--) {
        if (token->size > 0) {
          for (int k = (token++)->size; k > 0; k--) {
            gltfString key = NOM_STR(json, token);
            if (STR_EQ(key, "name")) { meta->charCount += token->end - token->start + 1; }
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
        scene->node = ~0u;
        scene->nodeCount = 0;
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
      meta->skinCount = token->size;
      for (int i = (token++)->size; i > 0; i--) {
        for (int k = (token++)->size; k > 0; k--) {
          gltfString key = NOM_STR(json, token);
          if (STR_EQ(key, "joints")) {
            meta->jointCount += token->size;
            lovrAssertGoto(fail, token->size < 256, "Currently the max number of joints per skin is 256");
          }
          token = NOM(token);
        }
      }

    } else {
      token = NOM(token);
    }
  }

  // Iterate over meshes and tally up vertex/index/blendshape counts (now that we have accessors)
  if (info.meshes) {
    jsmntok_t* token = info.meshes;
    for (int i = (token++)->size, index = 0; i > 0; i--, index++) {
      uint32_t blendShapeCount = 0;
      uint32_t maxUnindexedVertexCount = 0;
      bool indexed = false;
      for (int k = (token++)->size; k > 0; k--) {
        gltfString key = NOM_STR(json, token);
        if (STR_EQ(key, "primitives")) {
          meta->partCount += token->size;
          for (int p = (token++)->size; p > 0; p--) {
            uint32_t vertexCount = 0;
            uint32_t indexCount = 0;
            bool hasJoints = false;
            for (int k2 = (token++)->size; k2 > 0; k2--) {
              gltfString key = NOM_STR(json, token);
              if (STR_EQ(key, "attributes")) {
                for (int a = (token++)->size; a > 0; a--) {
                  gltfString name = NOM_STR(json, token);
                  uint32_t index = NOM_U32(json, token);
                  if (STR_EQ(name, "POSITION")) vertexCount = accessors[index].count;
                  else if (STR_EQ(name, "JOINTS_0")) hasJoints = true;
                }
              } else if (STR_EQ(key, "indices")) {
                uint32_t index = NOM_U32(json, token);
                indexCount = accessors[index].count;
                meta->indexSize = MAX(meta->indexSize, typeSizes[accessors[index].type]);
                meta->indexSize = MAX(meta->indexSize, 2); // U8 indices aren't supported
                indexed = true;
              } else if (STR_EQ(key, "targets")) {
                lovrAssertGoto(fail, blendShapeCount == 0 || blendShapeCount == token->size, "Model has inconsistent blend shape counts");
                blendShapeCount = token->size;
                token = NOM(token);
              } else {
                token = NOM(token);
              }
            }
            meshVertexCounts[index] += vertexCount;
            meta->vertexCount += vertexCount;
            meta->indexCount += indexCount;
            meta->skinnedVertexCount += hasJoints ? vertexCount : 0;
            meta->blendedVertexCount += vertexCount * blendShapeCount;
            meta->animatedVertexCount += (hasJoints || blendShapeCount > 0) ? vertexCount : 0;
            if (indexCount == 0) maxUnindexedVertexCount = MAX(maxUnindexedVertexCount, vertexCount);
          }
        } else if (STR_EQ(key, "extras")) {
          for (int k2 = (token++)->size; k2 > 0; k2--) {
            gltfString key = NOM_STR(json, token);
            if (STR_EQ(key, "targetNames")) {
              lovrAssertGoto(fail, blendShapeCount == 0 || blendShapeCount == token->size, "Model has inconsistent blend shape counts");
              blendShapeCount = token->size;
              for (int j = (token++)->size; j > 0; j--) {
                meta->charCount += token->end - token->start + 1;
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
      meta->blendShapeCount += blendShapeCount;
      // If any primitives are indexed, we generate indices for any non-indexed primitives in the
      // mesh.  If any of those dummy indices have a value > 65536, we need to upgrade to 32-bit indices.
      if (indexed && maxUnindexedVertexCount > UINT16_MAX) {
        meta->indexSize = 4;
      }
    }
  }

  // Count keyframes
  for (int i = 0; i < animationSamplerCount; i++) {
    gltfAccessor* times = &accessors[animationSamplers[i].input];
    gltfAccessor* values = &accessors[animationSamplers[i].output];
    meta->keyframeDataCount += times->count;
    meta->keyframeDataCount += values->count * values->components;
  }

  // We only support a single root node, so if there is more than one root node in the scene then
  // we create a fake "super root" node and add all the scene's root nodes as its children.
  if (info.sceneCount > 0 && scenes[rootScene].nodeCount > 1) {
    meta->nodeCount++;
  }

  // Allocate memory, then revisit all of the tokens that were recorded during the prepass and write
  // their data into this memory.
  lovrModelDataAllocate(model);

  memcpy(meta->comment, json, jsonLength);

  // Blobs
  if (info.buffers) {
    jsmntok_t* token = info.buffers;
    Blob** blob = blobs;
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
          lovrAssertGoto(fail, bufferData && decodedLength == size, "Could not decode base64 buffer");
          *blob = lovrBlobCreate(bufferData, size, NULL);
        } else {
          size_t bytesRead;
          lovrAssertGoto(fail, uri.length < maxPathLength, "Buffer filename is too long");
          lovrAssertGoto(fail, uri.data[0] != '/', "Absolute paths in models are not supported");
          if (uri.data[0] && uri.data[1] && !memcmp(uri.data, "./", 2)) uri.data += 2;
          strncat(filename, uri.data, uri.length);
          void* data = io(filename, &bytesRead);
          lovrAssertGoto(fail, data && bytesRead == size, "Unable to read '%s'", filename);
          *blob = lovrBlobCreate(data, size, NULL);
          *root = '\0';
        }
      } else {
        lovrAssertGoto(fail, glb, "Buffer is missing URI");
        lovrRetain(source);
        *blob = source;
      }
    }
  }

  // Buffer views
  if (info.bufferViews) {
    jsmntok_t* token = info.bufferViews;
    gltfBufferView* buffer = buffers;
    for (int i = (token++)->size; i > 0; i--, buffer++) {
      for (int k = (token++)->size; k > 0; k--) {
        gltfString key = NOM_STR(json, token);
        if (STR_EQ(key, "buffer")) { buffer->blob = NOM_U32(json, token); }
        else if (STR_EQ(key, "byteOffset")) { buffer->offset = NOM_U32(json, token); }
        else if (STR_EQ(key, "byteLength")) { buffer->size = NOM_U32(json, token); }
        else if (STR_EQ(key, "byteStride")) { buffer->stride = NOM_U32(json, token); }
        else { token = NOM(token); }
      }

      Blob* blob = blobs[buffer->blob];

      // If this is the glb binary data, increment the offset to account for the file header
      if (glb && blob == source) {
        buffer->offset += binOffset;
      }

      buffer->data = (char*) blob->data + buffer->offset;
    }
  }

  // Accessors
  if (info.accessors) {
    for (int i = 0; i < info.accessors->size; i++) {
      gltfAccessor* accessor = &accessors[i];
      gltfBufferView* buffer = &buffers[accessor->bufferView];
      accessors[i].data = buffer->data + accessor->offset;
      accessors[i].stride = buffer->stride ? buffer->stride : (typeSizes[accessor->type] * accessor->components);
    }
  }

  // Animations
  if (meta->animationCount > 0) {
    int channelIndex = 0;
    int baseSampler = 0;
    jsmntok_t* token = info.animations;
    float* keyframeData = meta->keyframeData;
    ModelAnimation* animation = meta->animations;
    for (int i = (token++)->size; i > 0; i--, animation++) {
      int samplerCount = 0;
      for (int k = (token++)->size; k > 0; k--) {
        gltfString key = NOM_STR(json, token);
        if (STR_EQ(key, "channels")) {
          animation->channelCount = (token++)->size;
          animation->channels = meta->channels + channelIndex;
          channelIndex += animation->channelCount;
          for (uint32_t j = 0; j < animation->channelCount; j++) {
            ModelAnimationChannel* channel = &animation->channels[j];

            for (int k2 = (token++)->size; k2 > 0; k2--) {
              gltfString key = NOM_STR(json, token);
              if (STR_EQ(key, "sampler")) {
                gltfAnimationSampler* sampler = animationSamplers + baseSampler + NOM_U32(json, token);

                gltfAccessor* times = &accessors[sampler->input];
                channel->times = keyframeData;
                copyAttribute(keyframeData, times, F32, 1, false, 0, 4, times->count, 0);
                animation->duration = MAX(animation->duration, channel->times[times->count - 1]);
                keyframeData += times->count;

                gltfAccessor* values = &accessors[sampler->output];
                channel->data = keyframeData;
                copyAttribute(keyframeData, values, F32, values->components, false, 0, values->components * 4, values->count, 0);
                keyframeData += values->count * values->components;

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
                    else { lovrAssertGoto(fail, false, "Unknown animation channel property"); }
                  } else {
                    token = NOM(token);
                  }
                }
              } else {
                token = NOM(token);
              }
            }
          }
        } else if (STR_EQ(key, "samplers")) {
          samplerCount = token->size;
          token = NOM(token);
        } else if (STR_EQ(key, "name")) {
          gltfString name = NOM_STR(json, token);
          meta->animationLookup[animation - meta->animations] = (uint32_t) hash64(name.data, name.length);
          memcpy(meta->chars, name.data, name.length);
          animation->name = meta->chars;
          meta->chars += name.length + 1;
        } else {
          token = NOM(token);
        }
      }
      baseSampler += samplerCount;
    }
  }

  // Materials
  if (meta->materialCount > 0) {
    jsmntok_t* token = info.materials;
    ModelMaterial* material = meta->materials;
    for (int i = (token++)->size; i > 0; i--, material++) {
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
              startImageJob(model, imageJobs, material->texture, buffers, images, io, filename);
              *root = '\0';
            } else if (STR_EQ(key, "metallicFactor")) {
              material->metalness = NOM_FLOAT(json, token);
            } else if (STR_EQ(key, "roughnessFactor")) {
              material->roughness = NOM_FLOAT(json, token);
            } else if (STR_EQ(key, "metallicRoughnessTexture")) {
              token = nomTexture(json, token, &material->metalnessTexture, textures, NULL);
              startImageJob(model, imageJobs, material->metalnessTexture, buffers, images, io, filename);
              material->roughnessTexture = material->metalnessTexture;
              *root = '\0';
            } else {
              token = NOM(token);
            }
          }
        } else if (STR_EQ(key, "normalTexture")) {
          token = nomTexture(json, token, &material->normalTexture, textures, NULL);
          startImageJob(model, imageJobs, material->normalTexture, buffers, images, io, filename);
          *root = '\0';
        } else if (STR_EQ(key, "occlusionTexture")) {
          token = nomTexture(json, token, &material->occlusionTexture, textures, NULL);
          startImageJob(model, imageJobs, material->occlusionTexture, buffers, images, io, filename);
          *root = '\0';
        } else if (STR_EQ(key, "emissiveTexture")) {
          token = nomTexture(json, token, &material->glowTexture, textures, NULL);
          startImageJob(model, imageJobs, material->glowTexture, buffers, images, io, filename);
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
          meta->materialLookup[material - meta->materials] = (uint32_t) hash64(name.data, name.length);
          memcpy(meta->chars, name.data, name.length);
          material->name = meta->chars;
          meta->chars += name.length + 1;
        } else {
          token = NOM(token);
        }
      }
    }
  }

  // Meshes
  if (meta->meshCount > 0) {
    jsmntok_t* token = info.meshes;
    ModelMesh* mesh = meta->meshes;
    ModelPart* part = meta->parts;
    uint32_t vertexOffset = 0;
    uint32_t indexOffset = 0;
    uint32_t skinDataOffset = 0;
    uint32_t blendDataOffset = 0;
    uint32_t blendShapeIndex = 0;
    ModelBlendShape* blendShapes = meta->blendShapes;
    for (int i = (token++)->size; i > 0; i--, mesh++) {
      mesh->parts = part;
      mesh->blendShapes = blendShapes;
      for (int k = (token++)->size; k > 0; k--) {
        gltfString key = NOM_STR(json, token);
        if (STR_EQ(key, "primitives")) {
          for (uint32_t j = (token++)->size; j > 0; j--, part++) {
            gltfAccessor* positions = NULL;
            gltfAccessor* normals = NULL;
            gltfAccessor* uv1 = NULL;
            gltfAccessor* uv2 = NULL;
            gltfAccessor* colors = NULL;
            gltfAccessor* tangents = NULL;
            gltfAccessor* indices = NULL;
            gltfAccessor* joints = NULL;
            gltfAccessor* weights = NULL;

            for (int k2 = (token++)->size; k2 > 0; k2--) {
              gltfString key = NOM_STR(json, token);
              if (STR_EQ(key, "mode")) {
                switch (NOM_U32(json, token)) {
                  case 0: part->mode = DRAW_POINT_LIST; break;
                  case 1: part->mode = DRAW_LINE_LIST; break;
                  case 2: part->mode = DRAW_LINE_LOOP; break;
                  case 3: part->mode = DRAW_LINE_STRIP; break;
                  case 4: part->mode = DRAW_TRIANGLE_LIST; break;
                  case 5: part->mode = DRAW_TRIANGLE_STRIP; break;
                  case 6: part->mode = DRAW_TRIANGLE_FAN; break;
                  default: lovrAssertGoto(fail, false, "Unknown primitive mode");
                }
              } else if (STR_EQ(key, "attributes")) {
                for (int a = (token++)->size; a > 0; a--) {
                  gltfString name = NOM_STR(json, token);
                  uint32_t accessor = NOM_U32(json, token);
                  if (STR_EQ(name, "POSITION")) positions = &accessors[accessor];
                  else if (STR_EQ(name, "NORMAL")) normals = &accessors[accessor];
                  else if (STR_EQ(name, "TEXCOORD_0")) uv1 = &accessors[accessor];
                  else if (STR_EQ(name, "TEXCOORD_1")) uv2 = &accessors[accessor];
                  else if (STR_EQ(name, "COLOR_0")) colors = &accessors[accessor];
                  else if (STR_EQ(name, "TANGENT")) tangents = &accessors[accessor];
                  else if (STR_EQ(name, "JOINTS_0")) joints = &accessors[accessor];
                  else if (STR_EQ(name, "WEIGHTS_0")) weights = &accessors[accessor];
                }
              } else if (STR_EQ(key, "indices")) {
                indices = &accessors[NOM_U32(json, token)];
              } else if (STR_EQ(key, "targets")) {
                if (mesh->blendShapeCount == 0) {
                  mesh->blendShapeCount = token->size;
                  mesh->blendDataOffset = blendDataOffset;
                }

                for (int t = (token++)->size, index = 0; t > 0; t--, index++) {
                  gltfAccessor* blendPositions = NULL;
                  gltfAccessor* blendNormals = NULL;
                  gltfAccessor* blendTangents = NULL;
                  uint32_t count = 0;

                  for (int a = (token++)->size; a > 0; a--) {
                    gltfString name = NOM_STR(json, token);
                    uint32_t accessor = NOM_U32(json, token);
                    if (STR_EQ(name, "POSITION")) blendPositions = &accessors[accessor];
                    else if (STR_EQ(name, "NORMAL")) blendNormals = &accessors[accessor];
                    else if (STR_EQ(name, "TANGENT")) blendTangents = &accessors[accessor];
                    count = accessors[accessor].count;
                  }

                  // Blend shape data must be deinterleaved because model->blendData is grouped by
                  // blend shape, not by primitive
                  uint32_t offset = mesh->vertexCount; // The vertex count so far
                  uint32_t stride = meshVertexCounts[mesh - meta->meshes]; // Total number of vertices in mesh
                  BlendData* blendData = model->blendData + mesh->blendDataOffset + index * stride + offset;
                  copyAttribute(blendData, blendPositions, F32, 3, false, 0, sizeof(BlendData), count, 0);
                  copyAttribute(blendData, blendNormals, F32, 3, false, 12, sizeof(BlendData), count, 0);
                  copyAttribute(blendData, blendTangents, F32, 3, false, 24, sizeof(BlendData), count, 0);
                  blendDataOffset += blendPositions->count;
                }
              } else if (STR_EQ(key, "material")) {
                part->material = NOM_U32(json, token);
              } else {
                token = NOM(token);
              }
            }

            if (positions) {
              if (mesh->vertexOffset == ~0u) mesh->vertexOffset = vertexOffset;
              if (indices && mesh->indexOffset == ~0u) mesh->indexOffset = indexOffset;

              if (indices || mesh->indexCount > 0) {
                part->start = indexOffset - mesh->indexOffset;
                part->count = indices ? indices->count : positions->count;
                part->baseVertex = vertexOffset - mesh->vertexOffset;
              } else {
                part->start = vertexOffset - mesh->vertexOffset;
                part->count = positions->count;
              }

              uint32_t vertexCount = positions->count;
              ModelVertex* vertices = model->vertices + vertexOffset;
              copyAttribute(vertices, positions, F32, 3, false, offsetof(ModelVertex, position), sizeof(ModelVertex), vertexCount, 0);
#ifdef LOVR_WEBGPU
              copyAttribute(vertices, normals, F32, 3, false, offsetof(ModelVertex, normal), sizeof(ModelVertex), vertexCount, 0);
#else
              copyAttribute(vertices, normals, SN10x3, 1, false, offsetof(ModelVertex, normal), sizeof(ModelVertex), vertexCount, 0);
#endif
              copyAttribute(vertices, uv1, F32, 2, false, offsetof(ModelVertex, uv), sizeof(ModelVertex), vertexCount, 0);
              copyAttribute(vertices, uv2, U16, 2, true, offsetof(ModelVertex, uv2), sizeof(ModelVertex), vertexCount, 0);
              copyAttribute(vertices, colors, U8, 4, true, offsetof(ModelVertex, color), sizeof(ModelVertex), vertexCount, 0xff);
#ifdef LOVR_WEBGPU
              copyAttribute(vertices, tangents, F32, 4, false, offsetof(ModelVertex, tangent), sizeof(ModelVertex), vertexCount, 0);
#else
              copyAttribute(vertices, tangents, SN10x3, 1, false, offsetof(ModelVertex, tangent), sizeof(ModelVertex), vertexCount, 0);
#endif
              mesh->vertexCount += vertexCount;
              vertexOffset += vertexCount;

              // We keep meshes consistently indexed or non-indexed, so we have to generate indices if:
              // - This is the first indexed primitive, and we already wrote non-indexed primitives
              // - This is a non-indexed primitive, and we've already written an indexed primitive
              if ((indices && mesh->indexCount == 0) || (!indices && mesh->indexCount > 0)) {
                uint32_t partIndex = indices ? 0 : part - mesh->parts;
                uint32_t partCount = indices ? mesh->partCount : 1;
                void* indexData = (char*) model->indices + (indexOffset * meta->indexSize);

                for (uint32_t p = 0; p < partCount; p++) {
                  uint32_t count = mesh->parts[partIndex + p].count;
                  if (meta->indexSize == 4) {
                    for (uint32_t index = 0; index < count; index++) {
                      ((uint32_t*) indexData)[index] = index;
                    }
                  } else {
                    for (uint32_t index = 0; index < count; index++) {
                      ((uint16_t*) indexData)[index] = index;
                    }
                  }
                  mesh->parts[partIndex + p].start = indexOffset - mesh->indexOffset;
                  mesh->indexCount += count;
                  indexOffset += count;
                }
              }

              if (indices) {
                uint32_t indexCount = indices->count;
                int type = meta->indexSize == 4 ? U32 : U16;
                void* indexData = (char*) model->indices + (indexOffset * meta->indexSize);
                copyAttribute(indexData, indices, type, 1, false, 0, meta->indexSize, indexCount, 0);
                mesh->indexCount += indexCount;
                indexOffset += indexCount;
              }

              if (joints && weights) {
                SkinData* skinData = model->skinData + skinDataOffset;
                copyAttribute(skinData, joints, U8, 4, false, 0, sizeof(SkinData), vertexCount, 0);
                copyAttribute(skinData, weights, U8, 4, true, 4, sizeof(SkinData), vertexCount, 0);
                if (mesh->skinDataOffset == ~0u) mesh->skinDataOffset = skinDataOffset;
                skinDataOffset += vertexCount;
              }

              part->bounds[0] = positions->min[0];
              part->bounds[1] = positions->max[0];
              part->bounds[2] = positions->min[1];
              part->bounds[3] = positions->max[1];
              part->bounds[4] = positions->min[2];
              part->bounds[5] = positions->max[2];
              mesh->partCount++;
            }
          }
        } else if (STR_EQ(key, "weights")) {
          for (int w = (token++)->size, index = 0; w > 0; w--, index++) {
            meta->blendShapes[index].weight = NOM_FLOAT(json, token);
          }
        } else if (STR_EQ(key, "extras")) {
          for (int k2 = (token++)->size; k2 > 0; k2--) {
            gltfString key = NOM_STR(json, token);
            if (STR_EQ(key, "targetNames")) {
              for (int k3 = (token++)->size, index = 0; k3 > 0; k3--, index++) {
                gltfString name = NOM_STR(json, token);
                uint32_t hash = (uint32_t) hash64(name.data, name.length);
                meta->blendShapeLookup[mesh->blendShapes - meta->blendShapes + index] = hash;
                memcpy(meta->chars, name.data, name.length);
                mesh->blendShapes[index].name = meta->chars;
                meta->chars += name.length + 1;
              }
            } else {
              token = NOM(token);
            }
          }
        } else {
          token = NOM(token);
        }
      }

      blendShapes += mesh->blendShapeCount;
    }
  }

  // Nodes
  if (meta->nodeCount > 0 && info.nodes) {
    jsmntok_t* token = info.nodes;
    ModelNode* node = meta->nodes;
    for (int i = (token++)->size; i > 0; i--, node++) {
      jsmntok_t* weights = NULL;
      for (int k = (token++)->size; k > 0; k--) {
        gltfString key = NOM_STR(json, token);
        if (STR_EQ(key, "mesh")) {
          node->mesh = NOM_U32(json, token);
        } else if (STR_EQ(key, "skin")) {
          node->skin = NOM_U32(json, token);
        } else if (STR_EQ(key, "weights")) {
          weights = token; // Deferred due to order dependency
        } else if (STR_EQ(key, "children")) {
          uint32_t childCount = (token++)->size;
          node->child = NOM_U32(json, token);
          meta->nodes[node->child].parent = node - meta->nodes;
          uint32_t prevChild = node->child;
          for (uint32_t j = 1; j < childCount; j++) {
            uint32_t child = NOM_U32(json, token);
            meta->nodes[prevChild].sibling = child;
            meta->nodes[child].parent = node - meta->nodes;
            prevChild = child;
          }
        } else if (STR_EQ(key, "matrix")) {
          lovrAssertGoto(fail, (token++)->size == 16, "Node matrix needs 16 elements");
          node->hasMatrix = true;
          for (int j = 0; j < 16; j++) {
            node->transform.matrix[j] = NOM_FLOAT(json, token);
          }
        } else if (STR_EQ(key, "translation")) {
          lovrAssertGoto(fail, (token++)->size == 3, "Node translation needs 3 elements");
          node->transform.translation[0] = NOM_FLOAT(json, token);
          node->transform.translation[1] = NOM_FLOAT(json, token);
          node->transform.translation[2] = NOM_FLOAT(json, token);
        } else if (STR_EQ(key, "rotation")) {
          lovrAssertGoto(fail, (token++)->size == 4, "Node rotation needs 4 elements");
          node->transform.rotation[0] = NOM_FLOAT(json, token);
          node->transform.rotation[1] = NOM_FLOAT(json, token);
          node->transform.rotation[2] = NOM_FLOAT(json, token);
          node->transform.rotation[3] = NOM_FLOAT(json, token);
        } else if (STR_EQ(key, "scale")) {
          lovrAssertGoto(fail, (token++)->size == 3, "Node scale needs 3 elements");
          node->transform.scale[0] = NOM_FLOAT(json, token);
          node->transform.scale[1] = NOM_FLOAT(json, token);
          node->transform.scale[2] = NOM_FLOAT(json, token);
        } else if (STR_EQ(key, "name")) {
          gltfString name = NOM_STR(json, token);
          meta->nodeLookup[node - meta->nodes] = (uint32_t) hash64(name.data, name.length);
          memcpy(meta->chars, name.data, name.length);
          node->name = meta->chars;
          meta->chars += name.length + 1;
        } else {
          token = NOM(token);
        }
      }

      if (weights && node->mesh != ~0u) {
        ModelMesh* mesh = &meta->meshes[node->mesh];
        lovrAssertGoto(fail, (uint32_t) weights->size == mesh->blendShapeCount, "Inconsistent blend shape counts");
        for (int w = (weights++)->size, index = 0; w > 0; w--, index++) {
          mesh->blendShapes[index].weight = NOM_FLOAT(json, token);
        }
      }
    }
  }

  // Skins
  if (meta->skinCount > 0) {
    int jointIndex = 0;
    jsmntok_t* token = info.skins;
    ModelSkin* skin = meta->skins;
    float* inverseBindMatrices = meta->inverseBindMatrices;
    for (int i = (token++)->size; i > 0; i--, skin++) {
      for (int k = (token++)->size; k > 0; k--) {
        gltfString key = NOM_STR(json, token);
        if (STR_EQ(key, "inverseBindMatrices")) {
          gltfAccessor* accessor = &accessors[NOM_U32(json, token)];
          skin->inverseBindMatrices = inverseBindMatrices;
          memcpy(skin->inverseBindMatrices, accessor->data, accessor->count * 16 * sizeof(float));
          inverseBindMatrices += accessor->count * 16;
        } else if (STR_EQ(key, "joints")) {
          skin->joints = &meta->joints[jointIndex];
          skin->jointCount = (token++)->size;
          for (uint32_t j = 0; j < skin->jointCount; j++) {
            meta->joints[jointIndex++] = NOM_U32(json, token);
          }
        } else {
          token = NOM(token);
        }
      }
    }
  }

  // Scenes
  if (meta->nodeCount == 0) {
    meta->rootNode = ~0u;
  } else if (info.sceneCount == 0) {
    meta->rootNode = 0;
  } else if (scenes[rootScene].nodeCount > 1) {
    meta->rootNode = meta->nodeCount - 1;
    ModelNode* root = &meta->nodes[meta->rootNode];
    root->skin = ~0u;

    float* matrix = root->transform.matrix;
    memset(matrix, 0, 16 * sizeof(float));
    matrix[0] = matrix[5] = matrix[10] = matrix[15] = 1.f;
    root->hasMatrix = true;

    jsmntok_t* token = info.scenes;
    int sceneCount = (token++)->size;
    for (int i = 0; i < sceneCount; i++) {
      if (i == rootScene) {
        for (int k = (token++)->size; k > 0; k--) {
          gltfString key = NOM_STR(json, token);
          if (STR_EQ(key, "nodes")) {
            uint32_t childCount = (token++)->size;
            root->child = NOM_U32(json, token);
            meta->nodes[root->child].parent = meta->rootNode;
            uint32_t prevChild = root->child;
            for (uint32_t j = 1; j < childCount; j++) {
              uint32_t child = NOM_U32(json, token);
              meta->nodes[prevChild].sibling = child;
              meta->nodes[child].parent = meta->rootNode;
              prevChild = child;
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
    meta->rootNode = scenes[rootScene].node;
  }

  for (uint32_t i = 0; i < meta->imageCount; i++) {
    ImageJob* job = &imageJobs[i];

    if (job->image) {
      while (!atomic_load(&job->done)) {
        job_spin();
      }

      if (job->error) {
        lovrSetError(job->error);
        lovrFree(job->error);
        job->error = NULL;
        goto fail;
      } else {
        model->images[i] = job->result;
        job->result = NULL;
      }
    }
  }

  lovrFree(imageJobs);

  if (info.buffers) {
    for (int i = 0; i < info.buffers->size; i++) {
      lovrRelease(blobs[i], lovrBlobDestroy);
    }
  }

  lovrFree(blobs);
  lovrFree(buffers);
  lovrFree(accessors);
  lovrFree(animationSamplers);
  lovrFree(images);
  lovrFree(textures);
  lovrFree(meshVertexCounts);
  lovrFree(scenes);
  lovrFree(heapTokens);
  *result = model;
  return true;

fail:
  for (uint32_t i = 0; i < meta->imageCount; i++) {
    ImageJob* job = &imageJobs[i];

    while (!atomic_load(&job->done)) {
      job_spin();
    }

    lovrRelease(job->result, lovrImageDestroy);
    lovrFree(job->error);
  }

  lovrFree(imageJobs);

  if (info.buffers) {
    for (int i = 0; i < info.buffers->size; i++) {
      lovrRelease(blobs[i], lovrBlobDestroy);
    }
  }

  lovrFree(blobs);
  lovrFree(buffers);
  lovrFree(accessors);
  lovrFree(animationSamplers);
  lovrFree(images);
  lovrFree(textures);
  lovrFree(meshVertexCounts);
  lovrFree(scenes);
  lovrFree(heapTokens);
  lovrModelDataDestroy(model);
  return false;
}
