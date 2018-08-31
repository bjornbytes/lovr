#include "graphics/texture.h"
#include "lib/map/map.h"
#include "lib/vec/vec.h"
#include <stdbool.h>

#pragma once

#define LOVR_MAX_UNIFORM_LENGTH 64
#define LOVR_MAX_ATTRIBUTE_LENGTH 64

typedef enum {
  USAGE_STATIC,
  USAGE_DYNAMIC,
  USAGE_STREAM
} BufferUsage;

typedef enum {
  ACCESS_READ,
  ACCESS_WRITE,
  ACCESS_READ_WRITE
} UniformAccess;

typedef enum {
  BLOCK_UNIFORM,
  BLOCK_STORAGE
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

typedef struct Shader Shader;
typedef struct ShaderBlock ShaderBlock;

typedef struct {
  vec_uniform_t uniforms;
  int slot;
  ShaderBlock* source;
  UniformAccess access;
} UniformBlock;

typedef vec_t(UniformBlock) vec_block_t;

Shader* lovrShaderCreateGraphics(const char* vertexSource, const char* fragmentSource);
Shader* lovrShaderCreateCompute(const char* source);
Shader* lovrShaderCreateDefault(DefaultShader type);
void lovrShaderDestroy(void* ref);
ShaderType lovrShaderGetType(Shader* shader);
void lovrShaderBind(Shader* shader);
int lovrShaderGetAttributeId(Shader* shader, const char* name);
bool lovrShaderHasUniform(Shader* shader, const char* name);
const Uniform* lovrShaderGetUniform(Shader* shader, const char* name);
void lovrShaderSetFloats(Shader* shader, const char* name, float* data, int start, int count);
void lovrShaderSetInts(Shader* shader, const char* name, int* data, int start, int count);
void lovrShaderSetMatrices(Shader* shader, const char* name, float* data, int start, int count);
void lovrShaderSetTextures(Shader* shader, const char* name, Texture** data, int start, int count);
void lovrShaderSetImages(Shader* shader, const char* name, Image* data, int start, int count);
void lovrShaderSetColor(Shader* shader, const char* name, Color color);
void lovrShaderSetBlock(Shader* shader, const char* name, ShaderBlock* block, UniformAccess access);

ShaderBlock* lovrShaderBlockCreate(vec_uniform_t* uniforms, BlockType type, BufferUsage usage);
void lovrShaderBlockDestroy(void* ref);
size_t lovrShaderBlockGetSize(ShaderBlock* block);
BlockType lovrShaderBlockGetType(ShaderBlock* block);
char* lovrShaderBlockGetShaderCode(ShaderBlock* block, const char* blockName, size_t* length);
const Uniform* lovrShaderBlockGetUniform(ShaderBlock* block, const char* name);
void* lovrShaderBlockMap(ShaderBlock* block);
void lovrShaderBlockUnmap(ShaderBlock* block);
