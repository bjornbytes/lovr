#include "graphics/opengl.h"
#include "graphics/graphics.h"
#include "graphics/buffer.h"
#include "graphics/canvas.h"
#include "graphics/mesh.h"
#include "graphics/shader.h"
#include "graphics/texture.h"
#include "resources/shaders.h"
#include "data/modelData.h"
#include "lib/math.h"
#include "lib/vec/vec.h"
#include <math.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Types

#define MAX_TEXTURES 16
#define MAX_IMAGES 8
#define MAX_BLOCK_BUFFERS 8

#define LOVR_SHADER_POSITION 0
#define LOVR_SHADER_NORMAL 1
#define LOVR_SHADER_TEX_COORD 2
#define LOVR_SHADER_VERTEX_COLOR 3
#define LOVR_SHADER_TANGENT 4
#define LOVR_SHADER_BONES 5
#define LOVR_SHADER_BONE_WEIGHTS 6
#define LOVR_SHADER_DRAW_ID 7

typedef enum {
  BARRIER_BLOCK,
  BARRIER_UNIFORM_TEXTURE,
  BARRIER_UNIFORM_IMAGE,
  BARRIER_TEXTURE,
  BARRIER_CANVAS,
  MAX_BARRIERS
} Barrier;

typedef struct {
  uint32_t buffer;
  size_t offset;
  size_t size;
} BlockBuffer;

static struct {
  Texture* defaultTexture;
  bool alphaToCoverage;
  bool blendEnabled;
  BlendMode blendMode;
  BlendAlphaMode blendAlphaMode;
  bool culling;
  bool depthEnabled;
  CompareMode depthTest;
  bool depthWrite;
  uint8_t lineWidth;
  uint32_t primitiveRestart;
  bool stencilEnabled;
  CompareMode stencilMode;
  int stencilValue;
  bool stencilWriting;
  Winding winding;
  bool wireframe;
  uint32_t framebuffer;
  uint32_t program;
  uint32_t vertexArray;
  uint32_t buffers[MAX_BUFFER_TYPES];
  BlockBuffer blockBuffers[2][MAX_BLOCK_BUFFERS];
  int activeTexture;
  Texture* textures[MAX_TEXTURES];
  Image images[MAX_IMAGES];
  float viewports[2][4];
  uint32_t viewportCount;
  vec_void_t incoherents[MAX_BARRIERS];
  bool srgb;
  GpuFeatures features;
  GpuLimits limits;
  GpuStats stats;
} state;

// Helper functions

static GLenum convertCompareMode(CompareMode mode) {
  switch (mode) {
    case COMPARE_NONE: return GL_ALWAYS;
    case COMPARE_EQUAL: return GL_EQUAL;
    case COMPARE_NEQUAL: return GL_NOTEQUAL;
    case COMPARE_LESS: return GL_LESS;
    case COMPARE_LEQUAL: return GL_LEQUAL;
    case COMPARE_GREATER: return GL_GREATER;
    case COMPARE_GEQUAL: return GL_GEQUAL;
  }
}

static GLenum convertWrapMode(WrapMode mode) {
  switch (mode) {
    case WRAP_CLAMP: return GL_CLAMP_TO_EDGE;
    case WRAP_REPEAT: return GL_REPEAT;
    case WRAP_MIRRORED_REPEAT: return GL_MIRRORED_REPEAT;
  }
}

static GLenum convertTextureTarget(TextureType type) {
  switch (type) {
    case TEXTURE_2D: return GL_TEXTURE_2D; break;
    case TEXTURE_ARRAY: return GL_TEXTURE_2D_ARRAY; break;
    case TEXTURE_CUBE: return GL_TEXTURE_CUBE_MAP; break;
    case TEXTURE_VOLUME: return GL_TEXTURE_3D; break;
  }
}

static GLenum convertTextureFormat(TextureFormat format) {
  switch (format) {
    case FORMAT_RGB: return GL_RGB;
    case FORMAT_RGBA: return GL_RGBA;
    case FORMAT_RGBA4: return GL_RGBA;
    case FORMAT_RGBA16F: return GL_RGBA;
    case FORMAT_RGBA32F: return GL_RGBA;
    case FORMAT_R16F: return GL_RED;
    case FORMAT_R32F: return GL_RED;
    case FORMAT_RG16F: return GL_RG;
    case FORMAT_RG32F: return GL_RG;
    case FORMAT_RGB5A1: return GL_RGBA;
    case FORMAT_RGB10A2: return GL_RGBA;
    case FORMAT_RG11B10F: return GL_RGB;
    case FORMAT_D16: return GL_DEPTH_COMPONENT;
    case FORMAT_D32F: return GL_DEPTH_COMPONENT;
    case FORMAT_D24S8: return GL_DEPTH_STENCIL;
    case FORMAT_DXT1: return GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
    case FORMAT_DXT3: return GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
    case FORMAT_DXT5: return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
  }
}

static GLenum convertTextureFormatInternal(TextureFormat format, bool srgb) {
  switch (format) {
    case FORMAT_RGB: return srgb ? GL_SRGB8 : GL_RGB8;
    case FORMAT_RGBA: return srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8;
    case FORMAT_RGBA4: return GL_RGBA4;
    case FORMAT_RGBA16F: return GL_RGBA16F;
    case FORMAT_RGBA32F: return GL_RGBA32F;
    case FORMAT_R16F: return GL_R16F;
    case FORMAT_R32F: return GL_R32F;
    case FORMAT_RG16F: return GL_RG16F;
    case FORMAT_RG32F: return GL_RG32F;
    case FORMAT_RGB5A1: return GL_RGB5_A1;
    case FORMAT_RGB10A2: return GL_RGB10_A2;
    case FORMAT_RG11B10F: return GL_R11F_G11F_B10F;
    case FORMAT_D16: return GL_DEPTH_COMPONENT16;
    case FORMAT_D32F: return GL_DEPTH_COMPONENT32F;
    case FORMAT_D24S8: return GL_DEPTH24_STENCIL8;
    case FORMAT_DXT1: return srgb ? GL_COMPRESSED_SRGB_S3TC_DXT1_EXT : GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
    case FORMAT_DXT3: return srgb ? GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT : GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
    case FORMAT_DXT5: return srgb ? GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT : GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
  }
}

static GLenum convertTextureFormatType(TextureFormat format) {
  switch (format) {
    case FORMAT_RGB: return GL_UNSIGNED_BYTE;
    case FORMAT_RGBA: return GL_UNSIGNED_BYTE;
    case FORMAT_RGBA4: return GL_UNSIGNED_SHORT_4_4_4_4;
    case FORMAT_RGBA16F: return GL_HALF_FLOAT;
    case FORMAT_RGBA32F: return GL_FLOAT;
    case FORMAT_R16F: return GL_HALF_FLOAT;
    case FORMAT_R32F: return GL_FLOAT;
    case FORMAT_RG16F: return GL_HALF_FLOAT;
    case FORMAT_RG32F: return GL_FLOAT;
    case FORMAT_RGB5A1: return GL_UNSIGNED_SHORT_5_5_5_1;
    case FORMAT_RGB10A2: return GL_UNSIGNED_INT_2_10_10_10_REV;
    case FORMAT_RG11B10F: return GL_UNSIGNED_INT_10F_11F_11F_REV;
    case FORMAT_D16: return GL_UNSIGNED_SHORT;
    case FORMAT_D32F: return GL_UNSIGNED_INT;
    case FORMAT_D24S8: return GL_UNSIGNED_INT_24_8;
    case FORMAT_DXT1:
    case FORMAT_DXT3:
    case FORMAT_DXT5:
      lovrThrow("Unreachable");
      return GL_UNSIGNED_BYTE;
  }
}

static bool isTextureFormatCompressed(TextureFormat format) {
  switch (format) {
    case FORMAT_DXT1: case FORMAT_DXT3: case FORMAT_DXT5: return true;
    default: return false;
  }
}

static bool isTextureFormatDepth(TextureFormat format) {
  switch (format) {
    case FORMAT_D16: case FORMAT_D32F: case FORMAT_D24S8: return true;
    default: return false;
  }
}

static GLenum convertBufferType(BufferType type) {
  switch (type) {
    case BUFFER_VERTEX: return GL_ARRAY_BUFFER;
    case BUFFER_INDEX: return GL_ELEMENT_ARRAY_BUFFER;
    case BUFFER_UNIFORM: return GL_UNIFORM_BUFFER;
    case BUFFER_SHADER_STORAGE: return GL_SHADER_STORAGE_BUFFER;
    case BUFFER_GENERIC: return GL_COPY_WRITE_BUFFER;
    default: lovrThrow("Unreachable"); return 0;
  }
}

static GLenum convertBufferUsage(BufferUsage usage) {
  switch (usage) {
    case USAGE_STATIC: return GL_STATIC_DRAW;
    case USAGE_DYNAMIC: return GL_DYNAMIC_DRAW;
    case USAGE_STREAM: return GL_STREAM_DRAW;
  }
}

#ifndef EMSCRIPTEN
static GLenum convertAccess(UniformAccess access) {
  switch (access) {
    case ACCESS_READ: return GL_READ_ONLY;
    case ACCESS_WRITE: return GL_WRITE_ONLY;
    case ACCESS_READ_WRITE: return GL_READ_WRITE;
  }
}
#endif

static GLenum convertDrawMode(DrawMode mode) {
  switch (mode) {
    case DRAW_POINTS: return GL_POINTS;
    case DRAW_LINES: return GL_LINES;
    case DRAW_LINE_STRIP: return GL_LINE_STRIP;
    case DRAW_LINE_LOOP: return GL_LINE_LOOP;
    case DRAW_TRIANGLE_STRIP: return GL_TRIANGLE_STRIP;
    case DRAW_TRIANGLES: return GL_TRIANGLES;
    case DRAW_TRIANGLE_FAN: return GL_TRIANGLE_FAN;
  }
}

static UniformType getUniformType(GLenum type, const char* debug) {
  switch (type) {
    case GL_FLOAT:
    case GL_FLOAT_VEC2:
    case GL_FLOAT_VEC3:
    case GL_FLOAT_VEC4:
      return UNIFORM_FLOAT;
    case GL_INT:
    case GL_INT_VEC2:
    case GL_INT_VEC3:
    case GL_INT_VEC4:
      return UNIFORM_INT;
    case GL_FLOAT_MAT2:
    case GL_FLOAT_MAT3:
    case GL_FLOAT_MAT4:
      return UNIFORM_MATRIX;
    case GL_SAMPLER_2D:
    case GL_SAMPLER_3D:
    case GL_SAMPLER_CUBE:
    case GL_SAMPLER_2D_ARRAY:
      return UNIFORM_SAMPLER;
#ifdef GL_ARB_shader_image_load_store
    case GL_IMAGE_2D:
    case GL_IMAGE_3D:
    case GL_IMAGE_CUBE:
    case GL_IMAGE_2D_ARRAY:
      return UNIFORM_IMAGE;
#endif
    default:
      lovrThrow("Unsupported uniform type for uniform '%s'", debug);
      return UNIFORM_FLOAT;
  }
}

static int getUniformComponents(GLenum type) {
  switch (type) {
    case GL_FLOAT_VEC2: case GL_INT_VEC2: case GL_FLOAT_MAT2: return 2;
    case GL_FLOAT_VEC3: case GL_INT_VEC3: case GL_FLOAT_MAT3: return 3;
    case GL_FLOAT_VEC4: case GL_INT_VEC4: case GL_FLOAT_MAT4: return 4;
    default: return 1;
  }
}

static TextureType getUniformTextureType(GLenum type) {
  switch (type) {
    case GL_SAMPLER_2D: return TEXTURE_2D;
    case GL_SAMPLER_3D: return TEXTURE_VOLUME;
    case GL_SAMPLER_CUBE: return TEXTURE_CUBE;
    case GL_SAMPLER_2D_ARRAY: return TEXTURE_ARRAY;
#ifdef GL_ARB_shader_image_load_store
    case GL_IMAGE_2D: return TEXTURE_2D;
    case GL_IMAGE_3D: return TEXTURE_VOLUME;
    case GL_IMAGE_CUBE: return TEXTURE_CUBE;
    case GL_IMAGE_2D_ARRAY: return TEXTURE_ARRAY;
#endif
    default: return -1;
  }
}

// TODO really ought to have TextureType-specific default textures
static Texture* lovrGpuGetDefaultTexture() {
  if (!state.defaultTexture) {
    TextureData* textureData = lovrTextureDataCreate(1, 1, 0xff, FORMAT_RGBA);
    state.defaultTexture = lovrTextureCreate(TEXTURE_2D, &textureData, 1, true, false, 0);
    lovrRelease(textureData);
  }

  return state.defaultTexture;
}

// Syncing resources is only relevant for compute shaders
#ifndef EMSCRIPTEN
static void lovrGpuSync(uint8_t flags) {
  if (!flags) {
    return;
  }

  GLbitfield bits = 0;
  for (int i = 0; i < MAX_BARRIERS; i++) {
    if (!((flags >> i) & 1)) {
      continue;
    }

    if (state.incoherents[i].length == 0) {
      flags &= ~(1 << i);
      continue;
    }

    if (i == BARRIER_BLOCK) {
      for (int j = 0; j < state.incoherents[i].length; j++) {
        Buffer* buffer = state.incoherents[i].data[j];
        buffer->incoherent &= ~(1 << i);
      }
    } else {
      for (int j = 0; j < state.incoherents[i].length; j++) {
        Texture* texture = state.incoherents[i].data[j];
        texture->incoherent &= ~(1 << i);
      }
    }

    vec_clear(&state.incoherents[i]);

    switch (i) {
      case BARRIER_BLOCK: bits |= GL_SHADER_STORAGE_BARRIER_BIT; break;
      case BARRIER_UNIFORM_IMAGE: bits |= GL_SHADER_IMAGE_ACCESS_BARRIER_BIT; break;
      case BARRIER_UNIFORM_TEXTURE: bits |= GL_TEXTURE_FETCH_BARRIER_BIT; break;
      case BARRIER_TEXTURE: bits |= GL_TEXTURE_UPDATE_BARRIER_BIT; break;
      case BARRIER_CANVAS: bits |= GL_FRAMEBUFFER_BARRIER_BIT; break;
    }
  }

  if (bits) {
    glMemoryBarrier(bits);
  }
}
#endif

static void lovrGpuDestroySyncResource(void* resource, uint8_t incoherent) {
  if (!incoherent) {
    return;
  }

  for (int i = 0; i < MAX_BARRIERS; i++) {
    if (incoherent & (1 << i)) {
      for (int j = 0; j < state.incoherents[i].length; j++) {
        if (state.incoherents[i].data[j] == resource) {
          vec_swapsplice(&state.incoherents[i], j, 1);
          break;
        }
      }
    }
  }
}

static void lovrGpuBindFramebuffer(uint32_t framebuffer) {
  if (state.framebuffer != framebuffer) {
    state.framebuffer = framebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  }
}

static void lovrGpuUseProgram(uint32_t program) {
  if (state.program != program) {
    state.program = program;
    glUseProgram(program);
    state.stats.shaderSwitches++;
  }
}

static void lovrGpuBindVertexArray(uint32_t vertexArray) {
  if (state.vertexArray != vertexArray) {
    state.vertexArray = vertexArray;
    glBindVertexArray(vertexArray);
  }
}

static void lovrGpuBindBuffer(BufferType type, uint32_t buffer, bool force) {
  if (force || state.buffers[type] != buffer) {
    state.buffers[type] = buffer;
    glBindBuffer(convertBufferType(type), buffer);
  }
}

static void lovrGpuBindBlockBuffer(BlockType type, uint32_t buffer, int slot, size_t offset, size_t size) {
  lovrAssert(offset % state.limits.blockAlign == 0, "Block buffer offset must be aligned to %d", state.limits.blockAlign);
#ifdef EMSCRIPTEN
  lovrAssert(type == BLOCK_UNIFORM, "Writable blocks are not supported on this system");
  GLenum target = GL_UNIFORM_BUFFER;
#else
  GLenum target = type == BLOCK_UNIFORM ? GL_UNIFORM_BUFFER : GL_SHADER_STORAGE_BUFFER;
#endif

  BlockBuffer* block = &state.blockBuffers[type][slot];
  if (block->buffer != buffer || block->offset != offset || block->size != size) {
    block->buffer = buffer;
    block->offset = offset;
    block->size = size;
    glBindBufferRange(target, slot, buffer, offset, size);
  }
}

static void lovrGpuBindTexture(Texture* texture, int slot) {
  lovrAssert(slot >= 0 && slot < MAX_TEXTURES, "Invalid texture slot %d", slot);
  texture = texture ? texture : lovrGpuGetDefaultTexture();

  if (texture != state.textures[slot]) {
    lovrRetain(texture);
    lovrRelease(state.textures[slot]);
    state.textures[slot] = texture;
    if (state.activeTexture != slot) {
      glActiveTexture(GL_TEXTURE0 + slot);
      state.activeTexture = slot;
    }
    glBindTexture(texture->target, texture->id);
  }
}

#ifndef EMSCRIPTEN
static void lovrGpuBindImage(Image* image, int slot) {
  lovrAssert(slot >= 0 && slot < MAX_IMAGES, "Invalid image slot %d", slot);

  // This is a risky way to compare the two structs
  if (memcmp(state.images + slot, image, sizeof(Image))) {
    Texture* texture = image->texture ? image->texture : lovrGpuGetDefaultTexture();
    lovrAssert(!texture->srgb, "sRGB textures can not be used as image uniforms");
    lovrAssert(!isTextureFormatCompressed(texture->format), "Compressed textures can not be used as image uniforms");
    lovrAssert(texture->format != FORMAT_RGB && texture->format != FORMAT_RGBA4 && texture->format != FORMAT_RGB5A1, "Unsupported texture format for image uniform");
    lovrAssert(image->mipmap >= 0 && image->mipmap < texture->mipmapCount, "Invalid mipmap level '%d' for image uniform", image->mipmap);
    lovrAssert(image->slice < texture->depth, "Invalid texture slice '%d' for image uniform", image->slice);
    GLenum glAccess = convertAccess(image->access);
    GLenum glFormat = convertTextureFormatInternal(texture->format, false);
    bool layered = image->slice == -1;
    int slice = layered ? 0 : image->slice;

    lovrRetain(texture);
    lovrRelease(state.images[slot].texture);
    glBindImageTexture(slot, texture->id, image->mipmap, layered, slice, glAccess, glFormat);
    memcpy(state.images + slot, image, sizeof(Image));
  }
}
#endif

static void lovrGpuBindMesh(Mesh* mesh, Shader* shader, int divisorMultiplier) {
  const char* key;
  map_iter_t iter = map_iter(&mesh->attachments);

  MeshAttribute layout[MAX_ATTRIBUTES];
  memset(layout, 0, MAX_ATTRIBUTES * sizeof(MeshAttribute));

  lovrGpuBindVertexArray(mesh->vao);

  if (mesh->indexBuffer && mesh->indexCount > 0) {
    lovrGpuBindBuffer(BUFFER_INDEX, mesh->indexBuffer->id, true);
    uint32_t primitiveRestart = (1 << (mesh->indexSize * 8)) - 1;
    if (state.primitiveRestart != primitiveRestart) {
      state.primitiveRestart = primitiveRestart;
      glPrimitiveRestartIndex(primitiveRestart);
    }
  }

  if (mesh->flushEnd > 0) {
    lovrBufferFlush(mesh->vertexBuffer, mesh->flushStart, mesh->flushEnd - mesh->flushStart);
    mesh->flushStart = mesh->flushEnd = 0;
  }

  while ((key = map_next(&mesh->attributes, &iter)) != NULL) {
    int location = lovrShaderGetAttributeId(shader, key);

    if (location >= 0) {
      MeshAttribute* attribute = map_get(&mesh->attributes, key);
      layout[location] = *attribute;
    }
  }

  for (int i = 0; i < MAX_ATTRIBUTES; i++) {
    MeshAttribute previous = mesh->layout[i];
    MeshAttribute current = layout[i];

    if (!memcmp(&previous, &current, sizeof(MeshAttribute))) {
      continue;
    }

    if (previous.enabled != current.enabled) {
      if (current.enabled) {
        glEnableVertexAttribArray(i);
      } else {
        glDisableVertexAttribArray(i);
        mesh->layout[i] = current;
        continue;
      }
    }

    if (previous.divisor != current.divisor) {
      glVertexAttribDivisor(i, current.divisor * divisorMultiplier);
    }

    bool changed =
      previous.buffer != current.buffer ||
      previous.type != current.type ||
      previous.components != current.components ||
      previous.offset != current.offset ||
      previous.stride != current.stride;

    if (changed) {
      lovrGpuBindBuffer(BUFFER_VERTEX, current.buffer->id, false);
      int count = current.components;
      int stride = current.stride;
      GLvoid* offset = (GLvoid*) current.offset;

      // TODO
      if (current.integer) {
        switch (current.type) {
          case ATTR_BYTE: glVertexAttribIPointer(i, count, GL_UNSIGNED_BYTE, stride, offset); break;
          case ATTR_INT: glVertexAttribIPointer(i, count, GL_INT, stride, offset); break;
          default: lovrThrow("Cannot use float data for int attribute");
        }
      } else {
        switch (current.type) {
          case ATTR_FLOAT: glVertexAttribPointer(i, count, GL_FLOAT, GL_TRUE, stride, offset); break;
          case ATTR_BYTE: glVertexAttribPointer(i, count, GL_UNSIGNED_BYTE, GL_TRUE, stride, offset); break;
          case ATTR_INT: glVertexAttribPointer(i, count, GL_INT, GL_TRUE, stride, offset); break;
        }
      }
    }
  }

  memcpy(mesh->layout, layout, MAX_ATTRIBUTES * sizeof(MeshAttribute));
}

static void lovrGpuBindCanvas(Canvas* canvas, bool willDraw) {
  if (canvas) {
    lovrGpuBindFramebuffer(canvas->framebuffer);
    canvas->needsResolve = willDraw;
  } else {
    lovrGpuBindFramebuffer(0);
    return;
  }

  if (!canvas->needsAttach) {
    return;
  }

  // We need to synchronize if any of the Canvas attachments have pending writes on them
#ifndef EMSCRIPTEN
  for (int i = 0; i < canvas->attachmentCount; i++) {
    Texture* texture = canvas->attachments[i].texture;
    if (texture->incoherent && (texture->incoherent >> BARRIER_CANVAS) & 1) {
      lovrGpuSync(1 << BARRIER_CANVAS);
      break;
    }
  }
#endif

  // Use the read framebuffer as a binding point to bind resolve textures
  if (canvas->flags.msaa) {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, canvas->resolveBuffer);
  }

  GLenum buffers[MAX_CANVAS_ATTACHMENTS] = { GL_NONE };
  for (int i = 0; i < canvas->attachmentCount; i++) {
    GLenum buffer = buffers[i] = GL_COLOR_ATTACHMENT0 + i;
    Attachment* attachment = &canvas->attachments[i];
    Texture* texture = attachment->texture;
    int slice = attachment->slice;
    int level = attachment->level;

    if (canvas->flags.msaa) {
      glFramebufferRenderbuffer(GL_FRAMEBUFFER, buffer, GL_RENDERBUFFER, texture->msaaId);
    }

    switch (texture->type) {
      case TEXTURE_2D: glFramebufferTexture2D(GL_READ_FRAMEBUFFER, buffer, GL_TEXTURE_2D, texture->id, level); break;
      case TEXTURE_CUBE: glFramebufferTexture2D(GL_READ_FRAMEBUFFER, buffer, GL_TEXTURE_CUBE_MAP_POSITIVE_X + slice, texture->id, level); break;
      case TEXTURE_ARRAY: glFramebufferTextureLayer(GL_READ_FRAMEBUFFER, buffer, texture->id, level, slice); break;
      case TEXTURE_VOLUME: glFramebufferTextureLayer(GL_READ_FRAMEBUFFER, buffer, texture->id, level, slice); break;
    }
  }
  glDrawBuffers(canvas->attachmentCount, buffers);

  GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  switch (status) {
    case GL_FRAMEBUFFER_COMPLETE: break;
    case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE: lovrThrow("Unable to set Canvas (MSAA settings)"); break;
    case GL_FRAMEBUFFER_UNSUPPORTED: lovrThrow("Unable to set Canvas (Texture formats)"); break;
    default: lovrThrow("Unable to set Canvas (reason unknown)"); break;
  }

  canvas->needsAttach = false;
}

static void lovrGpuBindPipeline(Pipeline* pipeline) {

  // Alpha Coverage
  if (state.alphaToCoverage != pipeline->alphaSampling) {
    state.alphaToCoverage = pipeline->alphaSampling;
    if (state.alphaToCoverage) {
      glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    } else {
      glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    }
  }

  // Blend mode
  if (state.blendMode != pipeline->blendMode || state.blendAlphaMode != pipeline->blendAlphaMode) {
    state.blendMode = pipeline->blendMode;
    state.blendAlphaMode = pipeline->blendAlphaMode;

    if (state.blendMode == BLEND_NONE) {
      if (state.blendEnabled) {
        state.blendEnabled = false;
        glDisable(GL_BLEND);
      }
    } else {
      if (!state.blendEnabled) {
        state.blendEnabled = true;
        glEnable(GL_BLEND);
      }

      GLenum srcRGB = state.blendMode == BLEND_MULTIPLY ? GL_DST_COLOR : GL_ONE;
      if (srcRGB == GL_ONE && state.blendAlphaMode == BLEND_ALPHA_MULTIPLY) {
        srcRGB = GL_SRC_ALPHA;
      }

      switch (state.blendMode) {
        case BLEND_ALPHA:
          glBlendEquation(GL_FUNC_ADD);
          glBlendFuncSeparate(srcRGB, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
          break;

        case BLEND_ADD:
          glBlendEquation(GL_FUNC_ADD);
          glBlendFuncSeparate(srcRGB, GL_ONE, GL_ZERO, GL_ONE);
          break;

        case BLEND_SUBTRACT:
          glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
          glBlendFuncSeparate(srcRGB, GL_ONE, GL_ZERO, GL_ONE);
          break;

        case BLEND_MULTIPLY:
          glBlendEquation(GL_FUNC_ADD);
          glBlendFuncSeparate(srcRGB, GL_ZERO, GL_DST_COLOR, GL_ZERO);
          break;

        case BLEND_LIGHTEN:
          glBlendEquation(GL_MAX);
          glBlendFuncSeparate(srcRGB, GL_ZERO, GL_ONE, GL_ZERO);
          break;

        case BLEND_DARKEN:
          glBlendEquation(GL_MIN);
          glBlendFuncSeparate(srcRGB, GL_ZERO, GL_ONE, GL_ZERO);
          break;

        case BLEND_SCREEN:
          glBlendEquation(GL_FUNC_ADD);
          glBlendFuncSeparate(srcRGB, GL_ONE_MINUS_SRC_COLOR, GL_ONE, GL_ONE_MINUS_SRC_COLOR);
          break;

        case BLEND_NONE: lovrThrow("Unreachable"); break;
      }
    }
  }

  // Culling
  if (state.culling != pipeline->culling) {
    state.culling = pipeline->culling;
    if (state.culling) {
      glEnable(GL_CULL_FACE);
    } else {
      glDisable(GL_CULL_FACE);
    }
  }

  // Depth test
  if (state.depthTest != pipeline->depthTest) {
    state.depthTest = pipeline->depthTest;
    if (state.depthTest != COMPARE_NONE) {
      if (!state.depthEnabled) {
        state.depthEnabled = true;
        glEnable(GL_DEPTH_TEST);
      }
      glDepthFunc(convertCompareMode(state.depthTest));
    } else if (state.depthEnabled) {
      state.depthEnabled = false;
      glDisable(GL_DEPTH_TEST);
    }
  }

  // Depth write
  if (state.depthWrite != (pipeline->depthWrite && !state.stencilWriting)) {
    state.depthWrite = pipeline->depthWrite && !state.stencilWriting;
    glDepthMask(state.depthWrite);
  }

  // Line width
  if (state.lineWidth != pipeline->lineWidth) {
    state.lineWidth = pipeline->lineWidth;
    glLineWidth(state.lineWidth);
  }

  // Stencil mode
  if (!state.stencilWriting && (state.stencilMode != pipeline->stencilMode || state.stencilValue != pipeline->stencilValue)) {
    state.stencilMode = pipeline->stencilMode;
    state.stencilValue = pipeline->stencilValue;
    if (state.stencilMode != COMPARE_NONE) {
      if (!state.stencilEnabled) {
        state.stencilEnabled = true;
        glEnable(GL_STENCIL_TEST);
      }

      GLenum glMode = GL_ALWAYS;
      switch (state.stencilMode) {
        case COMPARE_EQUAL: glMode = GL_EQUAL; break;
        case COMPARE_NEQUAL: glMode = GL_NOTEQUAL; break;
        case COMPARE_LESS: glMode = GL_GREATER; break;
        case COMPARE_LEQUAL: glMode = GL_GEQUAL; break;
        case COMPARE_GREATER: glMode = GL_LESS; break;
        case COMPARE_GEQUAL: glMode = GL_LEQUAL; break;
        default: break;
      }

      glStencilFunc(glMode, state.stencilValue, 0xff);
      glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    } else if (state.stencilEnabled) {
      state.stencilEnabled = false;
      glDisable(GL_STENCIL_TEST);
    }
  }

  // Winding
  if (state.winding != pipeline->winding) {
    state.winding = pipeline->winding;
    glFrontFace(state.winding == WINDING_CLOCKWISE ? GL_CW : GL_CCW);
  }

  // Wireframe
#ifndef EMSCRIPTEN
  if (state.wireframe != pipeline->wireframe) {
    state.wireframe = pipeline->wireframe;
    glPolygonMode(GL_FRONT_AND_BACK, state.wireframe ? GL_LINE : GL_FILL);
  }
#endif
}

static void lovrGpuBindShader(Shader* shader) {
  UniformBlock* block;
  Uniform* uniform;
  int i;

  lovrGpuUseProgram(shader->program);

  // Figure out if we need to wait for pending writes on resources to complete
#ifndef EMSCRIPTEN
  uint8_t flags = 0;
  vec_foreach_ptr(&shader->blocks[BLOCK_STORAGE], block, i) {
    if (block->source && (block->source->incoherent >> BARRIER_BLOCK) & 1) {
      flags |= 1 << BARRIER_BLOCK;
      break;
    }
  }

  vec_foreach_ptr(&shader->uniforms, uniform, i) {
    if (uniform->type == UNIFORM_SAMPLER) {
      for (int i = 0; i < uniform->count; i++) {
        Texture* texture = uniform->value.textures[i];
        if (texture && texture->incoherent && (texture->incoherent >> BARRIER_UNIFORM_TEXTURE) & 1) {
          flags |= 1 << BARRIER_UNIFORM_TEXTURE;
          if (flags & (1 << BARRIER_UNIFORM_IMAGE)) {
            break;
          }
        }
      }
    } else if (uniform->type == UNIFORM_IMAGE) {
      for (int i = 0; i < uniform->count; i++) {
        Texture* texture = uniform->value.images[i].texture;
        if (texture && texture->incoherent && (texture->incoherent >> BARRIER_UNIFORM_IMAGE) & 1) {
          flags |= 1 << BARRIER_UNIFORM_IMAGE;
          if (flags & (1 << BARRIER_UNIFORM_TEXTURE)) {
            break;
          }
        }
      }
    }
  }

  lovrGpuSync(flags);
#endif

  // Bind uniforms
  vec_foreach_ptr(&shader->uniforms, uniform, i) {
    if (uniform->type != UNIFORM_SAMPLER && uniform->type != UNIFORM_IMAGE && !uniform->dirty) {
      continue;
    }

    uniform->dirty = false;
    int count = uniform->count;
    void* data = uniform->value.data;

    switch (uniform->type) {
      case UNIFORM_FLOAT:
        switch (uniform->components) {
          case 1: glUniform1fv(uniform->location, count, data); break;
          case 2: glUniform2fv(uniform->location, count, data); break;
          case 3: glUniform3fv(uniform->location, count, data); break;
          case 4: glUniform4fv(uniform->location, count, data); break;
        }
        break;

      case UNIFORM_INT:
        switch (uniform->components) {
          case 1: glUniform1iv(uniform->location, count, data); break;
          case 2: glUniform2iv(uniform->location, count, data); break;
          case 3: glUniform3iv(uniform->location, count, data); break;
          case 4: glUniform4iv(uniform->location, count, data); break;
        }
        break;

      case UNIFORM_MATRIX:
        switch (uniform->components) {
          case 2: glUniformMatrix2fv(uniform->location, count, GL_FALSE, data); break;
          case 3: glUniformMatrix3fv(uniform->location, count, GL_FALSE, data); break;
          case 4: glUniformMatrix4fv(uniform->location, count, GL_FALSE, data); break;
        }
        break;

      case UNIFORM_IMAGE:
#ifndef EMSCRIPTEN
        for (int i = 0; i < count; i++) {
          Image* image = &uniform->value.images[i];
          Texture* texture = image->texture;
          lovrAssert(!texture || texture->type == uniform->textureType, "Uniform texture type mismatch for uniform %s", uniform->name);

          // If the Shader can write to the texture, mark it as incoherent
          if (texture && image->access != ACCESS_READ) {
            for (Barrier barrier = BARRIER_BLOCK + 1; barrier < MAX_BARRIERS; barrier++) {
              texture->incoherent |= 1 << barrier;
              vec_push(&state.incoherents[barrier], texture);
            }
          }

          lovrGpuBindImage(image, uniform->baseSlot + i);
        }
#endif
        break;

      case UNIFORM_SAMPLER:
        for (int i = 0; i < count; i++) {
          Texture* texture = uniform->value.textures[i];
          lovrAssert(!texture || texture->type == uniform->textureType, "Uniform texture type mismatch for uniform %s", uniform->name);
          lovrGpuBindTexture(texture, uniform->baseSlot + i);
        }
        break;
    }
  }

  // Bind uniform blocks
  for (BlockType type = BLOCK_UNIFORM; type <= BLOCK_STORAGE; type++) {
    vec_foreach_ptr(&shader->blocks[type], block, i) {
      if (block->source) {
        if (type == BLOCK_STORAGE && block->access != ACCESS_READ) {
          block->source->incoherent |= (1 << BARRIER_BLOCK);
          vec_push(&state.incoherents[BARRIER_BLOCK], block->source);
        }

        lovrGpuBindBlockBuffer(type, block->source->id, block->slot, block->offset, block->size);
      } else {
        lovrGpuBindBlockBuffer(type, 0, block->slot, 0, 0);
      }
    }
  }
}

static void lovrGpuSetViewports(float* viewport, uint32_t count) {
  if (state.viewportCount != count || memcmp(state.viewports, viewport, count * 4 * sizeof(float))) {
    memcpy(state.viewports, viewport, count * 4 * sizeof(float));
    state.viewportCount = count;
#ifndef EMSCRIPTEN
    if (count > 1) {
      glViewportArrayv(0, count, viewport);
    } else {
#endif
      glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    }
#ifndef EMSCRIPTEN
  }
#endif
}

// GPU

void lovrGpuInit(bool srgb, getProcAddressProc getProcAddress) {
#ifndef EMSCRIPTEN
  gladLoadGLLoader((GLADloadproc) getProcAddress);
  state.features.computeShaders = GLAD_GL_ARB_compute_shader;
  state.features.singlepass = GLAD_GL_ARB_viewport_array && GLAD_GL_AMD_vertex_shader_viewport_index && GLAD_GL_ARB_fragment_layer_viewport;
  glEnable(GL_LINE_SMOOTH);
  glEnable(GL_PROGRAM_POINT_SIZE);
  if (srgb) {
    glEnable(GL_FRAMEBUFFER_SRGB);
  } else {
    glDisable(GL_FRAMEBUFFER_SRGB);
  }
  glGetFloatv(GL_POINT_SIZE_RANGE, state.limits.pointSizes);
#else
  glGetFloatv(GL_ALIASED_POINT_SIZE_RANGE, state.limits.pointSizes);
#endif
  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &state.limits.textureSize);
  glGetIntegerv(GL_MAX_SAMPLES, &state.limits.textureMSAA);
  glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &state.limits.blockSize);
  glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &state.limits.blockAlign);
  glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &state.limits.textureAnisotropy);

  glEnable(GL_PRIMITIVE_RESTART);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  state.srgb = srgb;

  state.alphaToCoverage = false;
  glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);

  state.blendEnabled = true;
  state.blendMode = BLEND_ALPHA;
  state.blendAlphaMode = BLEND_ALPHA_MULTIPLY;
  glEnable(GL_BLEND);
  glBlendEquation(GL_FUNC_ADD);
  glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

  state.culling = false;
  glDisable(GL_CULL_FACE);

  state.depthEnabled = true;
  state.depthTest = COMPARE_LEQUAL;
  state.depthWrite = true;
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(convertCompareMode(state.depthTest));
  glDepthMask(state.depthWrite);

  state.lineWidth = 1;
  glLineWidth(state.lineWidth);

  state.primitiveRestart = 0xffffffff;
  glPrimitiveRestartIndex(state.primitiveRestart);

  state.stencilEnabled = false;
  state.stencilMode = COMPARE_NONE;
  state.stencilValue = 0;
  state.stencilWriting = false;
  glDisable(GL_STENCIL_TEST);

  state.winding = WINDING_COUNTERCLOCKWISE;
  glFrontFace(GL_CCW);

  state.wireframe = false;
#if !(defined(EMSCRIPTEN) || defined(__ANDROID__))
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
#endif

  for (int i = 0; i < MAX_BARRIERS; i++) {
    vec_init(&state.incoherents[i]);
  }
}

void lovrGpuDestroy() {
  lovrRelease(state.defaultTexture);
  for (int i = 0; i < MAX_TEXTURES; i++) {
    lovrRelease(state.textures[i]);
  }
  for (int i = 0; i < MAX_IMAGES; i++) {
    lovrRelease(state.images[i].texture);
  }
  for (int i = 0; i < MAX_BARRIERS; i++) {
    vec_deinit(&state.incoherents[i]);
  }
  memset(&state, 0, sizeof(state));
}

void lovrGpuClear(Canvas* canvas, Color* color, float* depth, int* stencil) {
  lovrGpuBindCanvas(canvas, true);

  if (color) {
    int count = canvas ? canvas->attachmentCount : 1;
    for (int i = 0; i < count; i++) {
      glClearBufferfv(GL_COLOR, i, (float[]) { color->r, color->g, color->b, color->a });
    }
  }

  if (depth && !state.depthWrite) {
    state.depthWrite = true;
    glDepthMask(state.depthWrite);
  }

  if (depth && stencil) {
    glClearBufferfi(GL_DEPTH_STENCIL, 0, *depth, *stencil);
  } else if (depth) {
    glClearBufferfv(GL_DEPTH, 0, depth);
  } else if (stencil) {
    glClearBufferiv(GL_STENCIL, 0, stencil);
  }
}

void lovrGpuCompute(Shader* shader, int x, int y, int z) {
#ifdef EMSCRIPTEN
  lovrThrow("Compute shaders are not supported on this system");
#else
  lovrAssert(GLAD_GL_ARB_compute_shader, "Compute shaders are not supported on this system");
  lovrAssert(shader->type == SHADER_COMPUTE, "Attempt to use a non-compute shader for a compute operation");
  lovrGraphicsFlush();
  lovrGpuBindShader(shader);
  glDispatchCompute(x, y, z);
#endif
}

void lovrGpuDiscard(Canvas* canvas, bool color, bool depth, bool stencil) {
#if defined(EMSCRIPTEN) || defined(__ANDROID__)
  lovrGpuBindCanvas(canvas, false);

  GLenum attachments[MAX_CANVAS_ATTACHMENTS + 1] = { 0 };
  int count = 0;

  if (color) {
    int n = canvas ? canvas->attachmentCount : 1;
    for (int i = 0; i < n; i++) {
      attachments[count++] = GL_COLOR_ATTACHMENT0 + i;
    }
  }

  if (depth) {
    attachments[count++] = GL_DEPTH_ATTACHMENT;
  }

  if (stencil) {
    attachments[count++] = GL_STENCIL_ATTACHMENT;
  }

  glInvalidateFramebuffer(GL_FRAMEBUFFER, count, attachments);
#endif
}

void lovrGpuDraw(DrawCommand* draw) {
  uint32_t viewCount = 1 + draw->stereo;
  uint32_t drawCount = state.features.singlepass ? 1 : viewCount;
  uint32_t viewsPerDraw = state.features.singlepass ? viewCount : 1;
  uint32_t instances = MAX(draw->instances, 1) * viewsPerDraw;

  float w = draw->width / (float) viewCount;
  float h = draw->height;
  float viewports[2][4] = { { 0, 0, w, h }, { w, 0, w, h } };
  lovrShaderSetInts(draw->shader, "lovrViewportCount", &(int) { viewCount }, 0, 1);

  lovrGpuBindCanvas(draw->canvas, true);
  lovrGpuBindPipeline(&draw->pipeline);
  lovrGpuBindMesh(draw->mesh, draw->shader, viewsPerDraw);

  for (uint32_t i = 0; i < drawCount; i++) {
    lovrGpuSetViewports(&viewports[i][0], viewsPerDraw);
    lovrShaderSetInts(draw->shader, "lovrViewportIndex", &(int) { i }, 0, 1);
    lovrGpuBindShader(draw->shader);

    Mesh* mesh = draw->mesh;
    GLenum mode = convertDrawMode(draw->drawMode);
    if (mesh->indexCount > 0) {
      GLenum indexType = mesh->indexSize == sizeof(uint16_t) ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
      GLvoid* offset = (GLvoid*) (draw->rangeStart * mesh->indexSize);
      if (instances > 1) {
        glDrawElementsInstanced(mode, draw->rangeCount, indexType, offset, instances);
      } else {
        glDrawElements(mode, draw->rangeCount, indexType, offset);
      }
    } else {
      if (instances > 1) {
        glDrawArraysInstanced(mode, draw->rangeStart, draw->rangeCount, instances);
      } else {
        glDrawArrays(mode, draw->rangeStart, draw->rangeCount);
      }
    }

    state.stats.drawCalls++;
  }
}

void lovrGpuPresent() {
  memset(&state.stats, 0, sizeof(state.stats));
#ifdef __APPLE__
  // For some reason instancing doesn't work on macOS unless you reset the shader every frame
  lovrGpuUseProgram(0);
#endif
}

void lovrGpuStencil(StencilAction action, int replaceValue, StencilCallback callback, void* userdata) {
  lovrGraphicsFlush();
  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

  if (!state.stencilEnabled) {
    state.stencilEnabled = true;
    glEnable(GL_STENCIL_TEST);
  }

  GLenum glAction;
  switch (action) {
    case STENCIL_REPLACE: glAction = GL_REPLACE; break;
    case STENCIL_INCREMENT: glAction = GL_INCR; break;
    case STENCIL_DECREMENT: glAction = GL_DECR; break;
    case STENCIL_INCREMENT_WRAP: glAction = GL_INCR_WRAP; break;
    case STENCIL_DECREMENT_WRAP: glAction = GL_DECR_WRAP; break;
    case STENCIL_INVERT: glAction = GL_INVERT; break;
  }

  glStencilFunc(GL_ALWAYS, replaceValue, 0xff);
  glStencilOp(GL_KEEP, GL_KEEP, glAction);

  state.stencilWriting = true;
  callback(userdata);
  lovrGraphicsFlush();
  state.stencilWriting = false;

  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  state.stencilMode = ~0; // Dirty
}

void lovrGpuDirtyTexture() {
  state.textures[state.activeTexture] = NULL;
}

void* lovrGpuLock() {
  return (void*) glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
}

void lovrGpuUnlock(void* lock) {
  if (!lock) return;
  GLsync sync = (GLsync) lock;
  if (glClientWaitSync(sync, 0, 0) == GL_TIMEOUT_EXPIRED) {
    while (glClientWaitSync(sync, GL_SYNC_FLUSH_COMMANDS_BIT, 1E9) == GL_TIMEOUT_EXPIRED) {
      continue;
    }
  }
}

void lovrGpuDestroyLock(void* lock) {
  if (lock) glDeleteSync((GLsync) lock);
}

const GpuFeatures* lovrGpuGetSupported() {
  return &state.features;
}

const GpuLimits* lovrGpuGetLimits() {
  return &state.limits;
}

const GpuStats* lovrGpuGetStats() {
  return &state.stats;
}

// Texture

Texture* lovrTextureInit(Texture* texture, TextureType type, TextureData** slices, int sliceCount, bool srgb, bool mipmaps, int msaa) {
  texture->type = type;
  texture->srgb = srgb;
  texture->mipmaps = mipmaps;
  texture->target = convertTextureTarget(type);

  WrapMode wrap = type == TEXTURE_CUBE ? WRAP_CLAMP : WRAP_REPEAT;
  glGenTextures(1, &texture->id);
  lovrGpuBindTexture(texture, 0);
  lovrTextureSetWrap(texture, (TextureWrap) { .s = wrap, .t = wrap, .r = wrap });

  if (msaa > 0) {
    texture->msaa = msaa;
    glGenRenderbuffers(1, &texture->msaaId);
  }

  if (sliceCount > 0) {
    lovrTextureAllocate(texture, slices[0]->width, slices[0]->height, sliceCount, slices[0]->format);
    for (int i = 0; i < sliceCount; i++) {
      lovrTextureReplacePixels(texture, slices[i], 0, 0, i, 0);
    }
  }

  return texture;
}

Texture* lovrTextureInitFromHandle(Texture* texture, uint32_t handle, TextureType type) {
  texture->type = type;
  texture->id = handle;
  texture->target = convertTextureTarget(type);

  lovrGpuBindTexture(texture, 0);
  glGetTexLevelParameteriv(texture->target, 0, GL_TEXTURE_WIDTH, &texture->width);
  glGetTexLevelParameteriv(texture->target, 0, GL_TEXTURE_HEIGHT, &texture->height);

  return texture;
}

void lovrTextureDestroy(void* ref) {
  Texture* texture = ref;
  glDeleteTextures(1, &texture->id);
  glDeleteRenderbuffers(1, &texture->msaaId);
  lovrGpuDestroySyncResource(texture, texture->incoherent);
}

void lovrTextureAllocate(Texture* texture, int width, int height, int depth, TextureFormat format) {
  int maxSize = state.limits.textureSize;
  lovrAssert(!texture->allocated, "Texture is already allocated");
  lovrAssert(texture->type != TEXTURE_CUBE || width == height, "Cubemap images must be square");
  lovrAssert(texture->type != TEXTURE_CUBE || depth == 6, "6 images are required for a cube texture\n");
  lovrAssert(texture->type != TEXTURE_2D || depth == 1, "2D textures can only contain a single image");
  lovrAssert(width < maxSize, "Texture width %d exceeds max of %d", width, maxSize);
  lovrAssert(height < maxSize, "Texture height %d exceeds max of %d", height, maxSize);
  lovrAssert(!texture->msaa || texture->type == TEXTURE_2D, "Only 2D textures can be created with MSAA");

  texture->allocated = true;
  texture->width = width;
  texture->height = height;
  texture->depth = depth;
  texture->format = format;

  if (texture->mipmaps) {
    int dimension = texture->type == TEXTURE_VOLUME ? (MAX(MAX(width, height), depth)) : MAX(width, height);
    texture->mipmapCount = texture->mipmaps ? (log2(dimension) + 1) : 1;
  } else {
    texture->mipmapCount = 1;
  }

  if (isTextureFormatCompressed(format)) {
    return;
  }

  bool srgb = state.srgb && texture->srgb;
  GLenum glFormat = convertTextureFormat(format);
  GLenum internalFormat = convertTextureFormatInternal(format, srgb);
#ifndef EMSCRIPTEN
  if (GLAD_GL_ARB_texture_storage) {
#endif
  if (texture->type == TEXTURE_ARRAY) {
    glTexStorage3D(texture->target, texture->mipmapCount, internalFormat, width, height, depth);
  } else {
    glTexStorage2D(texture->target, texture->mipmapCount, internalFormat, width, height);
  }
#ifndef EMSCRIPTEN
  } else {
    for (int i = 0; i < texture->mipmapCount; i++) {
      switch (texture->type) {
        case TEXTURE_2D:
          glTexImage2D(texture->target, i, internalFormat, width, height, 0, glFormat, GL_UNSIGNED_BYTE, NULL);
          break;

        case TEXTURE_CUBE:
          for (int face = 0; face < 6; face++) {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, i, internalFormat, width, height, 0, glFormat, GL_UNSIGNED_BYTE, NULL);
          }
          break;

        case TEXTURE_ARRAY:
        case TEXTURE_VOLUME:
          glTexImage3D(texture->target, i, internalFormat, width, height, depth, 0, glFormat, GL_UNSIGNED_BYTE, NULL);
          break;
      }
      width = MAX(width >> 1, 1);
      height = MAX(height >> 1, 1);
      depth = texture->type == TEXTURE_VOLUME ? MAX(depth >> 1, 1) : depth;
    }
  }
#endif

  if (texture->msaaId) {
    glBindRenderbuffer(GL_RENDERBUFFER, texture->msaaId);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, texture->msaa, internalFormat, width, height);
  }
}

void lovrTextureReplacePixels(Texture* texture, TextureData* textureData, int x, int y, int slice, int mipmap) {
  lovrGraphicsFlush();
  lovrAssert(texture->allocated, "Texture is not allocated");

#ifndef EMSCRIPTEN
  if ((texture->incoherent >> BARRIER_TEXTURE) & 1) {
    lovrGpuSync(1 << BARRIER_TEXTURE);
  }
#endif

  int maxWidth = lovrTextureGetWidth(texture, mipmap);
  int maxHeight = lovrTextureGetHeight(texture, mipmap);
  int width = textureData->width;
  int height = textureData->height;
  bool overflow = (x + width > maxWidth) || (y + height > maxHeight);
  lovrAssert(!overflow, "Trying to replace pixels outside the texture's bounds");
  lovrAssert(mipmap >= 0 && mipmap < texture->mipmapCount, "Invalid mipmap level %d", mipmap);
  GLenum glFormat = convertTextureFormat(textureData->format);
  GLenum glInternalFormat = convertTextureFormatInternal(textureData->format, texture->srgb);
  GLenum binding = (texture->type == TEXTURE_CUBE) ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + slice : texture->target;

  lovrGpuBindTexture(texture, 0);
  if (isTextureFormatCompressed(textureData->format)) {
    lovrAssert(width == maxWidth && height == maxHeight, "Compressed texture pixels must be fully replaced");
    lovrAssert(mipmap == 0, "Unable to replace a specific mipmap of a compressed texture");
    Mipmap m; int i;
    vec_foreach(&textureData->mipmaps, m, i) {
      switch (texture->type) {
        case TEXTURE_2D:
        case TEXTURE_CUBE:
          glCompressedTexImage2D(binding, i, glInternalFormat, m.width, m.height, 0, m.size, m.data);
          break;
        case TEXTURE_ARRAY:
        case TEXTURE_VOLUME:
          glCompressedTexSubImage3D(binding, i, x, y, slice, m.width, m.height, 1, glInternalFormat, m.size, m.data);
          break;
      }
    }
  } else {
    lovrAssert(textureData->blob.data, "Trying to replace Texture pixels with empty pixel data");
    GLenum glType = convertTextureFormatType(textureData->format);

    switch (texture->type) {
      case TEXTURE_2D:
      case TEXTURE_CUBE:
        glTexSubImage2D(binding, mipmap, x, y, width, height, glFormat, glType, textureData->blob.data);
        break;
      case TEXTURE_ARRAY:
      case TEXTURE_VOLUME:
        glTexSubImage3D(binding, mipmap, x, y, slice, width, height, 1, glFormat, glType, textureData->blob.data);
        break;
    }

    if (texture->mipmaps) {
#if defined(__APPLE__) || defined(EMSCRIPTEN) // glGenerateMipmap doesn't work on big cubemap textures on macOS
      if (texture->type != TEXTURE_CUBE || width < 2048) {
        glGenerateMipmap(texture->target);
      } else {
        glTexParameteri(texture->target, GL_TEXTURE_MAX_LEVEL, 0);
      }
#else
      glGenerateMipmap(texture->target);
#endif
    }
  }
}

void lovrTextureSetFilter(Texture* texture, TextureFilter filter) {
  lovrGraphicsFlush();
  float anisotropy = filter.mode == FILTER_ANISOTROPIC ? MAX(filter.anisotropy, 1.) : 1.;
  lovrGpuBindTexture(texture, 0);
  texture->filter = filter;

  switch (filter.mode) {
    case FILTER_NEAREST:
      glTexParameteri(texture->target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(texture->target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      break;

    case FILTER_BILINEAR:
      if (texture->mipmaps) {
        glTexParameteri(texture->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
        glTexParameteri(texture->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      } else {
        glTexParameteri(texture->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(texture->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      }
      break;

    case FILTER_TRILINEAR:
    case FILTER_ANISOTROPIC:
      if (texture->mipmaps) {
        glTexParameteri(texture->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(texture->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      } else {
        glTexParameteri(texture->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(texture->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      }
      break;
  }

  glTexParameteri(texture->target, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropy);
}

void lovrTextureSetWrap(Texture* texture, TextureWrap wrap) {
  lovrGraphicsFlush();
  texture->wrap = wrap;
  lovrGpuBindTexture(texture, 0);
  glTexParameteri(texture->target, GL_TEXTURE_WRAP_S, convertWrapMode(wrap.s));
  glTexParameteri(texture->target, GL_TEXTURE_WRAP_T, convertWrapMode(wrap.t));
  if (texture->type == TEXTURE_CUBE || texture->type == TEXTURE_VOLUME) {
    glTexParameteri(texture->target, GL_TEXTURE_WRAP_R, convertWrapMode(wrap.r));
  }
}

// Canvas

Canvas* lovrCanvasInit(Canvas* canvas, int width, int height, CanvasFlags flags) {
  canvas->width = width;
  canvas->height = height;
  canvas->flags = flags;

  glGenFramebuffers(1, &canvas->framebuffer);
  lovrGpuBindFramebuffer(canvas->framebuffer);

  if (flags.depth.enabled) {
    lovrAssert(isTextureFormatDepth(flags.depth.format), "Canvas depth buffer can't use a color TextureFormat");
    GLenum attachment = flags.depth.format == FORMAT_D24S8 ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT;
    if (flags.depth.readable) {
      canvas->depth.texture = lovrTextureCreate(TEXTURE_2D, NULL, 0, false, flags.mipmaps, flags.msaa);
      lovrTextureAllocate(canvas->depth.texture, width, height, 1, flags.depth.format);
      glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, canvas->depth.texture->id, 0);
    } else {
      GLenum format = convertTextureFormatInternal(flags.depth.format, false);
      glGenRenderbuffers(1, &canvas->depthBuffer);
      glBindRenderbuffer(GL_RENDERBUFFER, canvas->depthBuffer);
      glRenderbufferStorageMultisample(GL_RENDERBUFFER, canvas->flags.msaa, format, width, height);
      glFramebufferRenderbuffer(GL_FRAMEBUFFER, attachment, GL_RENDERBUFFER, canvas->depthBuffer);
    }
  }

  if (flags.msaa) {
    glGenFramebuffers(1, &canvas->resolveBuffer);
  }

  return canvas;
}

Canvas* lovrCanvasInitFromHandle(Canvas* canvas, int width, int height, CanvasFlags flags, uint32_t framebuffer, uint32_t depthBuffer, uint32_t resolveBuffer, int attachmentCount, bool immortal) {
  canvas->framebuffer = framebuffer;
  canvas->depthBuffer = depthBuffer;
  canvas->resolveBuffer = resolveBuffer;
  canvas->attachmentCount = attachmentCount;
  canvas->width = width;
  canvas->height = height;
  canvas->flags = flags;
  canvas->immortal = immortal;
  return canvas;
}

void lovrCanvasDestroy(void* ref) {
  Canvas* canvas = ref;
  lovrGraphicsFlushCanvas(canvas);
  if (!canvas->immortal) {
    glDeleteFramebuffers(1, &canvas->framebuffer);
    glDeleteRenderbuffers(1, &canvas->depthBuffer);
    glDeleteFramebuffers(1, &canvas->resolveBuffer);
  }
  for (int i = 0; i < canvas->attachmentCount; i++) {
    lovrRelease(canvas->attachments[i].texture);
  }
  lovrRelease(canvas->depth.texture);
}

void lovrCanvasResolve(Canvas* canvas) {
  if (!canvas->needsResolve) {
    return;
  }

  lovrGraphicsFlushCanvas(canvas);

  if (canvas->flags.msaa) {
    int w = canvas->width;
    int h = canvas->height;
    glBindFramebuffer(GL_READ_FRAMEBUFFER, canvas->framebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, canvas->resolveBuffer);
    state.framebuffer = canvas->resolveBuffer;

    if (canvas->attachmentCount == 1) {
      glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    } else {
      GLenum buffers[MAX_CANVAS_ATTACHMENTS] = { GL_NONE };
      for (int i = 0; i < canvas->attachmentCount; i++) {
        buffers[i] = GL_COLOR_ATTACHMENT0 + i;
        glReadBuffer(i);
        glDrawBuffers(1, &buffers[i]);
        glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
      }
      glReadBuffer(0);
      glDrawBuffers(canvas->attachmentCount, buffers);
    }
  }

  if (canvas->flags.mipmaps) {
    for (int i = 0; i < canvas->attachmentCount; i++) {
      Texture* texture = canvas->attachments[i].texture;
      if (texture->mipmapCount > 1) {
        lovrGpuBindTexture(texture, 0);
        glGenerateMipmap(texture->target);
      }
    }
  }

  canvas->needsResolve = false;
}

TextureData* lovrCanvasNewTextureData(Canvas* canvas, int index) {
  lovrGraphicsFlushCanvas(canvas);
  lovrGpuBindCanvas(canvas, false);

#ifndef EMSCRIPTEN
  Texture* texture = canvas->attachments[index].texture;
  if ((texture->incoherent >> BARRIER_TEXTURE) & 1) {
    lovrGpuSync(1 << BARRIER_TEXTURE);
  }
#endif

  if (index != 0) {
    glReadBuffer(index);
  }

  TextureData* textureData = lovrTextureDataCreate(canvas->width, canvas->height, 0x0, FORMAT_RGBA);
  glReadPixels(0, 0, canvas->width, canvas->height, GL_RGBA, GL_UNSIGNED_BYTE, textureData->blob.data);

  if (index != 0) {
    glReadBuffer(0);
  }

  return textureData;
}

// Buffer

Buffer* lovrBufferInit(Buffer* buffer, size_t size, void* data, BufferType type, BufferUsage usage, bool readable) {
  buffer->size = size;
  buffer->readable = readable;
  buffer->type = type;
  buffer->usage = usage;
  glGenBuffers(1, &buffer->id);
  lovrGpuBindBuffer(type, buffer->id, false);
  GLenum glType = convertBufferType(type);

#ifndef EMSCRIPTEN
  if (GLAD_GL_ARB_buffer_storage) {
    GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | (readable ? GL_MAP_READ_BIT : 0);
    glBufferStorage(glType, size, data, flags);
    buffer->data = glMapBufferRange(glType, 0, size, flags | GL_MAP_FLUSH_EXPLICIT_BIT);
  } else {
#endif
    buffer->data = calloc(1, size);
    glBufferData(glType, size, data, convertBufferUsage(usage));

    if (data) {
      memcpy(buffer->data, data, size);
    }
#ifndef EMSCRIPTEN
  }
#endif

  return buffer;
}

void lovrBufferDestroy(void* ref) {
  Buffer* buffer = ref;
  lovrGpuDestroySyncResource(buffer, buffer->incoherent);
  glDeleteBuffers(1, &buffer->id);
#ifndef EMSCRIPTEN
  if (!GLAD_GL_ARB_buffer_storage) {
#endif
    free(buffer->data);
#ifndef EMSCRIPTEN
  }
#endif
}

void* lovrBufferMap(Buffer* buffer, size_t offset) {
  return (uint8_t*) buffer->data + offset;
}

void lovrBufferFlush(Buffer* buffer, size_t offset, size_t size) {
  lovrGpuBindBuffer(buffer->type, buffer->id, false);
#ifndef EMSCRIPTEN
  if (GLAD_GL_ARB_buffer_storage) {
    glFlushMappedBufferRange(convertBufferType(buffer->type), offset, size);
  } else {
#endif
    glBufferSubData(convertBufferType(buffer->type), offset, size, (GLvoid*) ((uint8_t*) buffer->data + offset));
#ifndef EMSCRIPTEN
  }
#endif
}

// Shader

static GLuint compileShader(GLenum type, const char** sources, int count) {
  GLuint shader = glCreateShader(type);
  glShaderSource(shader, count, sources, NULL);
  glCompileShader(shader);

  int isShaderCompiled;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &isShaderCompiled);
  if (!isShaderCompiled) {
    int logLength;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
    char* log = malloc(logLength);
    glGetShaderInfoLog(shader, logLength, &logLength, log);
    lovrThrow("Could not compile shader:\n%s", log);
  }

  return shader;
}

static GLuint linkProgram(GLuint program) {
  glLinkProgram(program);

  int isLinked;
  glGetProgramiv(program, GL_LINK_STATUS, &isLinked);
  if (!isLinked) {
    int logLength;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
    char* log = malloc(logLength);
    glGetProgramInfoLog(program, logLength, &logLength, log);
    lovrThrow("Could not link shader:\n%s", log);
  }

  return program;
}

static void lovrShaderSetupUniforms(Shader* shader) {
  uint32_t program = shader->program;
  lovrGpuUseProgram(program); // TODO necessary?

  // Uniform blocks
  int32_t blockCount;
  glGetProgramiv(program, GL_ACTIVE_UNIFORM_BLOCKS, &blockCount);
  lovrAssert(blockCount <= MAX_BLOCK_BUFFERS, "Shader has too many read-only blocks (%d) the max is %d", blockCount, MAX_BLOCK_BUFFERS);
  map_init(&shader->blockMap);
  vec_block_t* uniformBlocks = &shader->blocks[BLOCK_UNIFORM];
  vec_init(uniformBlocks);
  vec_reserve(uniformBlocks, blockCount);
  for (int i = 0; i < blockCount; i++) {
    UniformBlock block = { .slot = i, .source = NULL };
    glUniformBlockBinding(program, i, block.slot);
    vec_init(&block.uniforms);

    char name[LOVR_MAX_UNIFORM_LENGTH];
    glGetActiveUniformBlockName(program, i, LOVR_MAX_UNIFORM_LENGTH, NULL, name);
    int blockId = (i << 1) + BLOCK_UNIFORM;
    map_set(&shader->blockMap, name, blockId);
    vec_push(uniformBlocks, block);
  }

  // Shader storage buffers and their buffer variables
  vec_block_t* storageBlocks = &shader->blocks[BLOCK_STORAGE];
  vec_init(storageBlocks);
#ifndef EMSCRIPTEN
  if (GLAD_GL_ARB_shader_storage_buffer_object && GLAD_GL_ARB_program_interface_query) {

    // Iterate over storage blocks, setting their binding and pushing them onto the block vector
    int storageCount;
    glGetProgramInterfaceiv(program, GL_SHADER_STORAGE_BLOCK, GL_ACTIVE_RESOURCES, &storageCount);
    lovrAssert(storageCount <= MAX_BLOCK_BUFFERS, "Shader has too many writable blocks (%d) the max is %d", storageCount, MAX_BLOCK_BUFFERS);
    vec_reserve(storageBlocks, storageCount);
    for (int i = 0; i < storageCount; i++) {
      UniformBlock block = { .slot = i, .source = NULL };
      glShaderStorageBlockBinding(program, i, block.slot);
      vec_init(&block.uniforms);

      char name[LOVR_MAX_UNIFORM_LENGTH];
      glGetProgramResourceName(program, GL_SHADER_STORAGE_BLOCK, i, LOVR_MAX_UNIFORM_LENGTH, NULL, name);
      int blockId = (i << 1) + BLOCK_STORAGE;
      map_set(&shader->blockMap, name, blockId);
      vec_push(storageBlocks, block);
    }

    // Iterate over buffer variables, pushing them onto the uniform list of the correct block
    int bufferVariableCount;
    glGetProgramInterfaceiv(program, GL_BUFFER_VARIABLE, GL_ACTIVE_RESOURCES, &bufferVariableCount);
    for (int i = 0; i < bufferVariableCount; i++) {
      Uniform uniform;
      enum { blockIndex, offset, glType, count, arrayStride, matrixStride, propCount };
      int values[propCount];
      GLenum properties[propCount] = { GL_BLOCK_INDEX, GL_OFFSET, GL_TYPE, GL_ARRAY_SIZE, GL_ARRAY_STRIDE, GL_MATRIX_STRIDE };
      glGetProgramResourceiv(program, GL_BUFFER_VARIABLE, i, propCount, properties, sizeof(values), NULL, values);
      glGetProgramResourceName(program, GL_BUFFER_VARIABLE, i, LOVR_MAX_UNIFORM_LENGTH, NULL, uniform.name);
      uniform.type = getUniformType(values[glType], uniform.name);
      uniform.components = getUniformComponents(uniform.type);
      uniform.count = values[count];
      uniform.offset = values[offset];
      if (uniform.count > 1) {
        uniform.size = uniform.count * values[arrayStride];
      } else if (uniform.type == UNIFORM_MATRIX) {
        uniform.size = values[matrixStride] * uniform.components;
      } else {
        uniform.size = 4 * (uniform.components == 3 ? 4 : uniform.components);
      }
      vec_push(&storageBlocks->data[values[blockIndex]].uniforms, uniform);
    }
  }
#endif

  // Uniform introspection
  int32_t uniformCount;
  int textureSlot = 0;
  int imageSlot = 0;
  map_init(&shader->uniformMap);
  vec_init(&shader->uniforms);
  glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &uniformCount);
  for (uint32_t i = 0; i < (uint32_t) uniformCount; i++) {
    Uniform uniform;
    GLenum glType;
    glGetActiveUniform(program, i, LOVR_MAX_UNIFORM_LENGTH, NULL, &uniform.count, &glType, uniform.name);

    char* subscript = strchr(uniform.name, '[');
    if (subscript) {
      *subscript = '\0';
    }

    uniform.location = glGetUniformLocation(program, uniform.name);
    uniform.type = getUniformType(glType, uniform.name);
    uniform.components = getUniformComponents(glType);
#ifdef EMSCRIPTEN
    uniform.image = false;
#else
    uniform.image = glType == GL_IMAGE_2D || glType == GL_IMAGE_3D || glType == GL_IMAGE_CUBE || glType == GL_IMAGE_2D_ARRAY;
#endif
    uniform.textureType = getUniformTextureType(glType);
    uniform.baseSlot = uniform.type == UNIFORM_SAMPLER ? textureSlot : (uniform.type == UNIFORM_IMAGE ? imageSlot : -1);

    int blockIndex;
    glGetActiveUniformsiv(program, 1, &i, GL_UNIFORM_BLOCK_INDEX, &blockIndex);

    if (blockIndex != -1) {
      UniformBlock* block = &shader->blocks[BLOCK_UNIFORM].data[blockIndex];
      glGetActiveUniformsiv(program, 1, &i, GL_UNIFORM_OFFSET, &uniform.offset);
      glGetActiveUniformsiv(program, 1, &i, GL_UNIFORM_SIZE, &uniform.count);
      if (uniform.count > 1) {
        int stride;
        glGetActiveUniformsiv(program, 1, &i, GL_UNIFORM_ARRAY_STRIDE, &stride);
        uniform.size = stride * uniform.count;
      } else if (uniform.type == UNIFORM_MATRIX) {
        int matrixStride;
        glGetActiveUniformsiv(program, 1, &i, GL_UNIFORM_MATRIX_STRIDE, &matrixStride);
        uniform.size = uniform.components * matrixStride;
      } else {
        uniform.size = 4 * (uniform.components == 3 ? 4 : uniform.components);
      }

      vec_push(&block->uniforms, uniform);
      continue;
    } else if (uniform.location == -1) {
      continue;
    }

    switch (uniform.type) {
      case UNIFORM_FLOAT:
        uniform.size = uniform.components * uniform.count * sizeof(float);
        uniform.value.data = calloc(1, uniform.size);
        break;

      case UNIFORM_INT:
        uniform.size = uniform.components * uniform.count * sizeof(int);
        uniform.value.data = calloc(1, uniform.size);
        break;

      case UNIFORM_MATRIX:
        uniform.size = uniform.components * uniform.components * uniform.count * sizeof(float);
        uniform.value.data = calloc(1, uniform.size);
        break;

      case UNIFORM_SAMPLER:
      case UNIFORM_IMAGE:
        uniform.size = uniform.count * (uniform.type == UNIFORM_SAMPLER ? sizeof(Texture*) : sizeof(Image));
        uniform.value.data = calloc(1, uniform.size);

        // Use the value for ints to bind texture slots, but use the value for textures afterwards.
        for (int i = 0; i < uniform.count; i++) {
          uniform.value.ints[i] = uniform.baseSlot + i;
        }
        glUniform1iv(uniform.location, uniform.count, uniform.value.ints);
        memset(uniform.value.data, 0, uniform.size);
        break;
    }

    size_t offset = 0;
    for (int j = 0; j < uniform.count; j++) {
      int location = uniform.location;

      if (uniform.count > 1) {
        char name[LOVR_MAX_UNIFORM_LENGTH];
        snprintf(name, LOVR_MAX_UNIFORM_LENGTH, "%s[%d]", uniform.name, j);
        location = glGetUniformLocation(program, name);
      }

      switch (uniform.type) {
        case UNIFORM_FLOAT: glGetUniformfv(program, location, &uniform.value.floats[offset]); break;
        case UNIFORM_INT: glGetUniformiv(program, location, &uniform.value.ints[offset]); break;
        case UNIFORM_MATRIX: glGetUniformfv(program, location, &uniform.value.floats[offset]); break;
        default: break;
      }

      offset += uniform.components * (uniform.type == UNIFORM_MATRIX ? uniform.components : 1);
    }

    map_set(&shader->uniformMap, uniform.name, shader->uniforms.length);
    vec_push(&shader->uniforms, uniform);
    textureSlot += uniform.type == UNIFORM_SAMPLER ? uniform.count : 0;
    imageSlot += uniform.type == UNIFORM_IMAGE ? uniform.count : 0;
  }
}

Shader* lovrShaderInitGraphics(Shader* shader, const char* vertexSource, const char* fragmentSource) {
#if defined(EMSCRIPTEN) || defined(__ANDROID__)
  const char* vertexHeader = "#version 300 es\nprecision mediump float;\nprecision mediump int;\n";
  const char* fragmentHeader = vertexHeader;
#else
  const char* vertexHeader = state.features.computeShaders ? "#version 430\n" : "#version 150\n";
  const char* fragmentHeader = "#version 150\nin vec4 gl_FragCoord;\n";
#endif

  const char* vertexSinglepass = state.features.singlepass ?
    "#extension GL_AMD_vertex_shader_viewport_index : require\n" "#define SINGLEPASS 1\n" :
    "#define SINGLEPASS 0\n";

  const char* fragmentSinglepass = state.features.singlepass ?
    "#extension GL_ARB_fragment_layer_viewport : require\n" "#define SINGLEPASS 1\n" :
    "#define SINGLEPASS 0\n";

  size_t maxDraws = MIN(state.limits.blockSize / (20 * sizeof(float)) / 64 * 64, 256);
  char maxDrawSource[32];
  snprintf(maxDrawSource, 31, "#define MAX_DRAWS %zu\n", maxDraws);

  // Vertex
  vertexSource = vertexSource == NULL ? lovrDefaultVertexShader : vertexSource;
  const char* vertexSources[] = { vertexHeader, vertexSinglepass, maxDrawSource, lovrShaderVertexPrefix, vertexSource, lovrShaderVertexSuffix };
  GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSources, sizeof(vertexSources) / sizeof(vertexSources[0]));

  // Fragment
  fragmentSource = fragmentSource == NULL ? lovrDefaultFragmentShader : fragmentSource;
  const char* fragmentSources[] = { fragmentHeader, fragmentSinglepass, lovrShaderFragmentPrefix, fragmentSource, lovrShaderFragmentSuffix };
  GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSources, sizeof(fragmentSources) / sizeof(fragmentSources[0]));

  // Link
  uint32_t program = glCreateProgram();
  glAttachShader(program, vertexShader);
  glAttachShader(program, fragmentShader);
  glBindAttribLocation(program, LOVR_SHADER_POSITION, "lovrPosition");
  glBindAttribLocation(program, LOVR_SHADER_NORMAL, "lovrNormal");
  glBindAttribLocation(program, LOVR_SHADER_TEX_COORD, "lovrTexCoord");
  glBindAttribLocation(program, LOVR_SHADER_VERTEX_COLOR, "lovrVertexColor");
  glBindAttribLocation(program, LOVR_SHADER_TANGENT, "lovrTangent");
  glBindAttribLocation(program, LOVR_SHADER_BONES, "lovrBones");
  glBindAttribLocation(program, LOVR_SHADER_BONE_WEIGHTS, "lovrBoneWeights");
  glBindAttribLocation(program, LOVR_SHADER_DRAW_ID, "lovrDrawID");
  linkProgram(program);
  glDetachShader(program, vertexShader);
  glDeleteShader(vertexShader);
  glDetachShader(program, fragmentShader);
  glDeleteShader(fragmentShader);
  shader->program = program;
  shader->type = SHADER_GRAPHICS;

  // Generic attributes
  lovrGpuUseProgram(program);
  glVertexAttrib4fv(LOVR_SHADER_VERTEX_COLOR, (float[4]) { 1., 1., 1., 1. });
  glVertexAttribI4iv(LOVR_SHADER_BONES, (int[4]) { 0., 0., 0., 0. });
  glVertexAttrib4fv(LOVR_SHADER_BONE_WEIGHTS, (float[4]) { 1., 0., 0., 0. });

  lovrShaderSetupUniforms(shader);

  // Attribute cache
  int32_t attributeCount;
  glGetProgramiv(program, GL_ACTIVE_ATTRIBUTES, &attributeCount);
  map_init(&shader->attributes);
  for (int i = 0; i < attributeCount; i++) {
    char name[LOVR_MAX_ATTRIBUTE_LENGTH];
    GLint size;
    GLenum type;
    glGetActiveAttrib(program, i, LOVR_MAX_ATTRIBUTE_LENGTH, NULL, &size, &type, name);
    map_set(&shader->attributes, name, glGetAttribLocation(program, name));
  }

  return shader;
}

Shader* lovrShaderInitCompute(Shader* shader, const char* source) {
#ifdef EMSCRIPTEN
  lovrThrow("Compute shaders are not supported on this system");
#else
  lovrAssert(GLAD_GL_ARB_compute_shader, "Compute shaders are not supported on this system");
  const char* sources[] = { lovrShaderComputePrefix, source, lovrShaderComputeSuffix };
  GLuint computeShader = compileShader(GL_COMPUTE_SHADER, sources, sizeof(sources) / sizeof(sources[0]));
  GLuint program = glCreateProgram();
  glAttachShader(program, computeShader);
  linkProgram(program);
  glDetachShader(program, computeShader);
  glDeleteShader(computeShader);
  shader->program = program;
  shader->type = SHADER_COMPUTE;
  lovrShaderSetupUniforms(shader);
#endif
  return shader;
}

void lovrShaderDestroy(void* ref) {
  Shader* shader = ref;
  lovrGraphicsFlushShader(shader);
  glDeleteProgram(shader->program);
  for (int i = 0; i < shader->uniforms.length; i++) {
    free(shader->uniforms.data[i].value.data);
  }
  for (BlockType type = BLOCK_UNIFORM; type <= BLOCK_STORAGE; type++) {
    UniformBlock* block; int i;
    vec_foreach_ptr(&shader->blocks[type], block, i) {
      lovrRelease(block->source);
    }
  }
  vec_deinit(&shader->uniforms);
  vec_deinit(&shader->blocks[BLOCK_UNIFORM]);
  vec_deinit(&shader->blocks[BLOCK_STORAGE]);
  map_deinit(&shader->attributes);
  map_deinit(&shader->uniformMap);
  map_deinit(&shader->blockMap);
}

// Mesh

Mesh* lovrMeshInit(Mesh* mesh, DrawMode mode, VertexFormat format, Buffer* vertexBuffer, uint32_t vertexCount) {
  mesh->mode = mode;
  mesh->format = format;
  mesh->vertexBuffer = vertexBuffer;
  mesh->vertexCount = vertexCount;
  lovrRetain(mesh->vertexBuffer);
  glGenVertexArrays(1, &mesh->vao);

  map_init(&mesh->attributes);
  for (int i = 0; i < format.count; i++) {
    lovrRetain(mesh->vertexBuffer);
    map_set(&mesh->attributes, format.attributes[i].name, ((MeshAttribute) {
      .buffer = mesh->vertexBuffer,
      .offset = format.attributes[i].offset,
      .stride = format.stride,
      .type = format.attributes[i].type,
      .components = format.attributes[i].count,
      .integer = format.attributes[i].type == ATTR_INT,
      .enabled = true
    }));
  }

  return mesh;
}

void lovrMeshDestroy(void* ref) {
  Mesh* mesh = ref;
  lovrGraphicsFlushMesh(mesh);
  glDeleteVertexArrays(1, &mesh->vao);
  const char* key;
  map_iter_t iter = map_iter(&mesh->attributes);
  while ((key = map_next(&mesh->attributes, &iter)) != NULL) {
    MeshAttribute* attribute = map_get(&mesh->attributes, key);
    lovrRelease(attribute->buffer);
  }
  map_deinit(&mesh->attributes);
  lovrRelease(mesh->vertexBuffer);
  lovrRelease(mesh->indexBuffer);
  lovrRelease(mesh->material);
}

void lovrMeshSetIndexBuffer(Mesh* mesh, Buffer* buffer, uint32_t indexCount, size_t indexSize) {
  if (mesh->indexBuffer != buffer || mesh->indexCount != indexCount || mesh->indexSize != indexSize) {
    lovrGraphicsFlushMesh(mesh);
    lovrRetain(buffer);
    lovrRelease(mesh->indexBuffer);
    mesh->indexBuffer = buffer;
    mesh->indexCount = indexCount;
    mesh->indexSize = indexSize;
  }
}
