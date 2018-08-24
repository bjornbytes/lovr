#include "graphics/graphics.h"
#include "graphics/canvas.h"
#include "graphics/mesh.h"
#include "graphics/shader.h"
#include "graphics/texture.h"
#include "resources/shaders.h"
#include "data/modelData.h"
#include "math/mat4.h"
#include "lib/vec/vec.h"
#include <math.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#if EMSCRIPTEN
#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>
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

static struct {
  Texture* defaultTexture;
  BlendMode blendMode;
  BlendAlphaMode blendAlphaMode;
  bool culling;
  bool depthEnabled;
  CompareMode depthTest;
  bool depthWrite;
  float lineWidth;
  bool stencilEnabled;
  CompareMode stencilMode;
  int stencilValue;
  bool stencilWriting;
  Winding winding;
  bool wireframe;
  uint32_t framebuffer;
  uint32_t indexBuffer;
  uint32_t program;
  Texture* textures[MAX_TEXTURES];
  Image images[MAX_IMAGES];
  uint32_t blockBuffers[2][MAX_BLOCK_BUFFERS];
  uint32_t vertexArray;
  uint32_t vertexBuffer;
  float viewport[4];
  vec_void_t incoherents[MAX_BARRIERS];
  bool srgb;
  bool singlepass;
  GraphicsLimits limits;
  GraphicsStats stats;
} state;

struct ShaderBlock {
  Ref ref;
  BlockType type;
  BufferUsage usage;
  vec_uniform_t uniforms;
  map_int_t uniformMap;
  uint32_t buffer;
  GLenum target;
  size_t size;
  void* data;
  bool mapped;
  uint8_t incoherent;
};

struct Shader {
  Ref ref;
  ShaderType type;
  uint32_t program;
  vec_uniform_t uniforms;
  vec_block_t blocks[2];
  map_int_t attributes;
  map_int_t uniformMap;
  map_int_t blockMap;
};

struct Texture {
  Ref ref;
  TextureType type;
  TextureFormat format;
  int width;
  int height;
  int depth;
  int mipmapCount;
  GLuint id;
  GLenum target;
  TextureFilter filter;
  TextureWrap wrap;
  bool srgb;
  bool mipmaps;
  bool allocated;
  uint8_t incoherent;
};

struct Canvas {
  Ref ref;
  uint32_t framebuffer;
  Attachment attachments[MAX_CANVAS_ATTACHMENTS];
  int count;
  bool dirty;
};

typedef struct {
  Mesh* mesh;
  int attributeIndex;
  int divisor;
  bool enabled;
} MeshAttachment;

typedef map_t(MeshAttachment) map_attachment_t;

struct Mesh {
  Ref ref;
  uint32_t count;
  VertexFormat format;
  MeshDrawMode drawMode;
  GLenum usage;
  VertexPointer data;
  IndexPointer indices;
  uint32_t indexCount;
  size_t indexSize;
  size_t indexCapacity;
  bool mappedIndices;
  uint32_t dirtyStart;
  uint32_t dirtyEnd;
  uint32_t rangeStart;
  uint32_t rangeCount;
  GLuint vao;
  GLuint vbo;
  GLuint ibo;
  Material* material;
  float* pose;
  map_attachment_t attachments;
  MeshAttachment layout[MAX_ATTACHMENTS];
  bool isAttachment;
};

// Helper functions

static void gammaCorrectColor(Color* color) {
  if (state.srgb) {
    color->r = lovrMathGammaToLinear(color->r);
    color->g = lovrMathGammaToLinear(color->g);
    color->b = lovrMathGammaToLinear(color->b);
  }
}

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
    case FORMAT_D32: return GL_DEPTH_COMPONENT;
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
    case FORMAT_D32: return GL_DEPTH_COMPONENT32;
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
    case FORMAT_D32: return GL_UNSIGNED_INT;
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
    case FORMAT_DXT1:
    case FORMAT_DXT3:
    case FORMAT_DXT5:
      return true;
    default:
      return false;
  }
}

static GLenum convertBufferUsage(BufferUsage usage) {
  switch (usage) {
    case USAGE_STATIC: return GL_STATIC_DRAW;
    case USAGE_DYNAMIC: return GL_DYNAMIC_DRAW;
    case USAGE_STREAM: return GL_STREAM_DRAW;
  }
}

static GLenum convertAccess(UniformAccess access) {
  switch (access) {
    case ACCESS_READ: return GL_READ_ONLY;
    case ACCESS_WRITE: return GL_WRITE_ONLY;
    case ACCESS_READ_WRITE: return GL_READ_WRITE;
  }
}

static GLenum convertMeshDrawMode(MeshDrawMode mode) {
  switch (mode) {
    case MESH_POINTS: return GL_POINTS;
    case MESH_LINES: return GL_LINES;
    case MESH_LINE_STRIP: return GL_LINE_STRIP;
    case MESH_LINE_LOOP: return GL_LINE_LOOP;
    case MESH_TRIANGLE_STRIP: return GL_TRIANGLE_STRIP;
    case MESH_TRIANGLES: return GL_TRIANGLES;
    case MESH_TRIANGLE_FAN: return GL_TRIANGLE_FAN;
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
    case GL_FLOAT_VEC2:
    case GL_INT_VEC2:
    case GL_FLOAT_MAT2:
      return 2;
    case GL_FLOAT_VEC3:
    case GL_INT_VEC3:
    case GL_FLOAT_MAT3:
      return 3;
    case GL_FLOAT_VEC4:
    case GL_INT_VEC4:
    case GL_FLOAT_MAT4:
      return 4;
    default:
      return 1;
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

static size_t getUniformTypeLength(const Uniform* uniform) {
  size_t size = 0;

  if (uniform->count > 1) {
    size += 2 + floor(log10(uniform->count)) + 1; // "[count]"
  }

  switch (uniform->type) {
    case UNIFORM_MATRIX: size += 3; break;
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

// TODO really ought to have TextureType-specific default textures
static Texture* lovrGpuGetDefaultTexture() {
  if (!state.defaultTexture) {
    TextureData* textureData = lovrTextureDataCreate(1, 1, 0xff, FORMAT_RGBA);
    state.defaultTexture = lovrTextureCreate(TEXTURE_2D, &textureData, 1, true, false);
    lovrRelease(textureData);
  }

  return state.defaultTexture;
}

// TODO this is pretty slow
static void lovrGpuCleanupIncoherentResource(void* resource, uint8_t incoherent) {
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

// GPU

static void lovrGpuBindFramebuffer(uint32_t framebuffer) {
  if (state.framebuffer != framebuffer) {
    state.framebuffer = framebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  }
}

static void lovrGpuBindIndexBuffer(uint32_t indexBuffer) {
  if (state.indexBuffer != indexBuffer) {
    state.indexBuffer = indexBuffer;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
  }
}

static void lovrGpuBindTexture(Texture* texture, int slot) {
  lovrAssert(slot >= 0 && slot < MAX_TEXTURES, "Invalid texture slot %d", slot);
  texture = texture ? texture : lovrGpuGetDefaultTexture();

  if (texture != state.textures[slot]) {
    lovrRetain(texture);
    lovrRelease(state.textures[slot]);
    state.textures[slot] = texture;
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(texture->target, texture->id);
  }
}

void lovrGpuDirtyTexture(int slot) {
  lovrAssert(slot >= 0 && slot < MAX_TEXTURES, "Invalid texture slot %d", slot);
  state.textures[slot] = NULL;
}

static void lovrGpuBindImage(Image* image, int slot) {
#ifndef EMSCRIPTEN
  lovrThrow("Shaders can not write to textures on this system");
#endif
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

    lovrRetain(texture);
    lovrRelease(state.images[slot].texture);
    glBindImageTexture(slot, texture->id, image->mipmap, image->slice == -1, image->slice, glAccess, glFormat);
    memcpy(state.images + slot, image, sizeof(Image));
  }
}

static void lovrGpuBindBlockBuffer(BlockType type, uint32_t buffer, int slot) {
#ifndef EMSCRIPTEN
  lovrAssert(type == BLOCK_UNIFORM, "Writable ShaderBlocks are not supported on this system");
#endif

  if (state.blockBuffers[type][slot] != buffer) {
    state.blockBuffers[type][slot] = buffer;
    glBindBufferBase(type == BLOCK_UNIFORM ? GL_UNIFORM_BUFFER : GL_SHADER_STORAGE_BUFFER, slot, buffer);
  }
}

static void lovrGpuBindVertexArray(uint32_t vertexArray) {
  if (state.vertexArray != vertexArray) {
    state.vertexArray = vertexArray;
    glBindVertexArray(vertexArray);
  }
}

static void lovrGpuBindVertexBuffer(uint32_t vertexBuffer) {
  if (state.vertexBuffer != vertexBuffer) {
    state.vertexBuffer = vertexBuffer;
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
  }
}

static void lovrGpuUseProgram(uint32_t program) {
  if (state.program != program) {
    state.program = program;
    glUseProgram(program);
    state.stats.shaderSwitches++;
  }
}

void lovrGpuInit(bool srgb, bool singlepass, gpuProc (*getProcAddress)(const char*)) {
#ifndef EMSCRIPTEN
  gladLoadGLLoader((GLADloadproc) getProcAddress);
  glEnable(GL_LINE_SMOOTH);
  glEnable(GL_PROGRAM_POINT_SIZE);
  if (srgb) {
    glEnable(GL_FRAMEBUFFER_SRGB);
  } else {
    glDisable(GL_FRAMEBUFFER_SRGB);
  }
  state.singlepass = singlepass && GLAD_GL_ARB_viewport_array && GLAD_GL_NV_viewport_array2 && GLAD_GL_NV_stereo_view_rendering;
#endif
  glEnable(GL_BLEND);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  state.srgb = srgb;
  state.blendMode = -1;
  state.blendAlphaMode = -1;
  state.culling = false;
  state.depthEnabled = false;
  state.depthTest = COMPARE_LESS;
  state.depthWrite = true;
  state.lineWidth = 1;
  state.stencilEnabled = false;
  state.stencilMode = COMPARE_NONE;
  state.stencilValue = 0;
  state.stencilWriting = false;
  state.winding = WINDING_COUNTERCLOCKWISE;
  state.wireframe = false;
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
}

void lovrGpuClear(Canvas* canvas, Color* color, float* depth, int* stencil) {
  lovrCanvasBind(canvas);

  if (color) {
    gammaCorrectColor(color);
    int count = canvas ? canvas->count : 1;
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

void lovrGraphicsStencil(StencilAction action, int replaceValue, StencilCallback callback, void* userdata) {
  state.depthWrite = false;
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
  state.stencilWriting = false;

  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  state.stencilMode = ~0; // Dirty
}

void lovrGpuDraw(DrawCommand* command) {
  Mesh* mesh = command->mesh;
  Material* material = command->material;
  Shader* shader = command->shader;
  Pipeline* pipeline = &command->pipeline;
  Canvas* canvas = pipeline->canvas ? pipeline->canvas : command->camera.canvas;
  int instances = command->instances;

  // Bind shader
  lovrGpuUseProgram(shader->program);

  // Blend mode
  if (state.blendMode != pipeline->blendMode || state.blendAlphaMode != pipeline->blendAlphaMode) {
    state.blendMode = pipeline->blendMode;
    state.blendAlphaMode = pipeline->blendAlphaMode;

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

      case BLEND_REPLACE:
        glBlendEquation(GL_FUNC_ADD);
        glBlendFuncSeparate(srcRGB, GL_ZERO, GL_ONE, GL_ZERO);
        break;
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
  if (state.depthWrite != pipeline->depthWrite) {
    state.depthWrite = pipeline->depthWrite;
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
  if (state.wireframe != pipeline->wireframe) {
    state.wireframe = pipeline->wireframe;
#ifndef EMSCRIPTEN
    glPolygonMode(GL_FRONT_AND_BACK, state.wireframe ? GL_LINE : GL_FILL);
#endif
  }

  // Canvas
  lovrCanvasBind(canvas);

  // Transform
  lovrShaderSetMatrices(shader, "lovrModel", command->transform, 0, 16);
  lovrShaderSetMatrices(shader, "lovrViews", command->camera.viewMatrix[0], 0, 32);
  lovrShaderSetMatrices(shader, "lovrProjections", command->camera.projection[0], 0, 32);

  float modelView[32];
  mat4_multiply(mat4_set(modelView, command->camera.viewMatrix[0]), command->transform);
  mat4_multiply(mat4_set(modelView + 16, command->camera.viewMatrix[1]), command->transform);
  lovrShaderSetMatrices(shader, "lovrTransforms", modelView, 0, 32);

  if (lovrShaderHasUniform(shader, "lovrNormalMatrices")) {
    if (mat4_invert(modelView) && mat4_invert(modelView + 16)) {
      mat4_transpose(modelView);
      mat4_transpose(modelView + 16);
    } else {
      mat4_identity(modelView);
      mat4_identity(modelView + 16);
    }

    float normalMatrices[18] = {
      modelView[0], modelView[1], modelView[2],
      modelView[4], modelView[5], modelView[6],
      modelView[8], modelView[9], modelView[10],

      modelView[16], modelView[17], modelView[18],
      modelView[20], modelView[21], modelView[22],
      modelView[24], modelView[25], modelView[26]
    };

    lovrShaderSetMatrices(shader, "lovrNormalMatrices", normalMatrices, 0, 18);
  }

  // Pose
  float* pose = lovrMeshGetPose(mesh);
  if (pose) {
    lovrShaderSetMatrices(shader, "lovrPose", pose, 0, MAX_BONES * 16);
  } else {
    float identity[16];
    mat4_identity(identity);
    lovrShaderSetMatrices(shader, "lovrPose", identity, 0, 16);
  }

  // Point size
  lovrShaderSetFloats(shader, "lovrPointSize", &pipeline->pointSize, 0, 1);

  // Color
  Color color = pipeline->color;
  gammaCorrectColor(&color);
  float data[4] = { color.r, color.g, color.b, color.a };
  lovrShaderSetFloats(shader, "lovrColor", data, 0, 4);

  // Material
  for (int i = 0; i < MAX_MATERIAL_SCALARS; i++) {
    float value = lovrMaterialGetScalar(material, i);
    lovrShaderSetFloats(shader, lovrShaderScalarUniforms[i], &value, 0, 1);
  }

  for (int i = 0; i < MAX_MATERIAL_COLORS; i++) {
    Color color = lovrMaterialGetColor(material, i);
    gammaCorrectColor(&color);
    float data[4] = { color.r, color.g, color.b, color.a };
    lovrShaderSetFloats(shader, lovrShaderColorUniforms[i], data, 0, 4);
  }

  for (int i = 0; i < MAX_MATERIAL_TEXTURES; i++) {
    Texture* texture = lovrMaterialGetTexture(material, i);
    lovrShaderSetTextures(shader, lovrShaderTextureUniforms[i], &texture, 0, 1);
  }

  lovrShaderSetMatrices(shader, "lovrMaterialTransform", material->transform, 0, 9);

  // Bind attributes
  lovrMeshBind(mesh, shader);

  bool stereo = false;
  int drawCount = 1 + (stereo == true && !state.singlepass);

  // Draw (TODEW)
  for (int i = 0; i < drawCount; i++) {

    // Bind uniforms
    lovrShaderSetInts(shader, "lovrIsStereo", &(int) { stereo && state.singlepass }, 0, 1);
    lovrShaderSetInts(shader, "_lovrEye", &i, 0, 1);
    lovrShaderBind(shader);

    uint32_t rangeStart, rangeCount;
    lovrMeshGetDrawRange(mesh, &rangeStart, &rangeCount);
    uint32_t indexCount;
    size_t indexSize;
    lovrMeshReadIndices(mesh, &indexCount, &indexSize);
    GLenum glDrawMode = convertMeshDrawMode(lovrMeshGetDrawMode(mesh));
    if (indexCount > 0) {
      size_t count = rangeCount ? rangeCount : indexCount;
      GLenum indexType = indexSize == sizeof(uint16_t) ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
      size_t offset = rangeStart * indexSize;
      if (instances > 1) {
        glDrawElementsInstanced(glDrawMode, count, indexType, (GLvoid*) offset, instances);
      } else {
        glDrawElements(glDrawMode, count, indexType, (GLvoid*) offset);
      }
    } else {
      size_t count = rangeCount ? rangeCount : lovrMeshGetVertexCount(mesh);
      if (instances > 1) {
        glDrawArraysInstanced(glDrawMode, rangeStart, count, instances);
      } else {
        glDrawArrays(glDrawMode, rangeStart, count);
      }
    }

    state.stats.drawCalls++;
  }
}

void lovrGpuCompute(Shader* shader, int x, int y, int z) {
#ifdef EMSCRIPTEN
  lovrThrow("Compute shaders are not supported on this system");
#else
  lovrAssert(GLAD_GL_ARB_compute_shader, "Compute shaders are not supported on this system");
  lovrAssert(shader->type == SHADER_COMPUTE, "Attempt to use a non-compute shader for a compute operation");
  lovrGpuUseProgram(shader->program);
  lovrShaderBind(shader);
  glDispatchCompute(x, y, z);
#endif
}

void lovrGpuWait(uint8_t flags) {
#ifndef EMSCRIPTEN
  if (!GL_ARB_shader_image_load_store || !flags) {
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
      for (int j = 0; j < state.incoherents[j].length; j++) {
        ShaderBlock* block = state.incoherents[i].data[j];
        block->incoherent &= ~(1 << i);
      }
    } else {
      for (int j = 0; j < state.incoherents[j].length; j++) {
        Texture* texture = state.incoherents[i].data[j];
        texture->incoherent &= ~(1 << i);
      }
    }

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
#endif
}

void lovrGpuPresent() {
  memset(&state.stats, 0, sizeof(state.stats));
#ifdef __APPLE__
  // For some reason instancing doesn't work on macOS unless you reset the shader every frame
  lovrGpuUseProgram(0);
#endif
}

GraphicsFeatures lovrGraphicsGetSupported() {
  return (GraphicsFeatures) {
#ifdef EMSCRIPTEN
    .computeShaders = false,
    .writableBlocks = false
#else
    .computeShaders = GLAD_GL_ARB_compute_shader,
    .writableBlocks = GLAD_GL_ARB_shader_storage_buffer_object
#endif
  };
}

GraphicsLimits lovrGraphicsGetLimits() {
  if (!state.limits.initialized) {
#ifdef EMSCRIPTEN
    glGetFloatv(GL_ALIASED_POINT_SIZE_RANGE, state.limits.pointSizes);
#else
    glGetFloatv(GL_POINT_SIZE_RANGE, state.limits.pointSizes);
#endif
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &state.limits.textureSize);
    glGetIntegerv(GL_MAX_SAMPLES, &state.limits.textureMSAA);
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &state.limits.textureAnisotropy);
    state.limits.initialized = 1;
  }

  return state.limits;
}

GraphicsStats lovrGraphicsGetStats() {
  return state.stats;
}

// Texture

Texture* lovrTextureCreate(TextureType type, TextureData** slices, int sliceCount, bool srgb, bool mipmaps) {
  Texture* texture = lovrAlloc(Texture, lovrTextureDestroy);
  if (!texture) return NULL;

  texture->type = type;
  texture->srgb = srgb;
  texture->mipmaps = mipmaps;

  switch (type) {
    case TEXTURE_2D: texture->target = GL_TEXTURE_2D; break;
    case TEXTURE_ARRAY: texture->target = GL_TEXTURE_2D_ARRAY; break;
    case TEXTURE_CUBE: texture->target = GL_TEXTURE_CUBE_MAP; break;
    case TEXTURE_VOLUME: texture->target = GL_TEXTURE_3D; break;
  }

  WrapMode wrap = type == TEXTURE_CUBE ? WRAP_CLAMP : WRAP_REPEAT;
  glGenTextures(1, &texture->id);
  lovrGpuBindTexture(texture, 0);
  lovrTextureSetFilter(texture, lovrGraphicsGetDefaultFilter());
  lovrTextureSetWrap(texture, (TextureWrap) { .s = wrap, .t = wrap, .r = wrap });

  if (sliceCount > 0) {
    lovrTextureAllocate(texture, slices[0]->width, slices[0]->height, sliceCount, slices[0]->format);
    for (int i = 0; i < sliceCount; i++) {
      lovrTextureReplacePixels(texture, slices[i], 0, 0, i, 0);
    }
  }

  return texture;
}

void lovrTextureDestroy(void* ref) {
  Texture* texture = ref;
  glDeleteTextures(1, &texture->id);
  lovrGpuCleanupIncoherentResource(texture, texture->incoherent);
  free(texture);
}

void lovrTextureAllocate(Texture* texture, int width, int height, int depth, TextureFormat format) {
  int maxSize = lovrGraphicsGetLimits().textureSize;
  lovrAssert(!texture->allocated, "Texture is already allocated");
  lovrAssert(texture->type != TEXTURE_CUBE || width == height, "Cubemap images must be square");
  lovrAssert(texture->type != TEXTURE_CUBE || depth == 6, "6 images are required for a cube texture\n");
  lovrAssert(texture->type != TEXTURE_2D || depth == 1, "2D textures can only contain a single image");
  lovrAssert(width < maxSize, "Texture width %d exceeds max of %d", width, maxSize);
  lovrAssert(height < maxSize, "Texture height %d exceeds max of %d", height, maxSize);

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

  bool srgb = lovrGraphicsIsGammaCorrect() && texture->srgb;
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
}

void lovrTextureReplacePixels(Texture* texture, TextureData* textureData, int x, int y, int slice, int mipmap) {
  lovrAssert(texture->allocated, "Texture is not allocated");
  lovrAssert(textureData->blob.data, "Trying to replace Texture pixels with empty pixel data");

  if ((texture->incoherent >> BARRIER_TEXTURE) & 1) {
    lovrGpuWait(1 << BARRIER_TEXTURE);
  }

  int maxWidth = lovrTextureGetWidth(texture, mipmap);
  int maxHeight = lovrTextureGetHeight(texture, mipmap);
  int width = textureData->width;
  int height = textureData->height;
  bool overflow = (x + width > maxWidth) || (y + height > maxHeight);
  lovrAssert(!overflow, "Trying to replace pixels outside the texture's bounds");
  lovrAssert(mipmap >= 0 && mipmap < texture->mipmapCount, "Invalid mipmap level %d", mipmap);
  GLenum glFormat = convertTextureFormat(textureData->format);
  GLenum glInternalFormat = convertTextureFormatInternal(textureData->format, texture->srgb);
  GLenum glType = convertTextureFormatType(textureData->format);
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
      glGenerateMipmap(texture->target);
    }
  }
}

GLuint lovrTextureGetId(Texture* texture) {
  return texture->id;
}

int lovrTextureGetWidth(Texture* texture, int mipmap) {
  return MAX(texture->width >> mipmap, 1);
}

int lovrTextureGetHeight(Texture* texture, int mipmap) {
  return MAX(texture->height >> mipmap, 1);
}

int lovrTextureGetDepth(Texture* texture, int mipmap) {
  return texture->type == TEXTURE_VOLUME ? MAX(texture->depth >> mipmap, 1) : texture->depth;
}

int lovrTextureGetMipmapCount(Texture* texture) {
  return texture->mipmapCount;
}

TextureType lovrTextureGetType(Texture* texture) {
  return texture->type;
}

TextureFormat lovrTextureGetFormat(Texture* texture) {
  return texture->format;
}

TextureFilter lovrTextureGetFilter(Texture* texture) {
  return texture->filter;
}

void lovrTextureSetFilter(Texture* texture, TextureFilter filter) {
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

TextureWrap lovrTextureGetWrap(Texture* texture) {
  return texture->wrap;
}

void lovrTextureSetWrap(Texture* texture, TextureWrap wrap) {
  texture->wrap = wrap;
  lovrGpuBindTexture(texture, 0);
  glTexParameteri(texture->target, GL_TEXTURE_WRAP_S, convertWrapMode(wrap.s));
  glTexParameteri(texture->target, GL_TEXTURE_WRAP_T, convertWrapMode(wrap.t));
  if (texture->type == TEXTURE_CUBE || texture->type == TEXTURE_VOLUME) {
    glTexParameteri(texture->target, GL_TEXTURE_WRAP_R, convertWrapMode(wrap.r));
  }
}

// Canvas

Canvas* lovrCanvasCreate() {
  Canvas* canvas = lovrAlloc(Canvas, lovrCanvasDestroy);
  if (!canvas) return NULL;

  glGenFramebuffers(1, &canvas->framebuffer);

  return canvas;
}

void lovrCanvasDestroy(void* ref) {
  Canvas* canvas = ref;
  glDeleteFramebuffers(1, &canvas->framebuffer);
  for (int i = 0; i < MAX_CANVAS_ATTACHMENTS; i++) {
    lovrRelease(canvas->attachments[i].texture);
  }
  free(ref);
}

const Attachment* lovrCanvasGetAttachments(Canvas* canvas, int* count) {
  *count = canvas->count;
  return canvas->attachments;
}

void lovrCanvasSetAttachments(Canvas* canvas, Attachment* attachments, int count) {
  lovrAssert(count > 0, "A Canvas must have at least one attached Texture");
  lovrAssert(count <= MAX_CANVAS_ATTACHMENTS, "Only %d textures can be attached to a Canvas, got %d\n", MAX_CANVAS_ATTACHMENTS, count);

  if (canvas->dirty || memcmp(canvas->attachments, attachments, count * sizeof(Attachment))) {
    memcpy(canvas->attachments, attachments, count * sizeof(Attachment));
    canvas->count = count;
    canvas->dirty = true;
  }
}

void lovrCanvasBind(Canvas* canvas) {
  if (!canvas) {
    lovrGpuBindFramebuffer(0);
    return;
  }

  lovrGpuBindFramebuffer(canvas->framebuffer);

  if (!canvas->dirty) {
    return;
  }

  // We need to synchronize if any of the Canvas attachments have pending writes on them
  for (int i = 0; i < canvas->count; i++) {
    Texture* texture = canvas->attachments[i].texture;
    if (texture->incoherent && (texture->incoherent >> BARRIER_CANVAS) & 1) {
      lovrGpuWait(1 << BARRIER_CANVAS);
      break;
    }
  }

  GLenum buffers[MAX_CANVAS_ATTACHMENTS] = { GL_NONE };
  for (int i = 0; i < canvas->count; i++) {
    GLenum buffer = buffers[i] = GL_COLOR_ATTACHMENT0 + i;
    Attachment* attachment = &canvas->attachments[i];
    Texture* texture = attachment->texture;
    int slice = attachment->slice;
    int level = attachment->level;

    switch (texture->type) {
      case TEXTURE_2D: glFramebufferTexture2D(GL_FRAMEBUFFER, buffer, GL_TEXTURE_2D, texture->id, level); break;
      case TEXTURE_CUBE: glFramebufferTexture2D(GL_FRAMEBUFFER, buffer, GL_TEXTURE_CUBE_MAP_POSITIVE_X + slice, texture->id, level); break;
      case TEXTURE_ARRAY: glFramebufferTextureLayer(GL_FRAMEBUFFER, buffer, texture->id, level, slice); break;
      case TEXTURE_VOLUME: glFramebufferTexture3D(GL_FRAMEBUFFER, buffer, GL_TEXTURE_3D, texture->id, level, slice); break;
    }
  }
  glDrawBuffers(canvas->count, buffers);

  GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  lovrAssert(status == GL_FRAMEBUFFER_COMPLETE, "Unable to bind framebuffer");

  canvas->dirty = false;
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
        uniform.size = uniform.components * uniform.components * uniform.count * sizeof(int);
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
        case UNIFORM_FLOAT:
          glGetUniformfv(program, location, &uniform.value.floats[offset]);
          offset += uniform.components;
          break;

        case UNIFORM_INT:
          glGetUniformiv(program, location, &uniform.value.ints[offset]);
          offset += uniform.components;
          break;

        case UNIFORM_MATRIX:
          glGetUniformfv(program, location, &uniform.value.floats[offset]);
          offset += uniform.components * uniform.components;
          break;

        default: break;
      }
    }

    map_set(&shader->uniformMap, uniform.name, shader->uniforms.length);
    vec_push(&shader->uniforms, uniform);
    textureSlot += uniform.type == UNIFORM_SAMPLER ? uniform.count : 0;
    imageSlot += uniform.type == UNIFORM_IMAGE ? uniform.count : 0;
  }
}

Shader* lovrShaderCreateGraphics(const char* vertexSource, const char* fragmentSource) {
  Shader* shader = lovrAlloc(Shader, lovrShaderDestroy);
  if (!shader) return NULL;

  // Vertex
  vertexSource = vertexSource == NULL ? lovrDefaultVertexShader : vertexSource;
  const char* vertexSources[] = { lovrShaderVertexPrefix, vertexSource, lovrShaderVertexSuffix };
  GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSources, sizeof(vertexSources) / sizeof(vertexSources[0]));

  // Fragment
  fragmentSource = fragmentSource == NULL ? lovrDefaultFragmentShader : fragmentSource;
  const char* fragmentSources[] = { lovrShaderFragmentPrefix, fragmentSource, lovrShaderFragmentSuffix };
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
  linkProgram(program);
  glDetachShader(program, vertexShader);
  glDeleteShader(vertexShader);
  glDetachShader(program, fragmentShader);
  glDeleteShader(fragmentShader);
  shader->program = program;
  shader->type = SHADER_GRAPHICS;

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

Shader* lovrShaderCreateCompute(const char* source) {
  Shader* shader = lovrAlloc(Shader, lovrShaderDestroy);
  if (!shader) return NULL;

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

Shader* lovrShaderCreateDefault(DefaultShader type) {
  switch (type) {
    case SHADER_DEFAULT: return lovrShaderCreateGraphics(NULL, NULL);
    case SHADER_CUBE: return lovrShaderCreateGraphics(lovrCubeVertexShader, lovrCubeFragmentShader); break;
    case SHADER_PANO: return lovrShaderCreateGraphics(lovrCubeVertexShader, lovrPanoFragmentShader); break;
    case SHADER_FONT: return lovrShaderCreateGraphics(NULL, lovrFontFragmentShader);
    case SHADER_FILL: return lovrShaderCreateGraphics(lovrFillVertexShader, NULL);
    default: lovrThrow("Unknown default shader type"); return NULL;
  }
}

void lovrShaderDestroy(void* ref) {
  Shader* shader = ref;
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
  free(shader);
}

ShaderType lovrShaderGetType(Shader* shader) {
  return shader->type;
}

void lovrShaderBind(Shader* shader) {
  UniformBlock* block;
  Uniform* uniform;
  int i;

  // Figure out if we need to wait for pending writes on resources to complete
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

  lovrGpuWait(flags);

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
        for (int i = 0; i < count; i++) {
          Image* image = &uniform->value.images[i];
          Texture* texture = image->texture;
          lovrAssert(!texture || texture->type == uniform->textureType, "Uniform texture type mismatch for uniform %s", uniform->name);

          // If the Shader can write to the texture, mark it as incoherent
          if (texture && image->access != ACCESS_READ) {
            texture->incoherent |= 1 << BARRIER_UNIFORM_TEXTURE;
            texture->incoherent |= 1 << BARRIER_UNIFORM_IMAGE;
            texture->incoherent |= 1 << BARRIER_TEXTURE;
            texture->incoherent |= 1 << BARRIER_CANVAS;
          }

          lovrGpuBindImage(image, uniform->baseSlot + i);
        }
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

        // If the Shader can write to the block, mark it as incoherent
        bool writable = type == BLOCK_STORAGE && block->access != ACCESS_READ;
        block->source->incoherent |= writable ? (1 << BARRIER_BLOCK) : 0;
        lovrShaderBlockUnmap(block->source);
        lovrGpuBindBlockBuffer(type, block->source->buffer, block->slot);
      } else {
        lovrGpuBindBlockBuffer(type, 0, block->slot);
      }
    }
  }
}

int lovrShaderGetAttributeId(Shader* shader, const char* name) {
  int* id = map_get(&shader->attributes, name);
  return id ? *id : -1;
}

bool lovrShaderHasUniform(Shader* shader, const char* name) {
  return map_get(&shader->uniformMap, name) != NULL;
}

const Uniform* lovrShaderGetUniform(Shader* shader, const char* name) {
  int* index = map_get(&shader->uniformMap, name);
  if (!index) {
    return false;
  }

  return &shader->uniforms.data[*index];
}

static void lovrShaderSetUniform(Shader* shader, const char* name, UniformType type, void* data, int start, int count, int size, const char* debug) {
  int* index = map_get(&shader->uniformMap, name);
  if (!index) {
    return;
  }

  Uniform* uniform = &shader->uniforms.data[*index];
  const char* plural = (uniform->size / size) > 1 ? "s" : "";
  lovrAssert(uniform->type == type, "Unable to send %ss to uniform %s", debug, name);
  lovrAssert((start + count) * size <= uniform->size, "Too many %s%s for uniform %s, maximum is %d", debug, plural, name, uniform->size / size);

  void* dest = uniform->value.bytes + start * size;
  if (!uniform->dirty && !memcmp(dest, data, count * size)) {
    return;
  }

  memcpy(dest, data, count * size);
  uniform->dirty = true;
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

void lovrShaderSetBlock(Shader* shader, const char* name, ShaderBlock* source, UniformAccess access) {
  int* id = map_get(&shader->blockMap, name);
  lovrAssert(id, "No shader block named '%s'", name);

  int type = *id & 1;
  int index = *id >> 1;
  UniformBlock* block = &shader->blocks[type].data[index];
  block->access = access;

  if (source != block->source) {
    if (source) {
      lovrAssert(block->uniforms.length == source->uniforms.length, "ShaderBlock must have same number of uniforms as block definition in Shader");
      for (int i = 0; i < block->uniforms.length; i++) {
        const Uniform* u = &block->uniforms.data[i];
        const Uniform* v = &source->uniforms.data[i];
        lovrAssert(u->type == v->type, "Shader is not compatible with ShaderBlock, check type of variable '%s'", v->name);
        lovrAssert(u->offset == v->offset, "Shader is not compatible with ShaderBlock, check order of variable '%s'", v->name);

        // This check is disabled due to observed driver bugs with std140 layouts
        // lovrAssert(u->size == v->size, "Shader is not compatible with ShaderBlock, check count of variable '%s'", v->name);
      }
    }

    lovrRetain(source);
    lovrRelease(block->source);
    block->source = source;
  }
}

// ShaderBlock

ShaderBlock* lovrShaderBlockCreate(vec_uniform_t* uniforms, BlockType type, BufferUsage usage) {
  ShaderBlock* block = lovrAlloc(ShaderBlock, lovrShaderBlockDestroy);
  if (!block) return NULL;

  lovrAssert(type != BLOCK_STORAGE || lovrGraphicsGetSupported().writableBlocks, "Writable ShaderBlocks are not supported on this system");

  vec_init(&block->uniforms);
  vec_extend(&block->uniforms, uniforms);
  map_init(&block->uniformMap);

  int i;
  Uniform* uniform;
  size_t size = 0;
  vec_foreach_ptr(&block->uniforms, uniform, i) {

    // Calculate size and offset
    size_t align;
    if (uniform->count > 1 || uniform->type == UNIFORM_MATRIX) {
      align = 16 * (uniform->type == UNIFORM_MATRIX ? uniform->components : 1);
      uniform->size = align * uniform->count;
    } else {
      align = (uniform->components + (uniform->components == 3)) * 4;
      uniform->size = uniform->components * 4;
    }
    uniform->offset = (size + (align - 1)) & -align;
    size = uniform->offset + uniform->size;

    // Write index to uniform lookup
    map_set(&block->uniformMap, uniform->name, i);
  }

#ifdef EMSCRIPTEN
  block->target = GL_UNIFORM_BUFFER;
#else
  block->target = block->type == BLOCK_UNIFORM ? GL_UNIFORM_BUFFER : GL_SHADER_STORAGE_BUFFER;
#endif
  block->type = type;
  block->usage = convertBufferUsage(usage);
  block->size = size;
  block->data = calloc(1, size);

  glGenBuffers(1, &block->buffer);
  lovrGpuBindBlockBuffer(block->type, block->buffer, 0);
  glBufferData(block->target, size, NULL, usage);

  return block;
}

void lovrShaderBlockDestroy(void* ref) {
  ShaderBlock* block = ref;
  lovrGpuCleanupIncoherentResource(block, block->incoherent);
  glDeleteBuffers(1, &block->buffer);
  vec_deinit(&block->uniforms);
  map_deinit(&block->uniformMap);
  free(block->data);
  free(block);
}

size_t lovrShaderBlockGetSize(ShaderBlock* block) {
  return block->size;
}

BlockType lovrShaderBlockGetType(ShaderBlock* block) {
  return block->type;
}

char* lovrShaderBlockGetShaderCode(ShaderBlock* block, const char* blockName, size_t* length) {

  // Calculate
  size_t size = 0;
  size_t tab = 2;
  size += 15; // "layout(std140) "
  size += block->type == BLOCK_UNIFORM ? 7 : 6; // "uniform" || "buffer"
  size += 1; // " "
  size += strlen(blockName);
  size += 3; // " {\n"
  for (int i = 0; i < block->uniforms.length; i++) {
    size += tab;
    size += getUniformTypeLength(&block->uniforms.data[i]);
    size += 1; // " "
    size += strlen(block->uniforms.data[i].name);
    size += 2; // ";\n"
  }
  size += 3; // "};\n"

  // Allocate
  char* code = malloc(size + 1);

  // Concatenate
  char* s = code;
  s += sprintf(s, "layout(std140) %s %s {\n", block->type == BLOCK_UNIFORM ? "uniform" : "buffer", blockName);
  for (int i = 0; i < block->uniforms.length; i++) {
    const Uniform* uniform = &block->uniforms.data[i];
    if (uniform->count > 1) {
      s += sprintf(s, "  %s %s[%d];\n", getUniformTypeName(uniform), uniform->name, uniform->count);
    } else {
      s += sprintf(s, "  %s %s;\n", getUniformTypeName(uniform), uniform->name);
    }
  }
  s += sprintf(s, "};\n");
  *s = '\0';

  *length = size;
  return code;
}

const Uniform* lovrShaderBlockGetUniform(ShaderBlock* block, const char* name) {
  int* index = map_get(&block->uniformMap, name);
  if (!index) return NULL;

  return &block->uniforms.data[*index];
}

void* lovrShaderBlockMap(ShaderBlock* block) {
  block->mapped = true;
  return block->data;
}

void lovrShaderBlockUnmap(ShaderBlock* block) {
  if (!block->mapped) {
    return;
  }

  lovrGpuBindBlockBuffer(block->type, block->buffer, 0);
  glBufferData(block->target, block->size, NULL, block->usage);
  glBufferSubData(block->target, 0, block->size, block->data);
  block->mapped = false;
}

// Mesh

Mesh* lovrMeshCreate(uint32_t count, VertexFormat format, MeshDrawMode drawMode, BufferUsage usage) {
  Mesh* mesh = lovrAlloc(Mesh, lovrMeshDestroy);
  if (!mesh) return NULL;

  mesh->count = count;
  mesh->format = format;
  mesh->drawMode = drawMode;
  mesh->usage = convertBufferUsage(usage);

  glGenBuffers(1, &mesh->vbo);
  glGenBuffers(1, &mesh->ibo);
  lovrGpuBindVertexBuffer(mesh->vbo);
  glBufferData(GL_ARRAY_BUFFER, count * format.stride, NULL, mesh->usage);
  glGenVertexArrays(1, &mesh->vao);

  map_init(&mesh->attachments);
  for (int i = 0; i < format.count; i++) {
    map_set(&mesh->attachments, format.attributes[i].name, ((MeshAttachment) { mesh, i, 0, true }));
  }

  mesh->data.raw = calloc(count, format.stride);

  return mesh;
}

void lovrMeshDestroy(void* ref) {
  Mesh* mesh = ref;
  lovrRelease(mesh->material);
  free(mesh->data.raw);
  free(mesh->indices.raw);
  glDeleteBuffers(1, &mesh->vbo);
  glDeleteBuffers(1, &mesh->ibo);
  glDeleteVertexArrays(1, &mesh->vao);
  const char* key;
  map_iter_t iter = map_iter(&mesh->attachments);
  while ((key = map_next(&mesh->attachments, &iter)) != NULL) {
    MeshAttachment* attachment = map_get(&mesh->attachments, key);
    if (attachment->mesh != mesh) {
      lovrRelease(attachment->mesh);
    }
  }
  map_deinit(&mesh->attachments);
  free(mesh);
}

void lovrMeshAttachAttribute(Mesh* mesh, Mesh* other, const char* name, int divisor) {
  MeshAttachment* otherAttachment = map_get(&other->attachments, name);
  lovrAssert(!mesh->isAttachment, "Attempted to attach to a mesh which is an attachment itself");
  lovrAssert(otherAttachment, "No attribute named '%s' exists", name);
  lovrAssert(!map_get(&mesh->attachments, name), "Mesh already has an attribute named '%s'", name);
  lovrAssert(divisor >= 0, "Divisor can't be negative");

  MeshAttachment attachment = { other, otherAttachment->attributeIndex, divisor, true };
  map_set(&mesh->attachments, name, attachment);
  other->isAttachment = true;
  lovrRetain(other);
}

void lovrMeshDetachAttribute(Mesh* mesh, const char* name) {
  MeshAttachment* attachment = map_get(&mesh->attachments, name);
  lovrAssert(attachment, "No attached attribute '%s' was found", name);
  lovrAssert(attachment->mesh != mesh, "Attribute '%s' was not attached from another Mesh", name);
  lovrRelease(attachment->mesh);
  map_remove(&mesh->attachments, name);
}

void lovrMeshBind(Mesh* mesh, Shader* shader) {
  const char* key;
  map_iter_t iter = map_iter(&mesh->attachments);

  MeshAttachment layout[MAX_ATTACHMENTS];
  memset(layout, 0, MAX_ATTACHMENTS * sizeof(MeshAttachment));

  lovrGpuBindVertexArray(mesh->vao);
  lovrMeshUnmapVertices(mesh);
  lovrMeshUnmapIndices(mesh);
  if (mesh->indexCount > 0) {
    lovrGpuBindIndexBuffer(mesh->ibo);
  }

  while ((key = map_next(&mesh->attachments, &iter)) != NULL) {
    int location = lovrShaderGetAttributeId(shader, key);

    if (location >= 0) {
      MeshAttachment* attachment = map_get(&mesh->attachments, key);
      layout[location] = *attachment;
      lovrMeshUnmapVertices(attachment->mesh);
      lovrMeshUnmapIndices(attachment->mesh);
    }
  }

  for (int i = 0; i < MAX_ATTACHMENTS; i++) {
    MeshAttachment previous = mesh->layout[i];
    MeshAttachment current = layout[i];

    if (!memcmp(&previous, &current, sizeof(MeshAttachment))) {
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
      glVertexAttribDivisor(i, current.divisor);
    }

    if (previous.mesh != current.mesh || previous.attributeIndex != current.attributeIndex) {
      lovrGpuBindVertexBuffer(current.mesh->vbo);
      VertexFormat* format = &current.mesh->format;
      Attribute attribute = format->attributes[current.attributeIndex];
      switch (attribute.type) {
        case ATTR_FLOAT:
          glVertexAttribPointer(i, attribute.count, GL_FLOAT, GL_TRUE, format->stride, (void*) attribute.offset);
          break;

        case ATTR_BYTE:
          glVertexAttribPointer(i, attribute.count, GL_UNSIGNED_BYTE, GL_TRUE, format->stride, (void*) attribute.offset);
          break;

        case ATTR_INT:
          glVertexAttribIPointer(i, attribute.count, GL_UNSIGNED_INT, format->stride, (void*) attribute.offset);
          break;
      }
    }

    mesh->layout[i] = current;
  }
}

VertexFormat* lovrMeshGetVertexFormat(Mesh* mesh) {
  return &mesh->format;
}

MeshDrawMode lovrMeshGetDrawMode(Mesh* mesh) {
  return mesh->drawMode;
}

void lovrMeshSetDrawMode(Mesh* mesh, MeshDrawMode drawMode) {
  mesh->drawMode = drawMode;
}

int lovrMeshGetVertexCount(Mesh* mesh) {
  return mesh->count;
}

bool lovrMeshIsAttributeEnabled(Mesh* mesh, const char* name) {
  MeshAttachment* attachment = map_get(&mesh->attachments, name);
  lovrAssert(attachment, "Mesh does not have an attribute named '%s'", name);
  return attachment->enabled;
}

void lovrMeshSetAttributeEnabled(Mesh* mesh, const char* name, bool enable) {
  MeshAttachment* attachment = map_get(&mesh->attachments, name);
  lovrAssert(attachment, "Mesh does not have an attribute named '%s'", name);
  attachment->enabled = enable;
}

void lovrMeshGetDrawRange(Mesh* mesh, uint32_t* start, uint32_t* count) {
  *start = mesh->rangeStart;
  *count = mesh->rangeCount;
}

void lovrMeshSetDrawRange(Mesh* mesh, uint32_t start, uint32_t count) {
  uint32_t limit = mesh->indexCount > 0 ? mesh->indexCount : mesh->count;
  lovrAssert(start + count <= limit, "Invalid mesh draw range [%d, %d]", start + 1, start + count + 1);
  mesh->rangeStart = start;
  mesh->rangeCount = count;
}

Material* lovrMeshGetMaterial(Mesh* mesh) {
  return mesh->material;
}

void lovrMeshSetMaterial(Mesh* mesh, Material* material) {
  if (mesh->material != material) {
    lovrRetain(material);
    lovrRelease(mesh->material);
    mesh->material = material;
  }
}

float* lovrMeshGetPose(Mesh* mesh) {
  return mesh->pose;
}

void lovrMeshSetPose(Mesh* mesh, float* pose) {
  mesh->pose = pose;
}

VertexPointer lovrMeshMapVertices(Mesh* mesh, uint32_t start, uint32_t count, bool read, bool write) {
  if (write) {
    mesh->dirtyStart = MIN(mesh->dirtyStart, start);
    mesh->dirtyEnd = MAX(mesh->dirtyEnd, start + count);
  }

  return (VertexPointer) { .bytes = mesh->data.bytes + start * mesh->format.stride };
}

void lovrMeshUnmapVertices(Mesh* mesh) {
  if (mesh->dirtyEnd == 0) {
    return;
  }

  size_t stride = mesh->format.stride;
  lovrGpuBindVertexBuffer(mesh->vbo);
  if (mesh->usage == USAGE_STREAM) {
    glBufferData(GL_ARRAY_BUFFER, mesh->count * stride, mesh->data.bytes, mesh->usage);
  } else {
    size_t offset = mesh->dirtyStart * stride;
    size_t count = (mesh->dirtyEnd - mesh->dirtyStart) * stride;
    glBufferSubData(GL_ARRAY_BUFFER, offset, count, mesh->data.bytes + offset);
  }

  mesh->dirtyStart = INT_MAX;
  mesh->dirtyEnd = 0;
}

IndexPointer lovrMeshReadIndices(Mesh* mesh, uint32_t* count, size_t* size) {
  *size = mesh->indexSize;
  *count = mesh->indexCount;

  if (mesh->indexCount == 0) {
    return (IndexPointer) { .raw = NULL };
  } else if (mesh->mappedIndices) {
    lovrMeshUnmapIndices(mesh);
  }

  return mesh->indices;
}

IndexPointer lovrMeshWriteIndices(Mesh* mesh, uint32_t count, size_t size) {
  if (mesh->mappedIndices) {
    lovrMeshUnmapIndices(mesh);
  }

  mesh->indexSize = size;
  mesh->indexCount = count;

  if (count == 0) {
    return (IndexPointer) { .raw = NULL };
  }

  lovrGpuBindVertexArray(mesh->vao);
  lovrGpuBindIndexBuffer(mesh->ibo);
  mesh->mappedIndices = true;

  if (mesh->indexCapacity < size * count) {
    mesh->indexCapacity = nextPo2(size * count);
    mesh->indices.raw = realloc(mesh->indices.raw, mesh->indexCapacity);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh->indexCapacity, NULL, mesh->usage);
  }

  return mesh->indices;
}

void lovrMeshUnmapIndices(Mesh* mesh) {
  if (!mesh->mappedIndices) {
    return;
  }

  mesh->mappedIndices = false;
  lovrGpuBindIndexBuffer(mesh->ibo);
  glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, mesh->indexCount * mesh->indexSize, mesh->indices.raw);
}

void lovrMeshResize(Mesh* mesh, uint32_t count) {
  if (mesh->count < count) {
    mesh->count = nextPo2(count);
    lovrGpuBindVertexBuffer(mesh->vbo);
    mesh->data.raw = realloc(mesh->data.raw, count * mesh->format.stride);
    glBufferData(GL_ARRAY_BUFFER, count * mesh->format.stride, mesh->data.raw, mesh->usage);
  }
}
