#include "graphics/shader.h"
#include "graphics/graphics.h"
#include "graphics/buffer.h"
#include "math/math.h"
#include "resources/shaders.h"
#include "core/hash.h"
#include "core/ref.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

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

Shader* lovrShaderInitDefault(Shader* shader, DefaultShader type, ShaderFlag* flags, uint32_t flagCount) {
  switch (type) {
    case SHADER_UNLIT: return lovrShaderInitGraphics(shader, NULL, NULL, flags, flagCount, true);
    case SHADER_STANDARD: return lovrShaderInitGraphics(shader, lovrStandardVertexShader, lovrStandardFragmentShader, flags, flagCount, true);
    case SHADER_CUBE: return lovrShaderInitGraphics(shader, lovrCubeVertexShader, lovrCubeFragmentShader, flags, flagCount, true);
    case SHADER_PANO: return lovrShaderInitGraphics(shader, lovrCubeVertexShader, lovrPanoFragmentShader, flags, flagCount, true);
    case SHADER_FONT: return lovrShaderInitGraphics(shader, NULL, lovrFontFragmentShader, flags, flagCount, true);
    case SHADER_FILL: return lovrShaderInitGraphics(shader, lovrFillVertexShader, NULL, flags, flagCount, true);
    default: lovrThrow("Unknown default shader type"); return NULL;
  }
}

ShaderType lovrShaderGetType(Shader* shader) {
  return shader->type;
}

int lovrShaderGetAttributeLocation(Shader* shader, const char* name) {
  uint64_t location = map_get(&shader->attributes, hash64(name, strlen(name)));
  return location == MAP_NIL ? -1 : (int) location;
}

bool lovrShaderHasUniform(Shader* shader, const char* name) {
  return map_get(&shader->uniformMap, hash64(name, strlen(name))) != MAP_NIL;
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
      align = 16 * (uniform->type == UNIFORM_MATRIX ? uniform->components : 1);
      uniform->size = align * uniform->count;
    } else {
      align = (uniform->components + (uniform->components == 3)) * 4;
      uniform->size = uniform->components * 4;
    }
    uniform->offset = (size + (align - 1)) & -align;
    size = uniform->offset + uniform->size;
  }
  return size;
}

ShaderBlock* lovrShaderBlockInit(ShaderBlock* block, BlockType type, Buffer* buffer, arr_uniform_t* uniforms) {
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

char* lovrShaderBlockGetShaderCode(ShaderBlock* block, const char* blockName, size_t* length) {

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
  size += 3; // "};\n"

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
  s += sprintf(s, "};\n");
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
