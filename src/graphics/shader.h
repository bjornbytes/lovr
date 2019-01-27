#include "graphics/buffer.h"
#include "graphics/texture.h"
#include "graphics/opengl.h"
#include "lib/map/map.h"
#include "lib/vec/vec.h"
#include <stdbool.h>

#pragma once

#define LOVR_MAX_UNIFORM_LENGTH 64
#define LOVR_MAX_ATTRIBUTE_LENGTH 64

typedef enum {
  ACCESS_READ,
  ACCESS_WRITE,
  ACCESS_READ_WRITE
} UniformAccess;

typedef enum {
  BLOCK_UNIFORM,
  BLOCK_COMPUTE
} BlockType;

typedef enum {
  UNIFORM_FLOAT,
  UNIFORM_MATRIX,
  UNIFORM_INT,
  UNIFORM_SAMPLER,
  UNIFORM_IMAGE
} UniformType;

typedef enum {
  SHADER_GRAPHICS,
  SHADER_COMPUTE
} ShaderType;

typedef enum {
  SHADER_DEFAULT,
  SHADER_CUBE,
  SHADER_PANO,
  SHADER_FONT,
  SHADER_FILL,
  MAX_DEFAULT_SHADERS
} DefaultShader;

typedef struct {
  Texture* texture;
  int slice;
  int mipmap;
  UniformAccess access;
} Image;

typedef struct {
  char name[LOVR_MAX_UNIFORM_LENGTH];
  UniformType type;
  int components;
  int count;
  int location;
  int offset;
  int size;
  union {
    void* data;
    char* bytes;
    int* ints;
    float* floats;
    Texture** textures;
    Image* images;
  } value;
  TextureType textureType;
  int baseSlot;
  bool image;
  bool dirty;
} Uniform;

typedef vec_t(Uniform) vec_uniform_t;

typedef struct {
  Ref ref;
  BlockType type;
  vec_uniform_t uniforms;
  map_int_t uniformMap;
  Buffer* buffer;
} ShaderBlock;

typedef struct {
  vec_uniform_t uniforms;
  UniformAccess access;
  Buffer* source;
  size_t offset;
  size_t size;
  int slot;
} UniformBlock;

typedef vec_t(UniformBlock) vec_block_t;

typedef struct {
  Ref ref;
  ShaderType type;
  vec_uniform_t uniforms;
  vec_block_t blocks[2];
  map_int_t attributes;
  map_int_t uniformMap;
  map_int_t blockMap;
  GPU_SHADER_FIELDS
} Shader;

// Shader

Shader* lovrShaderInitGraphics(Shader* shader, const char* vertexSource, const char* fragmentSource);
Shader* lovrShaderInitCompute(Shader* shader, const char* source);
Shader* lovrShaderInitDefault(Shader* shader, DefaultShader type);
#define lovrShaderCreateGraphics(...) lovrShaderInitGraphics(lovrAlloc(Shader), __VA_ARGS__)
#define lovrShaderCreateCompute(...) lovrShaderInitCompute(lovrAlloc(Shader), __VA_ARGS__)
#define lovrShaderCreateDefault(...) lovrShaderInitDefault(lovrAlloc(Shader), __VA_ARGS__)
void lovrShaderDestroy(void* ref);
ShaderType lovrShaderGetType(Shader* shader);
int lovrShaderGetAttributeLocation(Shader* shader, const char* name);
bool lovrShaderHasUniform(Shader* shader, const char* name);
const Uniform* lovrShaderGetUniform(Shader* shader, const char* name);
void lovrShaderSetFloats(Shader* shader, const char* name, float* data, int start, int count);
void lovrShaderSetInts(Shader* shader, const char* name, int* data, int start, int count);
void lovrShaderSetMatrices(Shader* shader, const char* name, float* data, int start, int count);
void lovrShaderSetTextures(Shader* shader, const char* name, Texture** data, int start, int count);
void lovrShaderSetImages(Shader* shader, const char* name, Image* data, int start, int count);
void lovrShaderSetColor(Shader* shader, const char* name, Color color);
void lovrShaderSetBlock(Shader* shader, const char* name, Buffer* buffer, size_t offset, size_t size, UniformAccess access);

// ShaderBlock

size_t lovrShaderComputeUniformLayout(vec_uniform_t* uniforms);

ShaderBlock* lovrShaderBlockInit(ShaderBlock* block, BlockType type, Buffer* buffer, vec_uniform_t* uniforms);
#define lovrShaderBlockCreate(...) lovrShaderBlockInit(lovrAlloc(ShaderBlock), __VA_ARGS__)
void lovrShaderBlockDestroy(void* ref);
BlockType lovrShaderBlockGetType(ShaderBlock* block);
char* lovrShaderBlockGetShaderCode(ShaderBlock* block, const char* blockName, size_t* length);
const Uniform* lovrShaderBlockGetUniform(ShaderBlock* block, const char* name);
Buffer* lovrShaderBlockGetBuffer(ShaderBlock* block);
