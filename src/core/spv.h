#include <stdint.h>

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
  SPV_MAT2,
  SPV_MAT3,
  SPV_MAT4
} spv_type;

typedef struct {
  const char* name;
  uint32_t id;
  spv_type type;
} spv_spec_constant;

typedef struct {
  const char* name;
  uint32_t offset;
  spv_type type;
} spv_push_constant;

typedef struct {
  const char* name;
  uint32_t location;
} spv_attribute;

typedef enum {
  SPV_UNIFORM_BUFFER,
  SPV_STORAGE_BUFFER,
  SPV_SAMPLED_TEXTURE,
  SPV_STORAGE_TEXTURE,
  SPV_SAMPLER
} spv_resource_type;

typedef struct {
  uint32_t set;
  uint32_t binding;
  const char* name;
  spv_resource_type type;
  uint32_t arraySize;
} spv_resource;

typedef struct {
  uint32_t version;
  uint32_t workgroupSize[3];
  uint32_t featureCount;
  uint32_t specConstantCount;
  uint32_t pushConstantCount;
  uint32_t pushConstantSize;
  uint32_t attributeCount;
  uint32_t resourceCount;
  uint32_t* features;
  spv_spec_constant* specConstants;
  spv_push_constant* pushConstants;
  spv_attribute* attributes;
  spv_resource* resources;
} spv_info;

typedef enum {
  SPV_OK,
  SPV_INVALID,
  SPV_TOO_BIG,
  SPV_UNSUPPORTED_IMAGE_TYPE,
  SPV_UNSUPPORTED_SPEC_CONSTANT_TYPE,
  SPV_UNSUPPORTED_PUSH_CONSTANT_TYPE
} spv_result;

spv_result spv_parse(const void* source, uint32_t size, spv_info* info);
const char* spv_result_to_string(spv_result);
