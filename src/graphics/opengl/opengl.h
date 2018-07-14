#include "graphics/shader.h"
#include "graphics/texture.h"
#include "graphics/canvas.h"
#include "graphics/mesh.h"
#include "util.h"
#include "lib/map/map.h"
#include <stdint.h>

#pragma once

#if EMSCRIPTEN
#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>
#else
#include "lib/glad/glad.h"
#endif

#define LOVR_SHADER_POSITION 0
#define LOVR_SHADER_NORMAL 1
#define LOVR_SHADER_TEX_COORD 2
#define LOVR_SHADER_VERTEX_COLOR 3
#define LOVR_SHADER_TANGENT 4
#define LOVR_SHADER_BONES 5
#define LOVR_SHADER_BONE_WEIGHTS 6
#define LOVR_MAX_UNIFORM_LENGTH 256
#define LOVR_MAX_ATTRIBUTE_LENGTH 256

typedef struct {
  GLchar name[LOVR_MAX_UNIFORM_LENGTH];
  GLenum glType;
  int index;
  int location;
  int count;
  int components;
  size_t size;
  UniformType type;
  union {
    void* data;
    int* ints;
    float* floats;
    Texture** textures;
  } value;
  int baseTextureSlot;
  bool dirty;
} Uniform;

typedef map_t(Uniform) map_uniform_t;

struct Shader {
  Ref ref;
  uint32_t program;
  map_uniform_t uniforms;
  map_int_t attributes;
};

struct Texture {
  Ref ref;
  TextureType type;
  GLenum glType;
  TextureData** slices;
  int width;
  int height;
  int depth;
  GLuint id;
  TextureFilter filter;
  TextureWrap wrap;
  bool srgb;
  bool mipmaps;
  bool allocated;
};

struct Canvas {
  Texture texture;
  GLuint framebuffer;
  GLuint resolveFramebuffer;
  GLuint depthStencilBuffer;
  GLuint msaaTexture;
  CanvasFlags flags;
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

GLenum lovrConvertWrapMode(WrapMode mode);
GLenum lovrConvertTextureFormat(TextureFormat format);
GLenum lovrConvertTextureFormatInternal(TextureFormat format, bool srgb);
GLenum lovrConvertMeshUsage(MeshUsage usage);
GLenum lovrConvertMeshDrawMode(MeshDrawMode mode);
bool lovrIsTextureFormatCompressed(TextureFormat format);
