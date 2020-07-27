#include "graphics/graphics.h"
#include "graphics/buffer.h"
#include "graphics/canvas.h"
#include "graphics/material.h"
#include "graphics/mesh.h"
#include "graphics/shader.h"
#include "graphics/texture.h"
#include "resources/shaders.h"
#include "data/modelData.h"
#include "math/math.h"
#include "core/ref.h"
#include <math.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef LOVR_WEBGL
#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>
#include <GL/gl.h>
#include <GL/glext.h>
#else
#include "lib/glad/glad.h"
#endif

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

struct Buffer {
  uint32_t id;
  void* data;
  size_t size;
  size_t flushFrom;
  size_t flushTo;
  BufferType type;
  BufferUsage usage;
  bool mapped;
  bool readable;
  uint8_t incoherent;
};

struct Texture {
  GLuint id;
  GLuint msaaId;
  GLenum target;
  TextureType type;
  TextureFormat format;
  uint32_t width;
  uint32_t height;
  uint32_t depth;
  uint32_t mipmapCount;
  CompareMode compareMode;
  TextureFilter filter;
  TextureWrap wrap;
  uint32_t msaa;
  bool srgb;
  bool mipmaps;
  bool allocated;
  bool native;
  uint8_t incoherent;
};

struct Canvas {
  uint32_t framebuffer;
  uint32_t resolveBuffer;
  uint32_t depthBuffer;
  uint32_t width;
  uint32_t height;
  CanvasFlags flags;
  Attachment attachments[MAX_CANVAS_ATTACHMENTS];
  Attachment depth;
  uint32_t attachmentCount;
  bool needsAttach;
  bool needsResolve;
  bool immortal;
};

struct ShaderBlock {
  BlockType type;
  arr_uniform_t uniforms;
  map_t uniformMap;
  struct Buffer* buffer;
};

struct Shader {
  uint32_t program;
  ShaderType type;
  arr_uniform_t uniforms;
  arr_block_t blocks[2];
  map_t attributes;
  map_t uniformMap;
  map_t blockMap;
  bool multiview;
};

struct Mesh {
  uint32_t vao;
  uint32_t ibo;
  DrawMode mode;
  char attributeNames[MAX_ATTRIBUTES][MAX_ATTRIBUTE_NAME_LENGTH];
  MeshAttribute attributes[MAX_ATTRIBUTES];
  uint8_t locations[MAX_ATTRIBUTES];
  uint16_t enabledLocations;
  uint16_t divisors[MAX_ATTRIBUTES];
  map_t attributeMap;
  uint32_t attributeCount;
  struct Buffer* vertexBuffer;
  struct Buffer* indexBuffer;
  uint32_t vertexCount;
  uint32_t indexCount;
  size_t indexSize;
  size_t indexOffset;
  uint32_t drawStart;
  uint32_t drawCount;
  struct Material* material;
};

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

typedef struct {
  GLuint* queries;
  uint32_t* chain;
  uint32_t next;
  uint32_t count;
} QueryPool;

typedef struct {
  uint32_t head;
  uint32_t tail;
  uint64_t nanoseconds;
} Timer;

static struct {
  Texture* defaultTexture;
  enum { NONE, INSTANCED_STEREO, MULTIVIEW } singlepass;
  bool alphaToCoverage;
  bool blendEnabled;
  BlendMode blendMode;
  BlendAlphaMode blendAlphaMode;
  uint8_t colorMask;
  bool culling;
  bool depthEnabled;
  CompareMode depthTest;
  bool depthWrite;
  float lineWidth;
  uint32_t primitiveRestart;
  bool stencilEnabled;
  CompareMode stencilMode;
  int stencilValue;
  bool stencilWriting;
  Winding winding;
  bool wireframe;
  uint32_t framebuffer;
  uint32_t program;
  Mesh* vertexArray;
  uint32_t buffers[MAX_BUFFER_TYPES];
  BlockBuffer blockBuffers[2][MAX_BLOCK_BUFFERS];
  int activeTexture;
  Texture* textures[MAX_TEXTURES];
  Image images[MAX_IMAGES];
  float viewports[2][4];
  uint32_t viewportCount;
  arr_t(void*) incoherents[MAX_BARRIERS];
  QueryPool queryPool;
  arr_t(Timer) timers;
  uint32_t activeTimer;
  map_t timerMap;
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
    default: lovrThrow("Unreachable");
  }
}

static GLenum convertWrapMode(WrapMode mode) {
  switch (mode) {
    case WRAP_CLAMP: return GL_CLAMP_TO_EDGE;
    case WRAP_REPEAT: return GL_REPEAT;
    case WRAP_MIRRORED_REPEAT: return GL_MIRRORED_REPEAT;
    default: lovrThrow("Unreachable");
  }
}

static GLenum convertTextureTarget(TextureType type) {
  switch (type) {
    case TEXTURE_2D: return GL_TEXTURE_2D; break;
    case TEXTURE_ARRAY: return GL_TEXTURE_2D_ARRAY; break;
    case TEXTURE_CUBE: return GL_TEXTURE_CUBE_MAP; break;
    case TEXTURE_VOLUME: return GL_TEXTURE_3D; break;
    default: lovrThrow("Unreachable");
  }
}

static GLenum convertTextureFormat(TextureFormat format) {
  switch (format) {
    case FORMAT_RGB: return GL_RGB;
    case FORMAT_RGBA: return GL_RGBA;
    case FORMAT_RGBA4: return GL_RGBA;
    case FORMAT_R16: return GL_RED;
    case FORMAT_RG16: return GL_RG;
    case FORMAT_RGBA16: return GL_RGBA;
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
    case FORMAT_ASTC_4x4:
    case FORMAT_ASTC_5x4:
    case FORMAT_ASTC_5x5:
    case FORMAT_ASTC_6x5:
    case FORMAT_ASTC_6x6:
    case FORMAT_ASTC_8x5:
    case FORMAT_ASTC_8x6:
    case FORMAT_ASTC_8x8:
    case FORMAT_ASTC_10x5:
    case FORMAT_ASTC_10x6:
    case FORMAT_ASTC_10x8:
    case FORMAT_ASTC_10x10:
    case FORMAT_ASTC_12x10:
    case FORMAT_ASTC_12x12:
      return GL_RGBA;
    default: lovrThrow("Unreachable");
  }
}

static GLenum convertTextureFormatInternal(TextureFormat format, bool srgb) {
  switch (format) {
    case FORMAT_RGB: return srgb ? GL_SRGB8 : GL_RGB8;
    case FORMAT_RGBA: return srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8;
    case FORMAT_RGBA4: return GL_RGBA4;
    case FORMAT_R16: return GL_R16;
    case FORMAT_RG16: return GL_RG16;
    case FORMAT_RGBA16: return GL_RGBA16;
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
#ifdef LOVR_WEBGL
    case FORMAT_ASTC_4x4: return srgb ? GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR : GL_COMPRESSED_RGBA_ASTC_4x4_KHR;
    case FORMAT_ASTC_5x4: return srgb ? GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR : GL_COMPRESSED_RGBA_ASTC_5x4_KHR;
    case FORMAT_ASTC_5x5: return srgb ? GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR : GL_COMPRESSED_RGBA_ASTC_5x5_KHR;
    case FORMAT_ASTC_6x5: return srgb ? GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR : GL_COMPRESSED_RGBA_ASTC_6x5_KHR;
    case FORMAT_ASTC_6x6: return srgb ? GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR : GL_COMPRESSED_RGBA_ASTC_6x6_KHR;
    case FORMAT_ASTC_8x5: return srgb ? GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR : GL_COMPRESSED_RGBA_ASTC_8x5_KHR;
    case FORMAT_ASTC_8x6: return srgb ? GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR : GL_COMPRESSED_RGBA_ASTC_8x6_KHR;
    case FORMAT_ASTC_8x8: return srgb ? GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR : GL_COMPRESSED_RGBA_ASTC_8x8_KHR;
    case FORMAT_ASTC_10x5: return srgb ? GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR : GL_COMPRESSED_RGBA_ASTC_10x5_KHR;
    case FORMAT_ASTC_10x6: return srgb ? GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR : GL_COMPRESSED_RGBA_ASTC_10x6_KHR;
    case FORMAT_ASTC_10x8: return srgb ? GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR : GL_COMPRESSED_RGBA_ASTC_10x8_KHR;
    case FORMAT_ASTC_10x10: return srgb ? GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR : GL_COMPRESSED_RGBA_ASTC_10x10_KHR;
    case FORMAT_ASTC_12x10: return srgb ? GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR : GL_COMPRESSED_RGBA_ASTC_12x10_KHR;
    case FORMAT_ASTC_12x12: return srgb ? GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR : GL_COMPRESSED_RGBA_ASTC_12x12_KHR;
#else
    case FORMAT_ASTC_4x4: return srgb ? GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4 : GL_COMPRESSED_RGBA_ASTC_4x4;
    case FORMAT_ASTC_5x4: return srgb ? GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4 : GL_COMPRESSED_RGBA_ASTC_5x4;
    case FORMAT_ASTC_5x5: return srgb ? GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5 : GL_COMPRESSED_RGBA_ASTC_5x5;
    case FORMAT_ASTC_6x5: return srgb ? GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5 : GL_COMPRESSED_RGBA_ASTC_6x5;
    case FORMAT_ASTC_6x6: return srgb ? GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6 : GL_COMPRESSED_RGBA_ASTC_6x6;
    case FORMAT_ASTC_8x5: return srgb ? GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5 : GL_COMPRESSED_RGBA_ASTC_8x5;
    case FORMAT_ASTC_8x6: return srgb ? GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6 : GL_COMPRESSED_RGBA_ASTC_8x6;
    case FORMAT_ASTC_8x8: return srgb ? GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8 : GL_COMPRESSED_RGBA_ASTC_8x8;
    case FORMAT_ASTC_10x5: return srgb ? GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5 : GL_COMPRESSED_RGBA_ASTC_10x5;
    case FORMAT_ASTC_10x6: return srgb ? GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6 : GL_COMPRESSED_RGBA_ASTC_10x6;
    case FORMAT_ASTC_10x8: return srgb ? GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8 : GL_COMPRESSED_RGBA_ASTC_10x8;
    case FORMAT_ASTC_10x10: return srgb ? GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10 : GL_COMPRESSED_RGBA_ASTC_10x10;
    case FORMAT_ASTC_12x10: return srgb ? GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10 : GL_COMPRESSED_RGBA_ASTC_12x10;
    case FORMAT_ASTC_12x12: return srgb ? GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12 : GL_COMPRESSED_RGBA_ASTC_12x12;
#endif
    default: lovrThrow("Unreachable");
  }
}

static GLenum convertTextureFormatType(TextureFormat format) {
  switch (format) {
    case FORMAT_RGB: return GL_UNSIGNED_BYTE;
    case FORMAT_RGBA: return GL_UNSIGNED_BYTE;
    case FORMAT_RGBA4: return GL_UNSIGNED_SHORT_4_4_4_4;
    case FORMAT_R16: return GL_UNSIGNED_SHORT;
    case FORMAT_RG16: return GL_UNSIGNED_SHORT;
    case FORMAT_RGBA16: return GL_UNSIGNED_SHORT;
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
    default: lovrThrow("Unreachable");
  }
}

static bool isTextureFormatCompressed(TextureFormat format) {
  switch (format) {
    case FORMAT_DXT1:
    case FORMAT_DXT3:
    case FORMAT_DXT5:
    case FORMAT_ASTC_4x4:
    case FORMAT_ASTC_5x4:
    case FORMAT_ASTC_5x5:
    case FORMAT_ASTC_6x5:
    case FORMAT_ASTC_6x6:
    case FORMAT_ASTC_8x5:
    case FORMAT_ASTC_8x6:
    case FORMAT_ASTC_8x8:
    case FORMAT_ASTC_10x5:
    case FORMAT_ASTC_10x6:
    case FORMAT_ASTC_10x8:
    case FORMAT_ASTC_10x10:
    case FORMAT_ASTC_12x10:
    case FORMAT_ASTC_12x12:
      return true;
    default:
      return false;
  }
}

static bool isTextureFormatDepth(TextureFormat format) {
  switch (format) {
    case FORMAT_D16: case FORMAT_D32F: case FORMAT_D24S8: return true;
    default: return false;
  }
}

static uint64_t getTextureMemorySize(Texture* texture) {
  if (texture->native) return 0;
  float size = 0.f;
  float bitrate;
  switch (texture->format) {
    case FORMAT_RGB: bitrate = 24.f; break;
    case FORMAT_RGBA: bitrate = 32.f; break;
    case FORMAT_RGBA4: bitrate = 16.f; break;
    case FORMAT_R16: bitrate = 16.f; break;
    case FORMAT_RG16: bitrate = 32.f; break;
    case FORMAT_RGBA16: bitrate = 64.f; break;
    case FORMAT_RGBA16F: bitrate = 64.f; break;
    case FORMAT_RGBA32F: bitrate = 128.f; break;
    case FORMAT_R16F: bitrate = 16.f; break;
    case FORMAT_R32F: bitrate = 32.f; break;
    case FORMAT_RG16F: bitrate = 32.f; break;
    case FORMAT_RG32F: bitrate = 64.f; break;
    case FORMAT_RGB5A1: bitrate = 16.f; break;
    case FORMAT_RGB10A2: bitrate = 32.f; break;
    case FORMAT_RG11B10F: bitrate = 32.f; break;
    case FORMAT_D16: bitrate = 16.f; break;
    case FORMAT_D32F: bitrate = 32.f; break;
    case FORMAT_D24S8: bitrate = 32.f; break;
    case FORMAT_DXT1: bitrate = 4.f; break;
    case FORMAT_DXT3: bitrate = 8.f; break;
    case FORMAT_DXT5: bitrate = 8.f; break;
    // Divide fixed-size 128-bit blocks by block size:
    case FORMAT_ASTC_4x4: bitrate = 8.00f; break;
    case FORMAT_ASTC_5x4: bitrate = 6.40f; break;
    case FORMAT_ASTC_5x5: bitrate = 5.12f; break;
    case FORMAT_ASTC_6x5: bitrate = 4.27f; break;
    case FORMAT_ASTC_6x6: bitrate = 3.56f; break;
    case FORMAT_ASTC_8x5: bitrate = 3.20f; break;
    case FORMAT_ASTC_8x6: bitrate = 2.67f; break;
    case FORMAT_ASTC_8x8: bitrate = 2.00f; break;
    case FORMAT_ASTC_10x5: bitrate = 2.56f; break;
    case FORMAT_ASTC_10x6: bitrate = 2.13f; break;
    case FORMAT_ASTC_10x8: bitrate = 1.60f; break;
    case FORMAT_ASTC_10x10: bitrate = 1.28f; break;
    case FORMAT_ASTC_12x10: bitrate = 1.07f; break;
    case FORMAT_ASTC_12x12: bitrate = 0.89f; break;
    default: lovrThrow("Unreachable");
  }
  size = texture->width * texture->height * texture->depth * (bitrate / 8.f) * (texture->mipmaps ? 1.33f : 1.f);
  size += texture->msaa > 1 ? (texture->width * texture->height * texture->msaa * (bitrate / 8.f)) : 0.f;
  return (uint64_t) (size + .5f);
}

static GLenum convertAttributeType(AttributeType type) {
  switch (type) {
    case I8: return GL_BYTE;
    case U8: return GL_UNSIGNED_BYTE;
    case I16: return GL_SHORT;
    case U16: return GL_UNSIGNED_SHORT;
    case I32: return GL_INT;
    case U32: return GL_UNSIGNED_INT;
    case F32: return GL_FLOAT;
    default: lovrThrow("Unreachable");
  }
}

static GLenum convertBufferType(BufferType type) {
  switch (type) {
    case BUFFER_VERTEX: return GL_ARRAY_BUFFER;
    case BUFFER_INDEX: return GL_ELEMENT_ARRAY_BUFFER;
    case BUFFER_UNIFORM: return GL_UNIFORM_BUFFER;
    case BUFFER_SHADER_STORAGE: return GL_SHADER_STORAGE_BUFFER;
    case BUFFER_GENERIC: return GL_COPY_WRITE_BUFFER;
    default: lovrThrow("Unreachable");
  }
}

static GLenum convertBufferUsage(BufferUsage usage) {
  switch (usage) {
    case USAGE_STATIC: return GL_STATIC_DRAW;
    case USAGE_DYNAMIC: return GL_DYNAMIC_DRAW;
    case USAGE_STREAM: return GL_STREAM_DRAW;
    default: lovrThrow("Unreachable");
  }
}

#ifndef LOVR_WEBGL
static GLenum convertAccess(UniformAccess access) {
  switch (access) {
    case ACCESS_READ: return GL_READ_ONLY;
    case ACCESS_WRITE: return GL_WRITE_ONLY;
    case ACCESS_READ_WRITE: return GL_READ_WRITE;
    default: lovrThrow("Unreachable");
  }
}
#endif

static GLenum convertTopology(DrawMode topology) {
  switch (topology) {
    case DRAW_POINTS: return GL_POINTS;
    case DRAW_LINES: return GL_LINES;
    case DRAW_LINE_STRIP: return GL_LINE_STRIP;
    case DRAW_LINE_LOOP: return GL_LINE_LOOP;
    case DRAW_TRIANGLE_STRIP: return GL_TRIANGLE_STRIP;
    case DRAW_TRIANGLES: return GL_TRIANGLES;
    case DRAW_TRIANGLE_FAN: return GL_TRIANGLE_FAN;
    default: lovrThrow("Unreachable");
  }
}

static bool isAttributeTypeInteger(GLenum type) {
  switch (type) {
    case GL_INT:
    case GL_INT_VEC2:
    case GL_INT_VEC3:
    case GL_INT_VEC4:
    case GL_UNSIGNED_INT:
    case GL_UNSIGNED_INT_VEC2:
    case GL_UNSIGNED_INT_VEC3:
    case GL_UNSIGNED_INT_VEC4:
      return true;
    default:
      return false;
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
    case GL_SAMPLER_2D_SHADOW:
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
    case GL_SAMPLER_2D_SHADOW: return TEXTURE_2D;
#ifdef GL_ARB_shader_image_load_store
    case GL_IMAGE_2D: return TEXTURE_2D;
    case GL_IMAGE_3D: return TEXTURE_VOLUME;
    case GL_IMAGE_CUBE: return TEXTURE_CUBE;
    case GL_IMAGE_2D_ARRAY: return TEXTURE_ARRAY;
#endif
    default: return -1;
  }
}

static size_t getUniformTypeLength(const Uniform* uniform) {
  size_t size = 0;

  if (uniform->count > 1) {
    size += 2 + floor(log10(uniform->count)) + 1; // "[count]"
  }

  switch (uniform->type) {
    case UNIFORM_MATRIX: size += 4; break;
    case UNIFORM_FLOAT: size += uniform->components == 1 ? 5 : 4; break;
    case UNIFORM_INT: size += uniform->components == 1 ? 3 : 5; break;
    default: break;
  }

  return size;
}

static const char* getUniformTypeName(const Uniform* uniform) {
  switch (uniform->type) {
    case UNIFORM_FLOAT:
      switch (uniform->components) {
        case 1: return "float";
        case 2: return "vec2";
        case 3: return "vec3";
        case 4: return "vec4";
      }
      break;

    case UNIFORM_INT:
      switch (uniform->components) {
        case 1: return "int";
        case 2: return "ivec2";
        case 3: return "ivec3";
        case 4: return "ivec4";
      }
      break;

    case UNIFORM_MATRIX:
      switch (uniform->components) {
        case 2: return "mat2";
        case 3: return "mat3";
        case 4: return "mat4";
      }
      break;

    default: break;
  }

  lovrThrow("Unreachable");
  return "";
}

// Syncing resources is only relevant for compute shaders
#ifndef LOVR_WEBGL
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
      for (size_t j = 0; j < state.incoherents[i].length; j++) {
        Buffer* buffer = state.incoherents[i].data[j];
        buffer->incoherent &= ~(1 << i);
      }
    } else {
      for (size_t j = 0; j < state.incoherents[i].length; j++) {
        Texture* texture = state.incoherents[i].data[j];
        texture->incoherent &= ~(1 << i);
      }
    }

    arr_clear(&state.incoherents[i]);

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

  for (uint32_t i = 0; i < MAX_BARRIERS; i++) {
    if (incoherent & (1 << i)) {
      for (size_t j = 0; j < state.incoherents[i].length; j++) {
        if (state.incoherents[i].data[j] == resource) {
          arr_splice(&state.incoherents[i], j, 1);
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
    state.stats.renderPasses++;
  }
}

static void lovrGpuUseProgram(uint32_t program) {
  if (state.program != program) {
    state.program = program;
    glUseProgram(program);
    state.stats.shaderSwitches++;
  }
}

static void lovrGpuBindVertexArray(Mesh* vertexArray) {
  if (state.vertexArray != vertexArray) {
    state.vertexArray = vertexArray;
    glBindVertexArray(vertexArray->vao);
  }
}

static void lovrGpuBindBuffer(BufferType type, uint32_t buffer) {
  if (type == BUFFER_INDEX && state.vertexArray) {
    if (buffer != state.vertexArray->ibo) {
      state.vertexArray->ibo = buffer;
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer);
    }
  } else {
    if (state.buffers[type] != buffer) {
      state.buffers[type] = buffer;
      glBindBuffer(convertBufferType(type), buffer);
    }
  }
}

static void lovrGpuBindBlockBuffer(BlockType type, uint32_t buffer, int slot, size_t offset, size_t size) {
  lovrAssert(offset % state.limits.blockAlign == 0, "Block buffer offset must be aligned to %d", state.limits.blockAlign);
#ifdef LOVR_WEBGL
  lovrAssert(type == BLOCK_UNIFORM, "Compute blocks are not supported on this system");
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

    // Binding to an indexed target also binds to the generic target
    BufferType bufferType = type == BLOCK_UNIFORM ? BUFFER_UNIFORM : BUFFER_SHADER_STORAGE;
    state.buffers[bufferType] = buffer;
  }
}

static void lovrGpuBindTexture(Texture* texture, int slot) {
  lovrAssert(slot >= 0 && slot < MAX_TEXTURES, "Invalid texture slot %d", slot);
  texture = texture ? texture : state.defaultTexture;

  if (texture != state.textures[slot]) {
    lovrRetain(texture);
    lovrRelease(Texture, state.textures[slot]);
    state.textures[slot] = texture;
    if (state.activeTexture != slot) {
      glActiveTexture(GL_TEXTURE0 + slot);
      state.activeTexture = slot;
    }
    glBindTexture(texture->target, texture->id);
  }
}

#ifndef LOVR_WEBGL
static void lovrGpuBindImage(Image* image, int slot, const char* name) {
  lovrAssert(slot >= 0 && slot < MAX_IMAGES, "Invalid image slot %d", slot);

  // This is a risky way to compare the two structs
  if (memcmp(state.images + slot, image, sizeof(Image))) {
    Texture* texture = image->texture;
    lovrAssert(texture, "No Texture bound to image uniform '%s'", name);
    lovrAssert(!texture->srgb, "Attempt to bind sRGB texture to image uniform '%s'", name);
    lovrAssert(!isTextureFormatCompressed(texture->format), "Attempt to bind compressed texture to image uniform '%s'", name);
    lovrAssert(texture->format != FORMAT_RGB && texture->format != FORMAT_RGBA4 && texture->format != FORMAT_RGB5A1, "Unsupported texture format for image uniform '%s'", name);
    lovrAssert(image->mipmap < (int) texture->mipmapCount, "Invalid mipmap level '%d' for image uniform '%s'", image->mipmap, name);
    lovrAssert(image->slice < (int) texture->depth, "Invalid texture slice '%d' for image uniform '%s'", image->slice, name);
    GLenum glAccess = convertAccess(image->access);
    GLenum glFormat = convertTextureFormatInternal(texture->format, false);
    bool layered = image->slice == -1;
    int slice = layered ? 0 : image->slice;

    lovrRetain(texture);
    lovrRelease(Texture, state.images[slot].texture);
    glBindImageTexture(slot, texture->id, image->mipmap, layered, slice, glAccess, glFormat);
    memcpy(state.images + slot, image, sizeof(Image));
  }
}
#endif

static void lovrGpuBindMesh(Mesh* mesh, Shader* shader, int baseDivisor) {
  lovrGpuBindVertexArray(mesh);

  if (mesh->indexBuffer && mesh->indexCount > 0) {
    lovrGpuBindBuffer(BUFFER_INDEX, mesh->indexBuffer->id);
    lovrBufferUnmap(mesh->indexBuffer);
#ifdef LOVR_GL
    uint32_t primitiveRestart = mesh->indexSize == 4 ? 0xffffffff : 0xffff;
    if (state.primitiveRestart != primitiveRestart) {
      state.primitiveRestart = primitiveRestart;
      glPrimitiveRestartIndex(primitiveRestart);
    }
#endif
  }

  uint16_t enabledLocations = 0;
  for (uint32_t i = 0; i < mesh->attributeCount; i++) {
    MeshAttribute* attribute;
    int location;
    bool integer;

    if ((attribute = &mesh->attributes[i])->disabled) { continue; }
    if ((location = lovrShaderGetAttributeLocation(shader, mesh->attributeNames[i], &integer)) < 0) { continue; }

    lovrBufferUnmap(attribute->buffer);
    enabledLocations |= (1 << location);

    uint16_t divisor = attribute->divisor * baseDivisor;
    if (mesh->divisors[location] != divisor) {
      glVertexAttribDivisor(location, divisor);
      mesh->divisors[location] = divisor;
    }

    if (mesh->locations[location] == i) { continue; }

    mesh->locations[location] = i;
    lovrGpuBindBuffer(BUFFER_VERTEX, attribute->buffer->id);
    GLenum type = convertAttributeType(attribute->type);
    GLvoid* offset = (GLvoid*) (intptr_t) attribute->offset;

    if (integer) {
      glVertexAttribIPointer(location, attribute->components, type, attribute->stride, offset);
    } else {
      glVertexAttribPointer(location, attribute->components, type, attribute->normalized, attribute->stride, offset);
    }
  }

  uint16_t diff = enabledLocations ^ mesh->enabledLocations;
  if (diff != 0) {
    for (uint32_t i = 0; i < MAX_ATTRIBUTES; i++) {
      if (diff & (1 << i)) {
        if (enabledLocations & (1 << i)) {
          glEnableVertexAttribArray(i);
        } else {
          glDisableVertexAttribArray(i);
        }
      }
    }

    mesh->enabledLocations = enabledLocations;
  }
}

static void lovrGpuBindCanvas(Canvas* canvas, bool willDraw) {
  lovrGpuBindFramebuffer(canvas->framebuffer);

  if (canvas->framebuffer == 0) {
    return;
  }

  canvas->needsResolve = willDraw;

  if (!canvas->needsAttach) {
    return;
  }

  // We need to synchronize if any of the Canvas attachments have pending writes on them
#ifndef LOVR_WEBGL
  for (uint32_t i = 0; i < canvas->attachmentCount; i++) {
    Texture* texture = canvas->attachments[i].texture;
    if (texture->incoherent && (texture->incoherent >> BARRIER_CANVAS) & 1) {
      lovrGpuSync(1 << BARRIER_CANVAS);
      break;
    }
  }
#endif

  GLenum buffers[MAX_CANVAS_ATTACHMENTS] = { GL_NONE };
  for (uint32_t i = 0; i < canvas->attachmentCount; i++) {
    GLenum drawBuffer = buffers[i] = GL_COLOR_ATTACHMENT0 + i;
    Attachment* attachment = &canvas->attachments[i];
    Texture* texture = attachment->texture;
    uint32_t slice = attachment->slice;
    uint32_t level = attachment->level;

    if (canvas->flags.stereo && state.singlepass == MULTIVIEW) {
#ifdef LOVR_WEBGL
      lovrThrow("Unreachable");
#else
      glFramebufferTextureMultisampleMultiviewOVR(GL_FRAMEBUFFER, drawBuffer, texture->id, level, canvas->flags.msaa, slice, 2);
#endif
    } else {
      if (canvas->flags.msaa) {
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, drawBuffer, GL_RENDERBUFFER, texture->msaaId);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, canvas->resolveBuffer);
      }

      switch (texture->type) {
        case TEXTURE_2D: glFramebufferTexture2D(GL_READ_FRAMEBUFFER, drawBuffer, GL_TEXTURE_2D, texture->id, level); break;
        case TEXTURE_CUBE: glFramebufferTexture2D(GL_READ_FRAMEBUFFER, drawBuffer, GL_TEXTURE_CUBE_MAP_POSITIVE_X + slice, texture->id, level); break;
        case TEXTURE_ARRAY: glFramebufferTextureLayer(GL_READ_FRAMEBUFFER, drawBuffer, texture->id, level, slice); break;
        case TEXTURE_VOLUME: glFramebufferTextureLayer(GL_READ_FRAMEBUFFER, drawBuffer, texture->id, level, slice); break;
      }
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

  // Color mask
  if (state.colorMask != pipeline->colorMask) {
    state.colorMask = pipeline->colorMask;
    glColorMask(state.colorMask & 0x8, state.colorMask & 0x4, state.colorMask & 0x2, state.colorMask & 0x1);
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

  // Depth test and depth write
  bool updateDepthTest = pipeline->depthTest != state.depthTest;
  bool updateDepthWrite = state.depthWrite != (pipeline->depthWrite && !state.stencilWriting);
  if (updateDepthTest || updateDepthWrite) {
    bool enable = state.depthTest != COMPARE_NONE || state.depthWrite;

    if (enable && !state.depthEnabled) {
      glEnable(GL_DEPTH_TEST);
    } else if (!enable && state.depthEnabled) {
      glDisable(GL_DEPTH_TEST);
    }
    state.depthEnabled = enable;

    if (enable && updateDepthTest) {
      state.depthTest = pipeline->depthTest;
      glDepthFunc(convertCompareMode(state.depthTest));
    }

    if (enable && updateDepthWrite) {
      state.depthWrite = pipeline->depthWrite && !state.stencilWriting;
      glDepthMask(state.depthWrite);
    }
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
#ifdef LOVR_GL
  if (state.wireframe != pipeline->wireframe) {
    state.wireframe = pipeline->wireframe;
    glPolygonMode(GL_FRONT_AND_BACK, state.wireframe ? GL_LINE : GL_FILL);
  }
#endif
}

static void lovrGpuBindShader(Shader* shader) {
  lovrGpuUseProgram(shader->program);

  // Figure out if we need to wait for pending writes on resources to complete
#ifndef LOVR_WEBGL
  uint8_t flags = 0;
  for (size_t i = 0; i < shader->blocks[BLOCK_COMPUTE].length; i++) {
    UniformBlock* block = &shader->blocks[BLOCK_COMPUTE].data[i];
    if (block->source && (block->source->incoherent >> BARRIER_BLOCK) & 1) {
      flags |= 1 << BARRIER_BLOCK;
      break;
    }
  }

  for (size_t i = 0; i < shader->uniforms.length; i++) {
    Uniform* uniform = &shader->uniforms.data[i];
    if (uniform->type == UNIFORM_SAMPLER) {
      for (int j = 0; j < uniform->count; j++) {
        Texture* texture = uniform->value.textures[j];
        if (texture && texture->incoherent && (texture->incoherent >> BARRIER_UNIFORM_TEXTURE) & 1) {
          flags |= 1 << BARRIER_UNIFORM_TEXTURE;
          if (flags & (1 << BARRIER_UNIFORM_IMAGE)) {
            break;
          }
        }
      }
    } else if (uniform->type == UNIFORM_IMAGE) {
      for (int j = 0; j < uniform->count; j++) {
        Texture* texture = uniform->value.images[j].texture;
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
  for (size_t i = 0; i < shader->uniforms.length; i++) {
    Uniform* uniform = &shader->uniforms.data[i];

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
#ifndef LOVR_WEBGL
        for (int j = 0; j < count; j++) {
          Image* image = &uniform->value.images[j];
          Texture* texture = image->texture;
          lovrAssert(!texture || texture->type == uniform->textureType, "Uniform texture type mismatch for uniform '%s'", uniform->name);

          // If the Shader can write to the texture, mark it as incoherent
          if (texture && image->access != ACCESS_READ) {
            for (Barrier barrier = BARRIER_BLOCK + 1; barrier < MAX_BARRIERS; barrier++) {
              texture->incoherent |= 1 << barrier;
              arr_push(&state.incoherents[barrier], texture);
            }
          }

          lovrGpuBindImage(image, uniform->baseSlot + j, uniform->name);
        }
#endif
        break;

      case UNIFORM_SAMPLER:
        for (int j = 0; j < count; j++) {
          Texture* texture = uniform->value.textures[j];
          lovrAssert(!texture || texture->type == uniform->textureType, "Uniform texture type mismatch for uniform '%s'", uniform->name);
          lovrAssert(!texture || (uniform->shadow == (texture->compareMode != COMPARE_NONE)), "Uniform '%s' requires a Texture with%s a compare mode", uniform->name, uniform->shadow ? "" : "out");
          lovrGpuBindTexture(texture, uniform->baseSlot + j);
        }
        break;
    }
  }

  // Bind uniform blocks
  for (BlockType type = BLOCK_UNIFORM; type <= BLOCK_COMPUTE; type++) {
    for (size_t i = 0; i < shader->blocks[type].length; i++) {
      UniformBlock* block = &shader->blocks[type].data[i];
      if (block->source) {
        if (type == BLOCK_COMPUTE && block->access != ACCESS_READ) {
          block->source->incoherent |= (1 << BARRIER_BLOCK);
          arr_push(&state.incoherents[BARRIER_BLOCK], block->source);
        }

        lovrBufferUnmap(block->source);
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
#ifndef LOVR_WEBGL
    if (count > 1) {
      glViewportArrayv(0, count, viewport);
    } else {
#endif
      glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    }
#ifndef LOVR_WEBGL
  }
#endif
}

// GPU

void lovrGpuInit(void* (*getProcAddress)(const char*)) {
#ifdef LOVR_GL
  gladLoadGLLoader((GLADloadproc) getProcAddress);
#elif defined(LOVR_GLES)
  gladLoadGLES2Loader((GLADloadproc) getProcAddress);
#endif

#ifndef LOVR_WEBGL
  state.features.astc = GLAD_GL_ES_VERSION_3_2;
  state.features.compute = GLAD_GL_ES_VERSION_3_1 || GLAD_GL_ARB_compute_shader;
  state.features.dxt = GLAD_GL_EXT_texture_compression_s3tc;
  state.features.instancedStereo = GLAD_GL_ARB_viewport_array && GLAD_GL_AMD_vertex_shader_viewport_index && GLAD_GL_ARB_fragment_layer_viewport;
  state.features.multiview = GLAD_GL_ES_VERSION_3_0 && GLAD_GL_OVR_multiview2 && GLAD_GL_OVR_multiview_multisampled_render_to_texture;
  state.features.timers = GLAD_GL_VERSION_3_3 || GLAD_GL_EXT_disjoint_timer_query;
#ifdef LOVR_GL
  glEnable(GL_LINE_SMOOTH);
  glEnable(GL_PROGRAM_POINT_SIZE);
  glEnable(GL_FRAMEBUFFER_SRGB);
  glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
#endif
  glGetFloatv(GL_POINT_SIZE_RANGE, state.limits.pointSizes);

  if (state.features.multiview) {
    state.singlepass = MULTIVIEW;
  } else if (state.features.instancedStereo) {
    state.singlepass = INSTANCED_STEREO;
  } else {
    state.singlepass = NONE;
  }
#else
  glGetFloatv(GL_ALIASED_POINT_SIZE_RANGE, state.limits.pointSizes);
#endif

  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &state.limits.textureSize);
  glGetIntegerv(GL_MAX_SAMPLES, &state.limits.textureMSAA);
  glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &state.limits.blockSize);
  glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &state.limits.blockAlign);
  glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &state.limits.textureAnisotropy);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

#ifdef LOVR_GLES
  glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
#elif defined(LOVR_GL)
  glEnable(GL_PRIMITIVE_RESTART);
  state.primitiveRestart = 0xffffffff;
  glPrimitiveRestartIndex(state.primitiveRestart);
#endif

  state.activeTexture = 0;
  glActiveTexture(GL_TEXTURE0 + state.activeTexture);

  state.alphaToCoverage = false;
  glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);

  state.blendEnabled = true;
  state.blendMode = BLEND_ALPHA;
  state.blendAlphaMode = BLEND_ALPHA_MULTIPLY;
  glEnable(GL_BLEND);
  glBlendEquation(GL_FUNC_ADD);
  glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

  state.colorMask = 0xf;
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

  state.culling = false;
  glDisable(GL_CULL_FACE);

  state.depthEnabled = true;
  state.depthTest = COMPARE_LEQUAL;
  state.depthWrite = true;
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(convertCompareMode(state.depthTest));
  glDepthMask(state.depthWrite);

  state.lineWidth = 1.f;
  glLineWidth(state.lineWidth);

  state.stencilEnabled = false;
  state.stencilMode = COMPARE_NONE;
  state.stencilValue = 0;
  state.stencilWriting = false;
  glDisable(GL_STENCIL_TEST);

  state.winding = WINDING_COUNTERCLOCKWISE;
  glFrontFace(GL_CCW);

  state.wireframe = false;
#ifdef LOVR_GL
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
#endif

  for (int i = 0; i < MAX_BARRIERS; i++) {
    arr_init(&state.incoherents[i]);
  }

  TextureData* textureData = lovrTextureDataCreate(1, 1, NULL, 0xff, FORMAT_RGBA);
  state.defaultTexture = lovrTextureCreate(TEXTURE_2D, &textureData, 1, true, false, 0);
  lovrTextureSetFilter(state.defaultTexture, (TextureFilter) { .mode = FILTER_NEAREST });
  lovrTextureSetWrap(state.defaultTexture, (TextureWrap) { WRAP_CLAMP, WRAP_CLAMP, WRAP_CLAMP });
  lovrRelease(TextureData, textureData);

  map_init(&state.timerMap, 4);
  state.queryPool.next = ~0u;
  state.activeTimer = ~0u;
}

void lovrGpuDestroy() {
  lovrRelease(Texture, state.defaultTexture);
  for (int i = 0; i < MAX_TEXTURES; i++) {
    lovrRelease(Texture, state.textures[i]);
  }
  for (int i = 0; i < MAX_IMAGES; i++) {
    lovrRelease(Texture, state.images[i].texture);
  }
  for (int i = 0; i < MAX_BARRIERS; i++) {
    arr_free(&state.incoherents[i]);
  }
  glDeleteQueries(state.queryPool.count, state.queryPool.queries);
  free(state.queryPool.queries);
  arr_free(&state.timers);
  map_free(&state.timerMap);
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
#ifdef LOVR_WEBGL
  lovrThrow("Compute shaders are not supported on this system");
#else
  lovrAssert(state.features.compute, "Compute shaders are not supported on this system");
  lovrAssert(shader->type == SHADER_COMPUTE, "Attempt to use a non-compute shader for a compute operation");
  lovrGraphicsFlush();
  lovrGpuBindShader(shader);
  glDispatchCompute(x, y, z);
#endif
}

void lovrGpuDiscard(Canvas* canvas, bool color, bool depth, bool stencil) {
#ifndef LOVR_GL
  lovrGpuBindCanvas(canvas, false);

  GLenum attachments[MAX_CANVAS_ATTACHMENTS + 1] = { 0 };
  int count = 0;

  if (color) {
    int n = MAX(canvas->attachmentCount, 1);
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
  lovrAssert(state.singlepass != MULTIVIEW || draw->shader->multiview == draw->canvas->flags.stereo, "Shader and Canvas multiview settings must match!");
  uint32_t viewportCount = (draw->canvas->flags.stereo && state.singlepass != MULTIVIEW) ? 2 : 1;
  uint32_t drawCount = state.singlepass == NONE ? viewportCount : 1;
  uint32_t instanceMultiplier = state.singlepass == INSTANCED_STEREO ? viewportCount : 1;
  uint32_t viewportsPerDraw = instanceMultiplier;
  uint32_t instances = MAX(draw->instances, 1) * instanceMultiplier;

  float w = state.singlepass == MULTIVIEW ? draw->canvas->width : draw->canvas->width / (float) viewportCount;
  float h = draw->canvas->height;
  float viewports[2][4] = { { 0.f, 0.f, w, h }, { w, 0.f, w, h } };
  lovrShaderSetInts(draw->shader, "lovrViewportCount", &(int) { viewportCount }, 0, 1);

  lovrGpuBindCanvas(draw->canvas, true);
  lovrGpuBindPipeline(&draw->pipeline);
  lovrGpuBindMesh(draw->mesh, draw->shader, instanceMultiplier);

  for (uint32_t i = 0; i < drawCount; i++) {
    lovrGpuSetViewports(&viewports[i][0], viewportsPerDraw);
    lovrShaderSetInts(draw->shader, "lovrViewID", &(int) { i }, 0, 1);
    lovrGpuBindShader(draw->shader);

    Mesh* mesh = draw->mesh;
    GLenum topology = convertTopology(draw->topology);
    if (mesh->indexCount > 0) {
      GLenum indexType = mesh->indexSize == sizeof(uint16_t) ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
      GLvoid* offset = (GLvoid*) (mesh->indexOffset + draw->rangeStart * mesh->indexSize);
      if (instances > 1) {
        glDrawElementsInstanced(topology, draw->rangeCount, indexType, offset, instances);
      } else {
        glDrawElements(topology, draw->rangeCount, indexType, offset);
      }
    } else {
      if (instances > 1) {
        glDrawArraysInstanced(topology, draw->rangeStart, draw->rangeCount, instances);
      } else {
        glDrawArrays(topology, draw->rangeStart, draw->rangeCount);
      }
    }

    state.stats.drawCalls++;
  }
}

void lovrGpuPresent() {
  state.stats.shaderSwitches = 0;
  state.stats.renderPasses = 0;
  state.stats.drawCalls = 0;
}

void lovrGpuStencil(StencilAction action, int replaceValue, StencilCallback callback, void* userdata) {
  lovrGraphicsFlush();
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
    default: lovrThrow("Unreachable");
  }

  glStencilFunc(GL_ALWAYS, replaceValue, 0xff);
  glStencilOp(GL_KEEP, GL_KEEP, glAction);

  state.stencilWriting = true;
  callback(userdata);
  lovrGraphicsFlush();
  state.stencilWriting = false;
  state.stencilMode = ~0; // Dirty
}

void lovrGpuDirtyTexture() {
  lovrRelease(Texture, state.textures[state.activeTexture]);
  state.textures[state.activeTexture] = NULL;
}

// This doesn't actually reset all state, just state that is known to be changed externally
void lovrGpuResetState() {
  if (state.vertexArray) {
    glBindVertexArray(state.vertexArray->vao);
  }

  for (size_t i = 0; i < MAX_BUFFER_TYPES; i++) {
    glBindBuffer(convertBufferType(i), state.buffers[i]);
  }

  glBindFramebuffer(GL_FRAMEBUFFER, state.framebuffer);
  glUseProgram(state.program);

  if (state.blendEnabled) {
    glEnable(GL_BLEND);
  } else {
    glDisable(GL_BLEND);
  }

  if (state.depthEnabled) {
    glEnable(GL_DEPTH_TEST);
  } else {
    glDisable(GL_DEPTH_TEST);
  }
}

void lovrGpuTick(const char* label) {
#ifndef LOVR_WEBGL
  lovrAssert(state.activeTimer == ~0u, "Attempt to start a new GPU timer while one is already active!");
  QueryPool* pool = &state.queryPool;
  uint64_t hash = hash64(label, strlen(label));
  uint64_t index = map_get(&state.timerMap, hash);

  // Create new timer
  if (index == MAP_NIL) {
    index = state.timers.length++;
    map_set(&state.timerMap, hash, index);
    arr_reserve(&state.timers, state.timers.length);
    state.timers.data[index].head = ~0u;
    state.timers.data[index].tail = ~0u;
  }

  Timer* timer = &state.timers.data[index];
  state.activeTimer = index;

  // Expand pool if no unused timers are available.
  // The pool manages one memory allocation split into two chunks.
  // - The first chunk contains OpenGL query objects (GLuint).
  // - The second chunk is a linked list of query indices (uint32_t), used for two purposes:
  //   - For inactive queries, pool->chain[query] points to the next inactive query (freelist).
  //   - For active queries, pool->chain[query] points to the next active query for that timer.
  // When resizing the query pool allocation, the second half of the old allocation needs to be
  // copied to the second half of the new allocation.
  if (pool->next == ~0u) {
    uint32_t n = pool->count;
    pool->count = n == 0 ? 4 : (n << 1);
    pool->queries = realloc(pool->queries, pool->count * (sizeof(GLuint) + sizeof(uint32_t)));
    lovrAssert(pool->queries, "Out of memory");
    pool->chain = pool->queries + pool->count;
    memcpy(pool->chain, pool->queries + n, n * sizeof(uint32_t));
    glGenQueries(n ? n : pool->count, pool->queries + n);
    for (uint32_t i = n; i < pool->count - 1; i++) {
      pool->chain[i] = i + 1;
    }
    pool->chain[pool->count - 1] = ~0u;
    pool->next = n;
  }

  // Start query, update linked list pointers
  uint32_t query = pool->next;
  glBeginQuery(GL_TIME_ELAPSED, pool->queries[query]);
  if (timer->tail != ~0u) { pool->chain[timer->tail] = query; }
  if (timer->head == ~0u) { timer->head = query; }
  pool->next = pool->chain[query];
  pool->chain[query] = ~0u;
  timer->tail = query;
#endif
}

double lovrGpuTock(const char* label) {
#ifndef LOVR_WEBGL
  QueryPool* pool = &state.queryPool;
  uint64_t hash = hash64(label, strlen(label));
  uint64_t index = map_get(&state.timerMap, hash);

  if (index == MAP_NIL) {
    return 0.;
  }

  Timer* timer = &state.timers.data[index];

  if (state.activeTimer != index) {
    return timer->nanoseconds / 1e9;
  }

  glEndQuery(GL_TIME_ELAPSED);
  state.activeTimer = ~0u;

  // Repeatedly check timer's oldest pending query for completion
  for (;;) {
    int query = timer->head;

    GLuint available;
    glGetQueryObjectuiv(pool->queries[query], GL_QUERY_RESULT_AVAILABLE, &available);

    if (!available) {
      break;
    }

    // Update timer result
    glGetQueryObjectui64v(pool->queries[query], GL_QUERY_RESULT, &timer->nanoseconds);

    // Update timer's head pointer and return the completed query back to the pool
    timer->head = pool->chain[query];
    pool->chain[query] = pool->next;
    pool->next = query;

    if (timer->head == ~0u) {
      timer->tail = ~0u;
      break;
    }
  }

  return timer->nanoseconds / 1e9;
#endif
  return 0.;
}

const GpuFeatures* lovrGpuGetFeatures() {
  return &state.features;
}

const GpuLimits* lovrGpuGetLimits() {
  return &state.limits;
}

const GpuStats* lovrGpuGetStats() {
  return &state.stats;
}

// Texture

Texture* lovrTextureCreate(TextureType type, TextureData** slices, uint32_t sliceCount, bool srgb, bool mipmaps, uint32_t msaa) {
  Texture* texture = lovrAlloc(Texture);
  state.stats.textureCount++;
  texture->type = type;
  texture->srgb = srgb;
  texture->mipmaps = mipmaps;
  texture->target = convertTextureTarget(type);
  texture->compareMode = COMPARE_NONE;

  WrapMode wrap = type == TEXTURE_CUBE ? WRAP_CLAMP : WRAP_REPEAT;
  glGenTextures(1, &texture->id);
  lovrGpuBindTexture(texture, 0);
  lovrTextureSetWrap(texture, (TextureWrap) { .s = wrap, .t = wrap, .r = wrap });

  if (msaa > 1) {
    texture->msaa = msaa;
    glGenRenderbuffers(1, &texture->msaaId);
  }

  if (sliceCount > 0) {
    lovrTextureAllocate(texture, slices[0]->width, slices[0]->height, sliceCount, slices[0]->format);
    for (uint32_t i = 0; i < sliceCount; i++) {
      lovrTextureReplacePixels(texture, slices[i], 0, 0, i, 0);
    }
  }

  return texture;
}

Texture* lovrTextureCreateFromHandle(uint32_t handle, TextureType type, uint32_t depth) {
  Texture* texture = lovrAlloc(Texture);
  state.stats.textureCount++;
  texture->type = type;
  texture->id = handle;
  texture->target = convertTextureTarget(type);
  texture->compareMode = COMPARE_NONE;
  texture->native = true;

  int width, height;
  lovrGpuBindTexture(texture, 0);
  glGetTexLevelParameteriv(texture->target, 0, GL_TEXTURE_WIDTH, &width);
  glGetTexLevelParameteriv(texture->target, 0, GL_TEXTURE_HEIGHT, &height);
  texture->width = (uint32_t) width;
  texture->height = (uint32_t) height;
  texture->depth = depth; // There isn't an easy way to get depth/layer count, so it's passed in...
  texture->mipmapCount = 1;

  return texture;
}

void lovrTextureDestroy(void* ref) {
  Texture* texture = ref;
  glDeleteTextures(1, &texture->id);
  glDeleteRenderbuffers(1, &texture->msaaId);
  lovrGpuDestroySyncResource(texture, texture->incoherent);
  state.stats.textureMemory -= getTextureMemorySize(texture);
  state.stats.textureCount--;
}

void lovrTextureAllocate(Texture* texture, uint32_t width, uint32_t height, uint32_t depth, TextureFormat format) {
  uint32_t maxSize = (uint32_t) state.limits.textureSize;
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
    uint32_t dimension = texture->type == TEXTURE_VOLUME ? (MAX(MAX(width, height), depth)) : MAX(width, height);
    texture->mipmapCount = texture->mipmaps ? (log2(dimension) + 1) : 1;
  } else {
    texture->mipmapCount = 1;
  }

  if (isTextureFormatCompressed(format)) {
    return;
  }

  GLenum internalFormat = convertTextureFormatInternal(format, texture->srgb);
#ifdef LOVR_GL
  if (GLAD_GL_ARB_texture_storage) {
#endif
  if (texture->type == TEXTURE_ARRAY || texture->type == TEXTURE_VOLUME) {
    glTexStorage3D(texture->target, texture->mipmapCount, internalFormat, width, height, depth);
  } else {
    glTexStorage2D(texture->target, texture->mipmapCount, internalFormat, width, height);
  }
#ifdef LOVR_GL
  } else {
    GLenum glFormat = convertTextureFormat(format);
    for (uint32_t i = 0; i < texture->mipmapCount; i++) {
      switch (texture->type) {
        case TEXTURE_2D:
          glTexImage2D(texture->target, i, internalFormat, width, height, 0, glFormat, GL_UNSIGNED_BYTE, NULL);
          break;

        case TEXTURE_CUBE:
          for (uint32_t face = 0; face < 6; face++) {
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

  state.stats.textureMemory += getTextureMemorySize(texture);
}

void lovrTextureReplacePixels(Texture* texture, TextureData* textureData, uint32_t x, uint32_t y, uint32_t slice, uint32_t mipmap) {
  lovrGraphicsFlush();
  lovrAssert(texture->allocated, "Texture is not allocated");

#ifndef LOVR_WEBGL
  if ((texture->incoherent >> BARRIER_TEXTURE) & 1) {
    lovrGpuSync(1 << BARRIER_TEXTURE);
  }
#endif

  uint32_t maxWidth = lovrTextureGetWidth(texture, mipmap);
  uint32_t maxHeight = lovrTextureGetHeight(texture, mipmap);
  uint32_t width = textureData->width;
  uint32_t height = textureData->height;
  bool overflow = (x + width > maxWidth) || (y + height > maxHeight);
  lovrAssert(!overflow, "Trying to replace pixels outside the texture's bounds");
  lovrAssert(mipmap < texture->mipmapCount, "Invalid mipmap level %d", mipmap);
  GLenum glFormat = convertTextureFormat(textureData->format);
  GLenum glInternalFormat = convertTextureFormatInternal(textureData->format, texture->srgb);
  GLenum binding = (texture->type == TEXTURE_CUBE) ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + slice : texture->target;

  lovrGpuBindTexture(texture, 0);
  if (isTextureFormatCompressed(textureData->format)) {
    lovrAssert(width == maxWidth && height == maxHeight, "Compressed texture pixels must be fully replaced");
    lovrAssert(mipmap == 0, "Unable to replace a specific mipmap of a compressed texture");
    for (uint32_t i = 0; i < textureData->mipmapCount; i++) {
      Mipmap* m = textureData->mipmaps + i;
      switch (texture->type) {
        case TEXTURE_2D:
        case TEXTURE_CUBE:
          glCompressedTexImage2D(binding, i, glInternalFormat, m->width, m->height, 0, (GLsizei) m->size, m->data);
          break;
        case TEXTURE_ARRAY:
        case TEXTURE_VOLUME:
          glCompressedTexSubImage3D(binding, i, x, y, slice, m->width, m->height, 1, glInternalFormat, (GLsizei) m->size, m->data);
          break;
      }
    }
  } else {
    lovrAssert(textureData->blob->data, "Trying to replace Texture pixels with empty pixel data");
    GLenum glType = convertTextureFormatType(textureData->format);

    switch (texture->type) {
      case TEXTURE_2D:
      case TEXTURE_CUBE:
        glTexSubImage2D(binding, mipmap, x, y, width, height, glFormat, glType, textureData->blob->data);
        break;
      case TEXTURE_ARRAY:
      case TEXTURE_VOLUME:
        glTexSubImage3D(binding, mipmap, x, y, slice, width, height, 1, glFormat, glType, textureData->blob->data);
        break;
    }

    if (texture->mipmaps) {
#if defined(__APPLE__) || defined(LOVR_WEBGL) // glGenerateMipmap doesn't work on big cubemap textures on macOS
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

uint64_t lovrTextureGetId(Texture* texture) {
  return texture->id;
}

uint32_t lovrTextureGetWidth(Texture* texture, uint32_t mipmap) {
  return MAX(texture->width >> mipmap, 1);
}

uint32_t lovrTextureGetHeight(Texture* texture, uint32_t mipmap) {
  return MAX(texture->height >> mipmap, 1);
}

uint32_t lovrTextureGetDepth(Texture* texture, uint32_t mipmap) {
  return texture->type == TEXTURE_VOLUME ? MAX(texture->depth >> mipmap, 1) : texture->depth;
}

uint32_t lovrTextureGetMipmapCount(Texture* texture) {
  return texture->mipmapCount;
}

uint32_t lovrTextureGetMSAA(Texture* texture) {
  return texture->msaa;
}

TextureType lovrTextureGetType(Texture* texture) {
  return texture->type;
}

TextureFormat lovrTextureGetFormat(Texture* texture) {
  return texture->format;
}

CompareMode lovrTextureGetCompareMode(Texture* texture) {
  return texture->compareMode;
}

TextureFilter lovrTextureGetFilter(Texture* texture) {
  return texture->filter;
}

TextureWrap lovrTextureGetWrap(Texture* texture) {
  return texture->wrap;
}

void lovrTextureSetCompareMode(Texture* texture, CompareMode compareMode) {
  if (texture->compareMode != compareMode) {
    lovrGraphicsFlush();
    lovrGpuBindTexture(texture, 0);
    texture->compareMode = compareMode;
    if (compareMode == COMPARE_NONE) {
      glTexParameteri(texture->target, GL_TEXTURE_COMPARE_MODE, GL_NONE);
    } else {
      lovrAssert(isTextureFormatDepth(texture->format), "Only depth textures can set a compare mode");
      glTexParameteri(texture->target, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
      glTexParameteri(texture->target, GL_TEXTURE_COMPARE_FUNC, convertCompareMode(compareMode));
    }
  }
}

void lovrTextureSetFilter(Texture* texture, TextureFilter filter) {
  lovrGraphicsFlush();
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
      if (texture->mipmaps) {
        glTexParameteri(texture->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(texture->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      } else {
        glTexParameteri(texture->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(texture->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      }
      break;
  }

  glTexParameteri(texture->target, GL_TEXTURE_MAX_ANISOTROPY_EXT, MAX(filter.anisotropy, 1.f));
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

Canvas* lovrCanvasCreate(uint32_t width, uint32_t height, CanvasFlags flags) {
  Canvas* canvas = lovrAlloc(Canvas);
  if (flags.stereo && state.singlepass != MULTIVIEW) {
    width *= 2;
  }

  canvas->width = width;
  canvas->height = height;
  canvas->flags = flags;

  glGenFramebuffers(1, &canvas->framebuffer);
  lovrGpuBindFramebuffer(canvas->framebuffer);

  if (flags.depth.enabled) {
    lovrAssert(isTextureFormatDepth(flags.depth.format), "Canvas depth buffer can't use a color TextureFormat");
    GLenum attachment = flags.depth.format == FORMAT_D24S8 ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT;
    if (flags.stereo && state.singlepass == MULTIVIEW) {
      // Zero MSAA is intentional here, we attach it to the Canvas using legacy MSAA technique
      canvas->depth.texture = lovrTextureCreate(TEXTURE_ARRAY, NULL, 0, false, flags.mipmaps, 0);
      lovrTextureAllocate(canvas->depth.texture, width, height, 2, flags.depth.format);
#ifdef LOVR_WEBGL
      lovrThrow("Unreachable");
#else
      glFramebufferTextureMultisampleMultiviewOVR(GL_FRAMEBUFFER, attachment, canvas->depth.texture->id, 0, flags.msaa, 0, 2);
#endif
    } else if (flags.depth.readable) {
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

  if (flags.msaa && (!flags.stereo || state.singlepass != MULTIVIEW)) {
    glGenFramebuffers(1, &canvas->resolveBuffer);
  }

  return canvas;
}

Canvas* lovrCanvasCreateFromHandle(uint32_t width, uint32_t height, CanvasFlags flags, uint32_t framebuffer, uint32_t depthBuffer, uint32_t resolveBuffer, uint32_t attachmentCount, bool immortal) {
  Canvas* canvas = lovrAlloc(Canvas);
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
  for (uint32_t i = 0; i < canvas->attachmentCount; i++) {
    lovrRelease(Texture, canvas->attachments[i].texture);
  }
  lovrRelease(Texture, canvas->depth.texture);
}

void lovrCanvasResolve(Canvas* canvas) {
  if (!canvas->needsResolve) {
    return;
  }

  lovrGraphicsFlushCanvas(canvas);

  // We don't need to resolve a multiview Canvas because it uses the legacy multisampling method in
  // which the driver does an implicit multisample resolve whenever the canvas textures are read.
  if (canvas->flags.msaa && (!canvas->flags.stereo || state.singlepass != MULTIVIEW)) {
    uint32_t w = canvas->width;
    uint32_t h = canvas->height;
    glBindFramebuffer(GL_READ_FRAMEBUFFER, canvas->framebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, canvas->resolveBuffer);
    state.framebuffer = canvas->resolveBuffer;

    if (canvas->attachmentCount == 1) {
      glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    } else {
      GLenum buffers[MAX_CANVAS_ATTACHMENTS] = { GL_NONE };
      for (uint32_t i = 0; i < canvas->attachmentCount; i++) {
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
    for (uint32_t i = 0; i < canvas->attachmentCount; i++) {
      Texture* texture = canvas->attachments[i].texture;
      if (texture->mipmapCount > 1) {
        lovrGpuBindTexture(texture, 0);
        glGenerateMipmap(texture->target);
      }
    }
  }

  canvas->needsResolve = false;
}

TextureData* lovrCanvasNewTextureData(Canvas* canvas, uint32_t index) {
  lovrGraphicsFlushCanvas(canvas);
  lovrGpuBindCanvas(canvas, false);

  if (canvas->flags.msaa) {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, canvas->resolveBuffer);
  }

#ifndef LOVR_WEBGL
  Texture* texture = canvas->attachments[index].texture;
  if ((texture->incoherent >> BARRIER_TEXTURE) & 1) {
    lovrGpuSync(1 << BARRIER_TEXTURE);
  }
#endif

  if (index != 0) {
    glReadBuffer(index);
  }

  TextureData* textureData = lovrTextureDataCreate(canvas->width, canvas->height, NULL, 0x0, FORMAT_RGBA);
  glReadPixels(0, 0, canvas->width, canvas->height, GL_RGBA, GL_UNSIGNED_BYTE, textureData->blob->data);

  if (index != 0) {
    glReadBuffer(0);
  }

  return textureData;
}

const Attachment* lovrCanvasGetAttachments(Canvas* canvas, uint32_t* count) {
  if (count) *count = canvas->attachmentCount;
  return canvas->attachments;
}

void lovrCanvasSetAttachments(Canvas* canvas, Attachment* attachments, uint32_t count) {
  lovrAssert(count > 0, "A Canvas must have at least one attached Texture");
  lovrAssert(count <= MAX_CANVAS_ATTACHMENTS, "Only %d textures can be attached to a Canvas, got %d\n", MAX_CANVAS_ATTACHMENTS, count);

  if (!canvas->needsAttach && count == canvas->attachmentCount && !memcmp(canvas->attachments, attachments, count * sizeof(Attachment))) {
    return;
  }

  lovrGraphicsFlushCanvas(canvas);

  for (uint32_t i = 0; i < count; i++) {
    Texture* texture = attachments[i].texture;
    uint32_t slice = attachments[i].slice;
    uint32_t level = attachments[i].level;
    uint32_t width = lovrTextureGetWidth(texture, level);
    uint32_t height = lovrTextureGetHeight(texture, level);
    uint32_t depth = lovrTextureGetDepth(texture, level);
    uint32_t mipmaps = lovrTextureGetMipmapCount(texture);
    bool hasDepthBuffer = canvas->flags.depth.enabled;
    lovrAssert(slice < depth, "Invalid attachment slice (Texture has %d, got %d)", depth, slice + 1);
    lovrAssert(level < mipmaps, "Invalid attachment mipmap level (Texture has %d, got %d)", mipmaps, level + 1);
    lovrAssert(!hasDepthBuffer || width == canvas->width, "Texture width of %d does not match Canvas width (%d)", width, canvas->width);
    lovrAssert(!hasDepthBuffer || height == canvas->height, "Texture height of %d does not match Canvas height (%d)", height, canvas->height);
#ifndef __ANDROID__ // On multiview canvases, the multisample settings can be different
    lovrAssert(lovrTextureGetMSAA(texture) == canvas->flags.msaa, "Texture MSAA does not match Canvas MSAA");
#endif
    lovrRetain(texture);
  }

  for (uint32_t i = 0; i < canvas->attachmentCount; i++) {
    lovrRelease(Texture, canvas->attachments[i].texture);
  }

  memcpy(canvas->attachments, attachments, count * sizeof(Attachment));
  canvas->attachmentCount = count;
  canvas->needsAttach = true;
}

bool lovrCanvasIsStereo(Canvas* canvas) {
  return canvas->flags.stereo;
}

void lovrCanvasSetStereo(Canvas* canvas, bool stereo) {
  canvas->flags.stereo = stereo;
}

uint32_t lovrCanvasGetWidth(Canvas* canvas) {
  return canvas->width;
}

uint32_t lovrCanvasGetHeight(Canvas* canvas) {
  return canvas->height;
}

void lovrCanvasSetWidth(Canvas* canvas, uint32_t width) {
  canvas->width = width;
}

void lovrCanvasSetHeight(Canvas* canvas, uint32_t height) {
  canvas->height = height;
}

uint32_t lovrCanvasGetMSAA(Canvas* canvas) {
  return canvas->flags.msaa;
}

Texture* lovrCanvasGetDepthTexture(Canvas* canvas) {
  return canvas->depth.texture;
}

// Buffer

Buffer* lovrBufferCreate(size_t size, void* data, BufferType type, BufferUsage usage, bool readable) {
  Buffer* buffer = lovrAlloc(Buffer);
  state.stats.bufferCount++;
  state.stats.bufferMemory += size;
  buffer->size = size;
  buffer->readable = readable;
  buffer->type = type;
  buffer->usage = usage;
  glGenBuffers(1, &buffer->id);
  lovrGpuBindBuffer(type, buffer->id);
  GLenum glType = convertBufferType(type);

#ifdef LOVR_WEBGL
  buffer->data = malloc(size);
  lovrAssert(buffer->data, "Out of memory");
  glBufferData(glType, size, data, convertBufferUsage(usage));

  if (data) {
    memcpy(buffer->data, data, size);
  }
#else
  if (GLAD_GL_ARB_buffer_storage) {
    GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | (readable ? GL_MAP_READ_BIT : 0);
    glBufferStorage(glType, size, data, flags);
    buffer->data = glMapBufferRange(glType, 0, size, flags | GL_MAP_FLUSH_EXPLICIT_BIT);
  } else {
    glBufferData(glType, size, data, convertBufferUsage(usage));
  }
#endif

  return buffer;
}

void lovrBufferDestroy(void* ref) {
  Buffer* buffer = ref;
  lovrGpuDestroySyncResource(buffer, buffer->incoherent);
  glDeleteBuffers(1, &buffer->id);
#ifdef LOVR_WEBGL
  free(buffer->data);
#endif
  state.stats.bufferMemory -= buffer->size;
  state.stats.bufferCount--;
}

size_t lovrBufferGetSize(Buffer* buffer) {
  return buffer->size;
}

bool lovrBufferIsReadable(Buffer* buffer) {
  return buffer->readable;
}

BufferUsage lovrBufferGetUsage(Buffer* buffer) {
  return buffer->usage;
}

void* lovrBufferMap(Buffer* buffer, size_t offset) {
#ifndef LOVR_WEBGL
  if (!GLAD_GL_ARB_buffer_storage && !buffer->mapped) {
    buffer->mapped = true;
    lovrGpuBindBuffer(buffer->type, buffer->id);
    GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT;
    flags |= buffer->readable ? GL_MAP_READ_BIT : GL_MAP_UNSYNCHRONIZED_BIT;
    buffer->data = glMapBufferRange(convertBufferType(buffer->type), 0, buffer->size, flags);
  }
#endif
  return (uint8_t*) buffer->data + offset;
}

void lovrBufferFlush(Buffer* buffer, size_t offset, size_t size) {
  buffer->flushFrom = MIN(buffer->flushFrom, offset);
  buffer->flushTo = MAX(buffer->flushTo, offset + size);
}

void lovrBufferUnmap(Buffer* buffer) {
#ifdef LOVR_WEBGL
  if (buffer->flushTo > buffer->flushFrom) {
    lovrGpuBindBuffer(buffer->type, buffer->id);
    void* data = (uint8_t*) buffer->data + buffer->flushFrom;
    glBufferSubData(convertBufferType(buffer->type), buffer->flushFrom, buffer->flushTo - buffer->flushFrom, data);
  }
#else
  if (buffer->mapped || GLAD_GL_ARB_buffer_storage) {
    lovrGpuBindBuffer(buffer->type, buffer->id);

    if (buffer->flushTo > buffer->flushFrom) {
      glFlushMappedBufferRange(convertBufferType(buffer->type), buffer->flushFrom, buffer->flushTo - buffer->flushFrom);
    }

    if (buffer->mapped) {
      glUnmapBuffer(convertBufferType(buffer->type));
      buffer->mapped = false;
    }
  }
#endif
  buffer->flushFrom = SIZE_MAX;
  buffer->flushTo = 0;
}

void lovrBufferDiscard(Buffer* buffer) {
  lovrGpuBindBuffer(buffer->type, buffer->id);
  GLenum glType = convertBufferType(buffer->type);
#ifdef LOVR_WEBGL
  glBufferData(glType, buffer->size, NULL, convertBufferUsage(buffer->usage));
#else
  // We unmap even if persistent mapping is supported
  if (buffer->mapped || GLAD_GL_ARB_buffer_storage) {
    glUnmapBuffer(glType);
    buffer->mapped = false;
  }

  GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT;
  flags |= buffer->readable ? GL_MAP_READ_BIT : (GL_MAP_UNSYNCHRONIZED_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
  flags |= GLAD_GL_ARB_buffer_storage ? GL_MAP_PERSISTENT_BIT : 0;
  buffer->data = glMapBufferRange(glType, 0, buffer->size, flags);

  if (!GLAD_GL_ARB_buffer_storage) {
    buffer->mapped = true;
  }
#endif
}

// Shader

static GLuint compileShader(GLenum type, const char** sources, int* lengths, int count) {
  GLuint shader = glCreateShader(type);
  glShaderSource(shader, count, sources, lengths);
  glCompileShader(shader);

  int isShaderCompiled;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &isShaderCompiled);
  if (!isShaderCompiled) {
    int logLength;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
    char* log = malloc(logLength);
    lovrAssert(log, "Out of memory");
    glGetShaderInfoLog(shader, logLength, &logLength, log);
    const char* name;
    switch (type) {
      case GL_VERTEX_SHADER: name = "vertex shader"; break;
      case GL_FRAGMENT_SHADER: name = "fragment shader"; break;
      case GL_COMPUTE_SHADER: name = "compute shader"; break;
      default: name = "shader"; break;
    }
    lovrThrow("Could not compile %s:\n%s", name, log);
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
    lovrAssert(log, "Out of memory");
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
  lovrAssert((size_t) blockCount <= MAX_BLOCK_BUFFERS, "Shader has too many uniform blocks (%d) the max is %d", blockCount, MAX_BLOCK_BUFFERS);
  map_init(&shader->blockMap, blockCount);
  arr_block_t* uniformBlocks = &shader->blocks[BLOCK_UNIFORM];
  arr_init(uniformBlocks);
  arr_reserve(uniformBlocks, (size_t) blockCount);
  for (int i = 0; i < blockCount; i++) {
    UniformBlock block = { .slot = i, .source = NULL };
    glUniformBlockBinding(program, i, block.slot);

    GLsizei length;
    char name[LOVR_MAX_UNIFORM_LENGTH];
    glGetActiveUniformBlockName(program, i, LOVR_MAX_UNIFORM_LENGTH, &length, name);
    int blockId = (i << 1) + BLOCK_UNIFORM;
    map_set(&shader->blockMap, hash64(name, length), blockId);
    arr_push(uniformBlocks, block);
    arr_init(&uniformBlocks->data[uniformBlocks->length - 1].uniforms);
  }

  // Shader storage buffers and their buffer variables
  arr_block_t* computeBlocks = &shader->blocks[BLOCK_COMPUTE];
  arr_init(computeBlocks);
#ifndef LOVR_WEBGL
  if ((GLAD_GL_ARB_shader_storage_buffer_object && GLAD_GL_ARB_program_interface_query) || GLAD_GL_ES_VERSION_3_1) {

    // Iterate over compute blocks, setting their binding and pushing them onto the block vector
    int32_t computeBlockCount;
    glGetProgramInterfaceiv(program, GL_SHADER_STORAGE_BLOCK, GL_ACTIVE_RESOURCES, &computeBlockCount);
    lovrAssert(computeBlockCount <= MAX_BLOCK_BUFFERS, "Shader has too many compute blocks (%d) the max is %d", computeBlockCount, MAX_BLOCK_BUFFERS);
    arr_reserve(computeBlocks, (size_t) computeBlockCount);
    for (int i = 0; i < computeBlockCount; i++) {
      UniformBlock block = { .slot = i, .source = NULL };
#ifdef LOVR_GLES // GLES can only set the block binding in shader code, so for now we only support one 0-bound block
      block.slot = 0;
#else
      glShaderStorageBlockBinding(program, i, block.slot);
#endif
      arr_init(&block.uniforms);

      GLsizei length;
      char name[LOVR_MAX_UNIFORM_LENGTH];
      glGetProgramResourceName(program, GL_SHADER_STORAGE_BLOCK, i, LOVR_MAX_UNIFORM_LENGTH, &length, name);
      int blockId = (i << 1) + BLOCK_COMPUTE;
      map_set(&shader->blockMap, hash64(name, length), blockId);
      arr_push(computeBlocks, block);
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
      arr_push(&computeBlocks->data[values[blockIndex]].uniforms, uniform);
    }
  }
#endif

  // Uniform introspection
  int32_t uniformCount;
  int textureSlot = 0;
  int imageSlot = 0;
  glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &uniformCount);
  map_init(&shader->uniformMap, 0);
  arr_init(&shader->uniforms);
  for (uint32_t i = 0; i < (uint32_t) uniformCount; i++) {
    Uniform uniform;
    GLenum glType;
    GLsizei length;
    glGetActiveUniform(program, i, LOVR_MAX_UNIFORM_LENGTH, &length, &uniform.count, &glType, uniform.name);

    char* subscript = strchr(uniform.name, '[');
    if (subscript) {
      if (subscript[1] > '0') {
        continue;
      } else {
        *subscript = '\0';
        length = subscript - uniform.name;
      }
    }

    uniform.location = glGetUniformLocation(program, uniform.name);
    uniform.type = getUniformType(glType, uniform.name);
    uniform.components = getUniformComponents(glType);
    uniform.shadow = glType == GL_SAMPLER_2D_SHADOW;
#ifdef LOVR_WEBGL
    uniform.image = false;
#else
    uniform.image = glType == GL_IMAGE_2D || glType == GL_IMAGE_3D || glType == GL_IMAGE_CUBE || glType == GL_IMAGE_2D_ARRAY;
#endif
    uniform.textureType = getUniformTextureType(glType);
    uniform.baseSlot = uniform.type == UNIFORM_SAMPLER ? textureSlot : (uniform.type == UNIFORM_IMAGE ? imageSlot : -1);
    uniform.dirty = false;

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

      arr_push(&block->uniforms, uniform);
      continue;
    } else if (uniform.location == -1) {
      continue;
    }

    switch (uniform.type) {
      case UNIFORM_FLOAT:
        uniform.size = uniform.components * uniform.count * sizeof(float);
        uniform.value.data = calloc(1, uniform.size);
        lovrAssert(uniform.value.data, "Out of memory");
        break;

      case UNIFORM_INT:
        uniform.size = uniform.components * uniform.count * sizeof(int);
        uniform.value.data = calloc(1, uniform.size);
        lovrAssert(uniform.value.data, "Out of memory");
        break;

      case UNIFORM_MATRIX:
        uniform.size = uniform.components * uniform.components * uniform.count * sizeof(float);
        uniform.value.data = calloc(1, uniform.size);
        lovrAssert(uniform.value.data, "Out of memory");
        break;

      case UNIFORM_SAMPLER:
      case UNIFORM_IMAGE:
        uniform.size = uniform.count * (uniform.type == UNIFORM_SAMPLER ? sizeof(Texture*) : sizeof(Image));
        uniform.value.data = calloc(1, uniform.size);
        lovrAssert(uniform.value.data, "Out of memory");

        // Use the value for ints to bind texture slots, but use the value for textures afterwards.
        for (int j = 0; j < uniform.count; j++) {
          uniform.value.ints[j] = uniform.baseSlot + j;
        }
        glUniform1iv(uniform.location, uniform.count, uniform.value.ints);
        memset(uniform.value.data, 0, uniform.size);
        break;
    }

    size_t offset = 0;
    for (int j = 0; j < uniform.count; j++) {
      int location = uniform.location;

      if (uniform.count > 1) {
        char name[76 /* LOVR_MAX_UNIFORM_LENGTH + 2 + 10 */];
        snprintf(name, sizeof(name), "%s[%d]", uniform.name, j);
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

    map_set(&shader->uniformMap, hash64(uniform.name, length), shader->uniforms.length);
    arr_push(&shader->uniforms, uniform);
    textureSlot += uniform.type == UNIFORM_SAMPLER ? uniform.count : 0;
    imageSlot += uniform.type == UNIFORM_IMAGE ? uniform.count : 0;
  }
}

static char* lovrShaderGetFlagCode(ShaderFlag* flags, uint32_t flagCount) {
  if (flagCount == 0) {
    return NULL;
  }

  // Figure out how much space we need
  size_t length = 0;
  for (uint32_t i = 0; i < flagCount; i++) {
    if (flags[i].name && !(flags[i].type == FLAG_BOOL && flags[i].value.b32 == false)) {
      length += strlen("#define FLAG_");
      length += strlen(flags[i].name);
      if (flags[i].type == FLAG_INT) {
        length += snprintf(NULL, 0, " %d", flags[i].value.i32);
      }
      length += strlen("\n");
    }
  }

  // Generate the string
  char* code = malloc(length + 1);
  code[length] = '\0';
  char* s = code;
  for (uint32_t i = 0; i < flagCount; i++) {
    if (flags[i].name && !(flags[i].type == FLAG_BOOL && flags[i].value.b32 == false)) {
      s += sprintf(s, "#define FLAG_%s", flags[i].name);
      if (flags[i].type == FLAG_INT) {
        s += sprintf(s, " %d", flags[i].value.i32);
      }
      *s++ = '\n';
    }
  }

  return code;
}

Shader* lovrShaderCreateGraphics(const char* vertexSource, int vertexSourceLength, const char* fragmentSource, int fragmentSourceLength, ShaderFlag* flags, uint32_t flagCount, bool multiview) {
  Shader* shader = lovrAlloc(Shader);
#if defined(LOVR_WEBGL) || defined(LOVR_GLES)
  const char* version = "#version 300 es\n";
#else
  const char* version = state.features.compute ? "#version 430\n" : "#version 150\n";
#endif

  const char* singlepass[2] = { "", "" };
  if (multiview && state.singlepass == MULTIVIEW) {
    singlepass[0] = singlepass[1] = "#extension GL_OVR_multiview2 : require\n#define MULTIVIEW\n";
  } else if (state.singlepass == INSTANCED_STEREO) {
    singlepass[0] = "#extension GL_AMD_vertex_shader_viewport_index : require\n""#define INSTANCED_STEREO\n";
    singlepass[1] = "#extension GL_ARB_fragment_layer_viewport : require\n""#define INSTANCED_STEREO\n";
  }

  char* flagSource = lovrShaderGetFlagCode(flags, flagCount);

  // Vertex
  vertexSource = vertexSource == NULL ? lovrUnlitVertexShader : vertexSource;
  const char* vertexSources[] = { version, singlepass[0], flagSource ? flagSource : "", lovrShaderVertexPrefix, vertexSource, lovrShaderVertexSuffix };
  int vertexSourceLengths[] = { -1, -1, -1, -1, vertexSourceLength, -1 };
  size_t vertexSourceCount = sizeof(vertexSources) / sizeof(vertexSources[0]);
  GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSources, vertexSourceLengths, vertexSourceCount);

  // Fragment
  fragmentSource = fragmentSource == NULL ? lovrUnlitFragmentShader : fragmentSource;
  const char* fragmentSources[] = { version, singlepass[1], flagSource ? flagSource : "", lovrShaderFragmentPrefix, fragmentSource, lovrShaderFragmentSuffix };
  int fragmentSourceLengths[] = { -1, -1, -1, -1, fragmentSourceLength, -1 };
  size_t fragmentSourceCount = sizeof(fragmentSources) / sizeof(fragmentSources[0]);
  GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSources, fragmentSourceLengths, fragmentSourceCount);

  free(flagSource);

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
  glVertexAttribI4uiv(LOVR_SHADER_BONES, (uint32_t[4]) { 0., 0., 0., 0. });
  glVertexAttrib4fv(LOVR_SHADER_BONE_WEIGHTS, (float[4]) { 1., 0., 0., 0. });
  glVertexAttribI4ui(LOVR_SHADER_DRAW_ID, 0, 0, 0, 0);

  lovrShaderSetupUniforms(shader);

  // Attribute cache
  int32_t attributeCount;
  glGetProgramiv(program, GL_ACTIVE_ATTRIBUTES, &attributeCount);
  map_init(&shader->attributes, attributeCount);
  for (int i = 0; i < attributeCount; i++) {
    char name[LOVR_MAX_ATTRIBUTE_LENGTH];
    GLint size;
    GLenum type;
    GLsizei length;
    glGetActiveAttrib(program, i, LOVR_MAX_ATTRIBUTE_LENGTH, &length, &size, &type, name);
    map_set(&shader->attributes, hash64(name, length), (glGetAttribLocation(program, name) << 1) | isAttributeTypeInteger(type));
  }

  shader->multiview = multiview;

  return shader;
}

Shader* lovrShaderCreateDefault(DefaultShader type, ShaderFlag* flags, uint32_t flagCount, bool multiview) {
  switch (type) {
    case SHADER_UNLIT: return lovrShaderCreateGraphics(NULL, -1, NULL, -1, flags, flagCount, multiview);
    case SHADER_STANDARD: return lovrShaderCreateGraphics(lovrStandardVertexShader, -1, lovrStandardFragmentShader, -1, flags, flagCount, multiview);
    case SHADER_CUBE: return lovrShaderCreateGraphics(lovrCubeVertexShader, -1, lovrCubeFragmentShader, -1, flags, flagCount, multiview);
    case SHADER_PANO: return lovrShaderCreateGraphics(lovrCubeVertexShader, -1, lovrPanoFragmentShader, -1, flags, flagCount, multiview);
    case SHADER_FONT: return lovrShaderCreateGraphics(NULL, -1, lovrFontFragmentShader, -1, flags, flagCount, multiview);
    case SHADER_FILL: return lovrShaderCreateGraphics(lovrFillVertexShader, -1, NULL, -1, flags, flagCount, multiview);
    default: lovrThrow("Unknown default shader type"); return NULL;
  }
}

Shader* lovrShaderCreateCompute(const char* source, int length, ShaderFlag* flags, uint32_t flagCount) {
  Shader* shader = lovrAlloc(Shader);
#ifdef LOVR_WEBGL
  lovrThrow("Compute shaders are not supported on this system");
#else
  lovrAssert(state.features.compute, "Compute shaders are not supported on this system");
  char* flagSource = lovrShaderGetFlagCode(flags, flagCount);
  const char* sources[] = { lovrShaderComputePrefix, flagSource ? flagSource : "", source, lovrShaderComputeSuffix };
  int lengths[] = { -1, -1, length, -1 };
  size_t count = sizeof(sources) / sizeof(sources[0]);
  GLuint computeShader = compileShader(GL_COMPUTE_SHADER, sources, lengths, count);
  free(flagSource);
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
  for (size_t i = 0; i < shader->uniforms.length; i++) {
    free(shader->uniforms.data[i].value.data);
  }
  for (BlockType type = BLOCK_UNIFORM; type <= BLOCK_COMPUTE; type++) {
    for (size_t i = 0; i < shader->blocks[type].length; i++) {
      lovrRelease(Buffer, shader->blocks[type].data[i].source);
      arr_free(&shader->blocks[type].data[i].uniforms);
    }
  }
  arr_free(&shader->uniforms);
  arr_free(&shader->blocks[BLOCK_UNIFORM]);
  arr_free(&shader->blocks[BLOCK_COMPUTE]);
  map_free(&shader->attributes);
  map_free(&shader->uniformMap);
  map_free(&shader->blockMap);
}

ShaderType lovrShaderGetType(Shader* shader) {
  return shader->type;
}

int lovrShaderGetAttributeLocation(Shader* shader, const char* name, bool* integer) {
  uint64_t info = map_get(&shader->attributes, hash64(name, strlen(name)));
  *integer = info & 1;
  return info == MAP_NIL ? -1 : (int) (info >> 1);
}

bool lovrShaderHasUniform(Shader* shader, const char* name) {
  return map_get(&shader->uniformMap, hash64(name, strlen(name))) != MAP_NIL;
}

bool lovrShaderHasBlock(Shader* shader, const char* name) {
  return map_get(&shader->blockMap, hash64(name, strlen(name))) != MAP_NIL;
}

const Uniform* lovrShaderGetUniform(Shader* shader, const char* name) {
  uint64_t index = map_get(&shader->uniformMap, hash64(name, strlen(name)));
  return index == MAP_NIL ? NULL : &shader->uniforms.data[index];
}

static void lovrShaderSetUniform(Shader* shader, const char* name, UniformType type, void* data, int start, int count, int size, const char* debug) {
  uint64_t index = map_get(&shader->uniformMap, hash64(name, strlen(name)));
  if (index == MAP_NIL) {
    return;
  }

  Uniform* uniform = &shader->uniforms.data[index];
  lovrAssert(uniform->type == type, "Unable to send %ss to uniform %s", debug, name);
  lovrAssert((start + count) * size <= uniform->size, "Too many %ss for uniform %s, maximum is %d", debug, name, uniform->size / size);

  void* dest = uniform->value.bytes + start * size;
  if (memcmp(dest, data, count * size)) {
    lovrGraphicsFlushShader(shader);
    memcpy(dest, data, count * size);
    uniform->dirty = true;
  }
}

void lovrShaderSetFloats(Shader* shader, const char* name, float* data, int start, int count) {
  lovrShaderSetUniform(shader, name, UNIFORM_FLOAT, data, start, count, sizeof(float), "float");
}

void lovrShaderSetInts(Shader* shader, const char* name, int* data, int start, int count) {
  lovrShaderSetUniform(shader, name, UNIFORM_INT, data, start, count, sizeof(int), "int");
}

void lovrShaderSetMatrices(Shader* shader, const char* name, float* data, int start, int count) {
  lovrShaderSetUniform(shader, name, UNIFORM_MATRIX, data, start, count, sizeof(float), "float");
}

void lovrShaderSetTextures(Shader* shader, const char* name, Texture** data, int start, int count) {
  lovrShaderSetUniform(shader, name, UNIFORM_SAMPLER, data, start, count, sizeof(Texture*), "texture");
}

void lovrShaderSetImages(Shader* shader, const char* name, Image* data, int start, int count) {
  lovrShaderSetUniform(shader, name, UNIFORM_IMAGE, data, start, count, sizeof(Image), "image");
}

void lovrShaderSetColor(Shader* shader, const char* name, Color color) {
  color.r = lovrMathGammaToLinear(color.r);
  color.g = lovrMathGammaToLinear(color.g);
  color.b = lovrMathGammaToLinear(color.b);
  lovrShaderSetUniform(shader, name, UNIFORM_FLOAT, (float*) &color, 0, 4, sizeof(float), "float");
}

void lovrShaderSetBlock(Shader* shader, const char* name, Buffer* buffer, size_t offset, size_t size, UniformAccess access) {
  uint64_t id = map_get(&shader->blockMap, hash64(name, strlen(name)));
  if (id == MAP_NIL) return;

  int type = id & 1;
  int index = id >> 1;
  UniformBlock* block = &shader->blocks[type].data[index];

  if (block->source != buffer || block->offset != offset || block->size != size) {
    lovrGraphicsFlushShader(shader);
    lovrRetain(buffer);
    lovrRelease(Buffer, block->source);
    block->access = access;
    block->source = buffer;
    block->offset = offset;
    block->size = size;
  }
}

// ShaderBlock

// Calculates uniform size and byte offsets using std140 rules, returning the total buffer size
size_t lovrShaderComputeUniformLayout(arr_uniform_t* uniforms) {
  size_t size = 0;
  for (size_t i = 0; i < uniforms->length; i++) {
    int align;
    Uniform* uniform = &uniforms->data[i];
    if (uniform->count > 1 || uniform->type == UNIFORM_MATRIX) {
      align = 16;
      uniform->size = align * uniform->count * (uniform->type == UNIFORM_MATRIX ? uniform->components : 1);
    } else {
      align = (uniform->components + (uniform->components == 3)) * 4;
      uniform->size = uniform->components * 4;
    }
    uniform->offset = (size + (align - 1)) & -align;
    size = uniform->offset + uniform->size;
  }
  return size;
}

ShaderBlock* lovrShaderBlockCreate(BlockType type, Buffer* buffer, arr_uniform_t* uniforms) {
  ShaderBlock* block = lovrAlloc(ShaderBlock);
  arr_init(&block->uniforms);
  map_init(&block->uniformMap, uniforms->length);

  arr_append(&block->uniforms, uniforms->data, uniforms->length);

  for (size_t i = 0; i < block->uniforms.length; i++) {
    Uniform* uniform = &block->uniforms.data[i];
    map_set(&block->uniformMap, hash64(uniform->name, strlen(uniform->name)), i);
  }

  block->type = type;
  block->buffer = buffer;
  lovrRetain(buffer);
  return block;
}

void lovrShaderBlockDestroy(void* ref) {
  ShaderBlock* block = ref;
  lovrRelease(Buffer, block->buffer);
  arr_free(&block->uniforms);
  map_free(&block->uniformMap);
}

BlockType lovrShaderBlockGetType(ShaderBlock* block) {
  return block->type;
}

char* lovrShaderBlockGetShaderCode(ShaderBlock* block, const char* blockName, const char* namespace, size_t* length) {

  // Calculate
  size_t size = 0;
  size_t tab = 2;
  size += 15; // "layout(std140) "
  size += block->type == BLOCK_UNIFORM ? 7 : 6; // "uniform" || "buffer"
  size += 1; // " "
  size += strlen(blockName);
  size += 3; // " {\n"
  for (size_t i = 0; i < block->uniforms.length; i++) {
    size += tab;
    size += getUniformTypeLength(&block->uniforms.data[i]);
    size += 1; // " "
    size += strlen(block->uniforms.data[i].name);
    size += 2; // ";\n"
  }
  if (namespace) {
    size += 2; // "} "
    size += strlen(namespace);
    size += 2; // ";\n"
  } else {
    size += 3; // "};\n"
  }

  // Allocate
  char* code = malloc(size + 1);
  lovrAssert(code, "Out of memory");

  // Concatenate
  char* s = code;
  s += sprintf(s, "layout(std140) %s %s {\n", block->type == BLOCK_UNIFORM ? "uniform" : "buffer", blockName);
  for (size_t i = 0; i < block->uniforms.length; i++) {
    const Uniform* uniform = &block->uniforms.data[i];
    if (uniform->count > 1) {
      s += sprintf(s, "  %s %s[%d];\n", getUniformTypeName(uniform), uniform->name, uniform->count);
    } else {
      s += sprintf(s, "  %s %s;\n", getUniformTypeName(uniform), uniform->name);
    }
  }
  if (namespace) {
    s += sprintf(s, "} %s;\n", namespace);
  } else {
    s += sprintf(s, "};\n");
  }
  *s = '\0';

  *length = size;
  return code;
}

const Uniform* lovrShaderBlockGetUniform(ShaderBlock* block, const char* name) {
  uint64_t index = map_get(&block->uniformMap, hash64(name, strlen(name)));
  return index == MAP_NIL ? NULL : &block->uniforms.data[index];
}

Buffer* lovrShaderBlockGetBuffer(ShaderBlock* block) {
  return block->buffer;
}

// Mesh

Mesh* lovrMeshCreate(DrawMode mode, Buffer* vertexBuffer, uint32_t vertexCount) {
  Mesh* mesh = lovrAlloc(Mesh);
  mesh->mode = mode;
  mesh->vertexBuffer = vertexBuffer;
  mesh->vertexCount = vertexCount;
  lovrRetain(mesh->vertexBuffer);
  glGenVertexArrays(1, &mesh->vao);
  map_init(&mesh->attributeMap, MAX_ATTRIBUTES);
  memset(mesh->locations, 0xff, MAX_ATTRIBUTES * sizeof(uint8_t));
  return mesh;
}

void lovrMeshDestroy(void* ref) {
  Mesh* mesh = ref;
  lovrGraphicsFlushMesh(mesh);
  glDeleteVertexArrays(1, &mesh->vao);
  for (uint32_t i = 0; i < mesh->attributeCount; i++) {
    lovrRelease(Buffer, mesh->attributes[i].buffer);
  }
  map_free(&mesh->attributeMap);
  lovrRelease(Buffer, mesh->vertexBuffer);
  lovrRelease(Buffer, mesh->indexBuffer);
  lovrRelease(Material, mesh->material);
}

void lovrMeshSetIndexBuffer(Mesh* mesh, Buffer* buffer, uint32_t indexCount, size_t indexSize, size_t offset) {
  if (mesh->indexBuffer != buffer || mesh->indexCount != indexCount || mesh->indexSize != indexSize) {
    lovrGraphicsFlushMesh(mesh);
    lovrRetain(buffer);
    lovrRelease(Buffer, mesh->indexBuffer);
    mesh->indexBuffer = buffer;
    mesh->indexCount = indexCount;
    mesh->indexSize = indexSize;
    mesh->indexOffset = offset;
  }
}

Buffer* lovrMeshGetVertexBuffer(Mesh* mesh) {
  return mesh->vertexBuffer;
}

Buffer* lovrMeshGetIndexBuffer(Mesh* mesh) {
  return mesh->indexBuffer;
}

uint32_t lovrMeshGetVertexCount(Mesh* mesh) {
  return mesh->vertexCount;
}

uint32_t lovrMeshGetIndexCount(Mesh* mesh) {
  return mesh->indexCount;
}

size_t lovrMeshGetIndexSize(Mesh* mesh) {
  return mesh->indexSize;
}

uint32_t lovrMeshGetAttributeCount(Mesh* mesh) {
  return mesh->attributeCount;
}

void lovrMeshAttachAttribute(Mesh* mesh, const char* name, MeshAttribute* attribute) {
  uint64_t hash = hash64(name, strlen(name));
  lovrAssert(map_get(&mesh->attributeMap, hash) == MAP_NIL, "Mesh already has an attribute named '%s'", name);
  lovrAssert(mesh->attributeCount < MAX_ATTRIBUTES, "Mesh already has the max number of attributes (%d)", MAX_ATTRIBUTES);
  lovrAssert(strlen(name) < MAX_ATTRIBUTE_NAME_LENGTH, "Mesh attribute name '%s' is too long (max is %d)", name, MAX_ATTRIBUTE_NAME_LENGTH);
  lovrGraphicsFlushMesh(mesh);
  uint64_t index = mesh->attributeCount++;
  mesh->attributes[index] = *attribute;
  strcpy(mesh->attributeNames[index], name);
  map_set(&mesh->attributeMap, hash, index);
  lovrRetain(attribute->buffer);
}

void lovrMeshDetachAttribute(Mesh* mesh, const char* name) {
  uint64_t hash = hash64(name, strlen(name));
  uint64_t index = map_get(&mesh->attributeMap, hash);
  lovrAssert(index != MAP_NIL, "No attached attribute named '%s' was found", name);
  MeshAttribute* attribute = &mesh->attributes[index];
  lovrGraphicsFlushMesh(mesh);
  lovrRelease(Buffer, attribute->buffer);
  map_remove(&mesh->attributeMap, hash);
  mesh->attributeNames[index][0] = '\0';
  memmove(mesh->attributeNames + index, mesh->attributeNames + index + 1, (mesh->attributeCount - index - 1) * MAX_ATTRIBUTE_NAME_LENGTH * sizeof(char));
  memmove(mesh->attributes + index, mesh->attributes + index + 1, (mesh->attributeCount - index - 1) * sizeof(MeshAttribute));
  mesh->attributeCount--;
  for (uint32_t i = 0; i < MAX_ATTRIBUTES; i++) {
    if (mesh->locations[i] > index) {
      mesh->locations[i]--;
    } else if (mesh->locations[i] == index) {
      mesh->locations[i] = 0xff;
    }
  }
}

const MeshAttribute* lovrMeshGetAttribute(Mesh* mesh, uint32_t index) {
  return index < mesh->attributeCount ? &mesh->attributes[index] : NULL;
}

uint32_t lovrMeshGetAttributeIndex(Mesh* mesh, const char* name) {
  uint64_t hash = hash64(name, strlen(name));
  uint64_t index = map_get(&mesh->attributeMap, hash);
  return index == MAP_NIL ? ~0u : index;
}

const char* lovrMeshGetAttributeName(Mesh* mesh, uint32_t index) {
  return mesh->attributeNames[index];
}

bool lovrMeshIsAttributeEnabled(Mesh* mesh, const char* name) {
  uint64_t hash = hash64(name, strlen(name));
  uint64_t index = map_get(&mesh->attributeMap, hash);
  lovrAssert(index != MAP_NIL, "Mesh does not have an attribute named '%s'", name);
  return !mesh->attributes[index].disabled;
}

void lovrMeshSetAttributeEnabled(Mesh* mesh, const char* name, bool enable) {
  bool disable = !enable;
  uint64_t hash = hash64(name, strlen(name));
  uint64_t index = map_get(&mesh->attributeMap, hash);
  lovrAssert(index != MAP_NIL, "Mesh does not have an attribute named '%s'", name);
  if (mesh->attributes[index].disabled != disable) {
    lovrGraphicsFlushMesh(mesh);
    mesh->attributes[index].disabled = disable;
  }
}

DrawMode lovrMeshGetDrawMode(Mesh* mesh) {
  return mesh->mode;
}

void lovrMeshSetDrawMode(Mesh* mesh, DrawMode mode) {
  mesh->mode = mode;
}

void lovrMeshGetDrawRange(Mesh* mesh, uint32_t* start, uint32_t* count) {
  *start = mesh->drawStart;
  *count = mesh->drawCount;
}

void lovrMeshSetDrawRange(Mesh* mesh, uint32_t start, uint32_t count) {
  uint32_t limit = mesh->indexSize > 0 ? mesh->indexCount : mesh->vertexCount;
  lovrAssert(start + count <= limit, "Invalid mesh draw range [%d, %d]", start + 1, start + count + 1);
  mesh->drawStart = start;
  mesh->drawCount = count;
}

Material* lovrMeshGetMaterial(Mesh* mesh) {
  return mesh->material;
}

void lovrMeshSetMaterial(Mesh* mesh, Material* material) {
  lovrRetain(material);
  lovrRelease(Material, mesh->material);
  mesh->material = material;
}
