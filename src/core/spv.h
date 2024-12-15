#include <stdint.h>
#include <stddef.h>

#pragma once

typedef enum {
  SPV_B32,
  SPV_I32,
  SPV_I32x2,
  SPV_I32x3,
  SPV_I32x4,
  SPV_U32,
  SPV_U32x2,
  SPV_U32x3,
  SPV_U32x4,
  SPV_F32,
  SPV_F32x2,
  SPV_F32x3,
  SPV_F32x4,
  SPV_MAT2x2,
  SPV_MAT2x3,
  SPV_MAT2x4,
  SPV_MAT3x2,
  SPV_MAT3x3,
  SPV_MAT3x4,
  SPV_MAT4x2,
  SPV_MAT4x3,
  SPV_MAT4x4,
  SPV_STRUCT
} spv_type;

typedef struct {
  const char* name;
  uint32_t id;
  spv_type type;
} spv_spec_constant;

typedef struct spv_field {
  const char* name;
  spv_type type;
  uint32_t offset;
  uint32_t arrayLength;
  uint32_t arrayStride;
  uint32_t elementSize;
  uint16_t fieldCount;
  uint16_t totalFieldCount;
  struct spv_field* fields;
} spv_field;

typedef struct {
  const char* name;
  uint32_t location;
} spv_attribute;

typedef enum {
  SPV_UNIFORM_BUFFER,
  SPV_STORAGE_BUFFER,
  SPV_SAMPLED_TEXTURE,
  SPV_STORAGE_TEXTURE,
  SPV_SAMPLER,
  SPV_COMBINED_TEXTURE_SAMPLER,
  SPV_UNIFORM_TEXEL_BUFFER,
  SPV_STORAGE_TEXEL_BUFFER,
  SPV_INPUT_ATTACHMENT
} spv_resource_type;

typedef enum {
  SPV_TEXTURE_1D,
  SPV_TEXTURE_2D,
  SPV_TEXTURE_3D
} spv_texture_dimension;

enum {
  SPV_TEXTURE_CUBE = (1 << 0),
  SPV_TEXTURE_ARRAY = (1 << 1),
  SPV_TEXTURE_SHADOW = (1 << 2),
  SPV_TEXTURE_MULTISAMPLE = (1 << 3),
  SPV_TEXTURE_INTEGER = (1 << 4)
};

typedef struct {
  const uint32_t* set;
  const uint32_t* binding;
  const char* name;
  uint32_t arraySize;
  spv_resource_type type;
  spv_texture_dimension dimension;
  uint16_t textureFlags;
  spv_field* bufferFields;
} spv_resource;

typedef struct {
  uint32_t version;
  uint32_t workgroupSize[3];
  uint32_t featureCount;
  uint32_t specConstantCount;
  uint32_t attributeCount;
  uint32_t resourceCount;
  uint32_t fieldCount;
  uint32_t* features;
  spv_spec_constant* specConstants;
  spv_field* pushConstants;
  spv_attribute* attributes;
  spv_resource* resources;
  spv_field* fields;
} spv_info;

typedef enum {
  SPV_OK,
  SPV_INVALID,
  SPV_TOO_BIG,
  SPV_UNSUPPORTED_SPEC_CONSTANT_TYPE,
  SPV_UNSUPPORTED_DATA_TYPE
} spv_result;

spv_result spv_parse(const void* source, size_t size, spv_info* info);
const char* spv_result_to_string(spv_result);
