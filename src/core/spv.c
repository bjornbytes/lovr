#include "spv.h"
#include <string.h>
#include <stdbool.h>

// Notes about this spirv parser:
// - It sucks!!!  Don't use it!
// - This is a smol version of spirv-reflect for lovr's purposes, it may even be good to switch to
//   spirv-reflect in the future if the amount of parsing required grows
// - Its job is to just tell you what's in the file, you have to decide if you consider it valid
// - Limitations:
//   - Max ID bound is 65534
//   - Doesn't support multiple entry points in one module (I think?)
//   - Input variables with a location > 31 are ignored since they don't fit in the 32-bit mask
//   - Max supported descriptor set or binding number is 255
//   - Doesn't parse stuff lovr doesn't care about: texel buffers, geometry/tessellation, etc. etc.

typedef union {
  struct {
    uint16_t location;
    uint16_t name;
  } attribute;
  struct {
    uint8_t set;
    uint8_t binding;
    uint16_t name;
  } variable;
  struct {
    uint16_t number;
    uint16_t name;
  } flag;
  struct {
    uint32_t word;
  } constant;
  struct {
    uint16_t word;
    uint16_t name;
  } type;
} spv_cache;

typedef struct {
  const uint32_t* words;
  const uint32_t* edge;
  uint32_t wordCount;
  uint32_t bound;
  spv_cache* cache;
} spv_context;

#define OP_CODE(op) (op[0] & 0xffff)
#define OP_LENGTH(op) (op[0] >> 16)

static spv_result spv_parse_capability(spv_context* spv, const uint32_t* op, spv_info* info);
static spv_result spv_parse_name(spv_context* spv, const uint32_t* op, spv_info* info);
static spv_result spv_parse_decoration(spv_context* spv, const uint32_t* op, spv_info* info);
static spv_result spv_parse_type(spv_context* spv, const uint32_t* op, spv_info* info);
static spv_result spv_parse_spec_constant(spv_context* spv, const uint32_t* op, spv_info* info);
static spv_result spv_parse_constant(spv_context* spv, const uint32_t* op, spv_info* info);
static spv_result spv_parse_variable(spv_context* spv, const uint32_t* op, spv_info* info);
static spv_result spv_parse_push_constants(spv_context* spv, const uint32_t* op, spv_info* info);
static bool spv_load_type(spv_context* spv, uint32_t id, const uint32_t** op);

spv_result spv_parse(const void* source, uint32_t size, spv_info* info) {
  spv_context spv;

  spv.words = source;
  spv.wordCount = size / sizeof(uint32_t);
  spv.edge = spv.words + spv.wordCount - 8;

  if (spv.wordCount < 16 || spv.words[0] != 0x07230203) {
    return SPV_INVALID;
  }

  info->version = spv.words[1];

  spv.bound = spv.words[3];

  if (spv.bound >= 8192) {
    return SPV_TOO_BIG;
  }

  spv_cache cache[8192];
  memset(cache, 0xff, spv.bound * sizeof(spv_cache));
  spv.cache = cache;

  info->featureCount = 0;
  info->specConstantCount = 0;
  info->pushConstantCount = 0;
  info->pushConstantSize = 0;
  info->attributeCount = 0;
  info->resourceCount = 0;

  const uint32_t* op = spv.words + 5;

  while (op < spv.words + spv.wordCount) {
    uint16_t opcode = OP_CODE(op);
    uint16_t length = OP_LENGTH(op);

    if (length == 0 || op + length > spv.words + spv.wordCount) {
      return SPV_INVALID;
    }

    spv_result result = SPV_OK;

    switch (opcode) {
      case 17: // OpCapability
        result = spv_parse_capability(&spv, op, info);
        break;
      case 5: // OpName
        result = spv_parse_name(&spv, op, info);
        break;
      case 71: // OpDecorate
        result = spv_parse_decoration(&spv, op, info);
        break;
      case 19: // OpTypeVoid
      case 20: // OpTypeBool
      case 21: // OpTypeInt
      case 22: // OpTypeFloat
      case 23: // OpTypeVector
      case 24: // OpTypeMatrix
      case 25: // OpTypeImage
      case 26: // OpTypeSampler
      case 27: // OpTypeSampledImage
      case 28: // OpTypeArray
      case 29: // OpTypeRuntimeArray
      case 30: // OpTypeStruct
      case 31: // OpTypeOpaque
      case 32: // OpTypePointer
        result = spv_parse_type(&spv, op, info);
        break;
      case 48: // OpSpecConstantTrue
      case 49: // OpSpecConstantFalse
      case 50: // OpSpecConstant
        result = spv_parse_spec_constant(&spv, op, info);
        break;
      case 43: // OpConstant
        result = spv_parse_constant(&spv, op, info);
        break;
      case 59: // OpVariable
        result = spv_parse_variable(&spv, op, info);
        break;
      case 54: // OpFunction
        op = spv.words + spv.wordCount; // Can exit early upon reaching actual code
        length = 0;
        break;
    }

    if (result != SPV_OK) {
      return result;
    }

    op += length;
  }

  return SPV_OK;
}

const char* spv_result_to_string(spv_result result) {
  switch (result) {
    case SPV_OK: return "OK";
    case SPV_INVALID: return "Invalid SPIR-V";
    case SPV_TOO_BIG: return "SPIR-V contains too many types/variables (max ID is 65534)";
    case SPV_UNSUPPORTED_IMAGE_TYPE: return "This type of image variable is not supported";
    case SPV_UNSUPPORTED_SPEC_CONSTANT_TYPE: return "This type of specialization constant is not supported";
    case SPV_UNSUPPORTED_PUSH_CONSTANT_TYPE: return "Push constants must be square matrices, vectors, 32 bit numbers, or bools";
    default: return NULL;
  }
}

static spv_result spv_parse_capability(spv_context* spv, const uint32_t* op, spv_info* info) {
  if (OP_LENGTH(op) != 2) {
    return SPV_INVALID;
  }

  if (info->features) {
    info->features[info->featureCount] = op[1];
  }

  info->featureCount++;

  return SPV_OK;
}

static spv_result spv_parse_name(spv_context* spv, const uint32_t* op, spv_info* info) {
  if (OP_LENGTH(op) < 3 || op[1] > spv->bound) {
    return SPV_INVALID;
  }

  // Track the word index of the name of the type with the given id
  spv->cache[op[1]].type.name = &op[2] - spv->words;

  return SPV_OK;
}

static spv_result spv_parse_decoration(spv_context* spv, const uint32_t* op, spv_info* info) {
  uint32_t id = op[1];
  uint32_t decoration = op[2];

  if (OP_LENGTH(op) < 3 || id > spv->bound) {
    return SPV_INVALID;
  }

  switch (decoration) {
    case 33: spv->cache[id].variable.binding = op[3]; break; // Binding
    case 34: spv->cache[id].variable.set = op[3]; break; // Set
    case 30: spv->cache[id].attribute.location = op[3]; break; // Location
    case 1: spv->cache[id].flag.number = op[3]; break; // SpecID
    default: break;
  }

  return SPV_OK;
}

static spv_result spv_parse_type(spv_context* spv, const uint32_t* op, spv_info* info) {
  if (OP_LENGTH(op) < 2 || op[1] > spv->bound) {
    return SPV_INVALID;
  }

  // Track the word index of the declaration of the type with the given ID
  spv->cache[op[1]].type.word = op - spv->words;

  return SPV_OK;
}

static spv_result spv_parse_spec_constant(spv_context* spv, const uint32_t* op, spv_info* info) {
  if (OP_LENGTH(op) < 2 || op[1] >= spv->bound) {
    return SPV_INVALID;
  }

  uint32_t id = op[2];

  if (spv->cache[id].flag.number == 0xffff) {
    return SPV_INVALID;
  }

  if (info->specConstants) {
    spv_spec_constant* constant = &info->specConstants[info->specConstantCount];
    constant->id = spv->cache[id].flag.number;

    if (spv->cache[id].flag.name != 0xffff) {
      constant->name = (char*) (spv->words + spv->cache[id].flag.name);
    }

    if (OP_CODE(op) == 50) { // OpSpecConstant
      uint32_t typeId = op[1];
      const uint32_t* type = spv->words + spv->cache[typeId].type.word;

      if (typeId > spv->bound || type > spv->edge) {
        return SPV_INVALID;
      }

      if ((type[0] & 0xffff) == 21 && type[2] == 32) { // OpTypeInt
        constant->type = type[3] == 0 ? SPV_U32 : SPV_I32;
      } else if ((type[0] & 0xffff) == 22 && type[3] == 32) { // OpTypeFloat
        constant->type = SPV_F32;
      } else {
        return SPV_UNSUPPORTED_SPEC_CONSTANT_TYPE;
      }
    } else { // It's OpSpecConstantTrue or OpSpecConstantFalse
      constant->type = SPV_B32;
    }
  }

  info->specConstantCount++;

  // Tricky thing: the cache currently contains {id,name} for the constant.  It gets replaced
  // with the index of this word as a u32 so that array types using the specialization constants for
  // their lengths can find this word to get the array length.
  spv->cache[id].constant.word = op - spv->words;

  return SPV_OK;
}

static spv_result spv_parse_constant(spv_context* spv, const uint32_t* op, spv_info* info) {
  if (OP_LENGTH(op) < 3 || op[2] > spv->bound) {
    return SPV_INVALID;
  }

  // Track the index of the word declaring the constant with this ID, so arrays can find it later
  spv->cache[op[2]].constant.word = op - spv->words;

  return SPV_OK;
}

static spv_result spv_parse_variable(spv_context* spv, const uint32_t* op, spv_info* info) {
  if (OP_LENGTH(op) < 4 || op[1] > spv->bound || op[2] > spv->bound) {
    return SPV_INVALID;
  }

  uint32_t pointerId = op[1];
  uint32_t variableId = op[2];
  uint32_t storageClass = op[3];

  if (storageClass == 1) { // Input
    if (spv->cache[variableId].attribute.location == 0xffff) {
      return SPV_OK; // Not all input variables are attributes
    }

    if (info->attributes) {
      spv_attribute* attribute = &info->attributes[info->attributeCount++];
      attribute->location = spv->cache[variableId].attribute.location;
      attribute->name = (char*) (spv->words + spv->cache[variableId].attribute.name);
      return SPV_OK;
    } else {
      info->attributeCount++;
      return SPV_OK;
    }
  }

  if (storageClass == 9) { // PushConstant
    return spv_parse_push_constants(spv, op, info);
  }

  uint32_t set = spv->cache[variableId].variable.set;
  uint32_t binding = spv->cache[variableId].variable.binding;

  // Ignore output variables (storageClass 3) and anything without a set/binding decoration
  if (storageClass == 3 || set == 0xff || binding == 0xff) {
    return SPV_OK;
  }

  if (!info->resources) {
    info->resourceCount++;
    return SPV_OK;
  }

  spv_resource* resource = &info->resources[info->resourceCount++];

  resource->set = set;
  resource->binding = binding;

  const uint32_t* pointer;
  if (!spv_load_type(spv, pointerId, &pointer)) {
    return SPV_INVALID;
  }

  const uint32_t* type;
  uint32_t typeId = pointer[3];
  if (!spv_load_type(spv, typeId, &type)) {
    return SPV_INVALID;
  }

  // If it's an array, read the array size and unwrap the inner type
  if (OP_CODE(type) == 28) { // OpTypeArray
    const uint32_t* array = type;
    uint32_t lengthId = array[3];
    typeId = array[2];

    if (!spv_load_type(spv, typeId, &type)) {
      return SPV_INVALID;
    }

    const uint32_t* length = spv->words + spv->cache[lengthId].constant.word;

    // Length must be an OpConstant or an OpSpecConstant
    if (lengthId > spv->bound || length > spv->edge || (OP_CODE(length) != 43 && OP_CODE(length) != 50)) {
      return SPV_INVALID;
    }

    resource->arraySize = length[3];
  } else {
    resource->arraySize = 0;
  }

  // Buffers
  if (storageClass == 2 || storageClass == 12) { // Uniform || StorageBuffer
    resource->type = storageClass == 2 ? SPV_UNIFORM_BUFFER : SPV_STORAGE_BUFFER;

    // Buffers name their structs instead of their variables
    if (spv->cache[typeId].type.name != 0xffff) {
      resource->name = (char*) (spv->words + spv->cache[typeId].type.name);
    }

    return SPV_OK;
  }

  // Sampler and texture variables are named directly
  if (spv->cache[variableId].variable.name != 0xffff) {
    resource->name = (char*) (spv->words + spv->cache[variableId].variable.name);
  }

  if (OP_CODE(type) == 26) { // OpTypeSampler
    resource->type = SPV_SAMPLER;
    return SPV_OK;
  }

  // Combined image samplers are currently not supported (ty webgpu)
  if (OP_CODE(type) == 27) { // OpTypeSampledImage
    return SPV_UNSUPPORTED_IMAGE_TYPE;
  } else if (OP_CODE(type) != 25) { // OpTypeImage
    return SPV_INVALID;
  }

  // Reject texel buffers (DimBuffer) and input attachments (DimSubpassData)
  if (type[3] == 5 || type[3] == 6) {
    return SPV_UNSUPPORTED_IMAGE_TYPE;
  }

  // Read the Sampled key to determine if it's a sampled image (1) or a storage image (2)
  switch (type[7]) {
    case 1: resource->type = SPV_SAMPLED_TEXTURE; return SPV_OK;
    case 2: resource->type = SPV_STORAGE_TEXTURE; return SPV_OK;
    default: return SPV_UNSUPPORTED_IMAGE_TYPE;
  }
}

static spv_result spv_parse_push_constants(spv_context* spv, const uint32_t* op, spv_info* info) {
  const uint32_t* pointer;
  if (!spv_load_type(spv, op[1], &pointer) || OP_CODE(pointer) != 32) {
    return SPV_INVALID;
  }

  const uint32_t* structure;
  if (!spv_load_type(spv, pointer[3], &structure) || OP_CODE(structure) != 30) {
    return SPV_INVALID;
  }

  uint32_t memberCount = OP_LENGTH(structure) - 2;

  if (!info->pushConstants) {
    info->pushConstantCount = memberCount;
    return SPV_OK;
  }

  const uint32_t* type;
  for (uint32_t i = 0; i < memberCount; i++) {
    if (!spv_load_type(spv, structure[i + 2], &type)) {
      return SPV_INVALID;
    }

    spv_push_constant* constant = &info->pushConstants[info->pushConstantCount++];

    uint32_t columnCount = 1;
    uint32_t componentCount = 1;

    if (OP_CODE(type) == 24) { // OpTypeMatrix
      columnCount = type[3];
      if (!spv_load_type(spv, type[2], &type)) {
        return SPV_INVALID;
      }
    }

    if (OP_CODE(type) == 23) { // OpTypeVector
      componentCount = type[3];
      if (!spv_load_type(spv, type[2], &type)) {
        return SPV_INVALID;
      }
    }

    if (OP_CODE(type) == 22 && type[2] == 32) { // OpTypeFloat
      if (columnCount > 2 && columnCount < 4 && componentCount == columnCount) {
        constant->type = SPV_MAT2 + columnCount - 2;
      } else if (columnCount == 1 && componentCount > 2 && componentCount < 4) {
        constant->type = SPV_F32x2 + columnCount - 2;
      } else if (columnCount == 1 && componentCount == 1) {
        constant->type = SPV_F32;
      } else {
        return SPV_UNSUPPORTED_PUSH_CONSTANT_TYPE;
      }
    } else if (OP_CODE(type) == 21 && type[2] == 32) { // OpTypeInteger
      if (type[3] > 0) { // signed
        if (columnCount == 1 && componentCount > 2 && componentCount < 4) {
          constant->type = SPV_I32x2 + componentCount - 2;
        } else if (columnCount == 1 && componentCount == 1) {
          constant->type = SPV_I32;
        } else {
          return SPV_UNSUPPORTED_PUSH_CONSTANT_TYPE;
        }
      } else {
        if (columnCount == 1 && componentCount > 2 && componentCount < 4) {
          constant->type = SPV_U32x2 + componentCount - 2;
        } else if (columnCount == 1 && componentCount == 1) {
          constant->type = SPV_U32;
        } else {
          return SPV_UNSUPPORTED_PUSH_CONSTANT_TYPE;
        }
      }
    } else if (OP_CODE(type) == 20 && columnCount == 1 && componentCount == 1) { // OpTypeBool
      constant->type = SPV_B32;
    } else {
      return SPV_UNSUPPORTED_PUSH_CONSTANT_TYPE;
    }
  }

  // Need to do a second pass to find the name and offset decorations, hard to cache them
  op = spv->words + 5;

  while (op < spv->words + spv->wordCount) {
    uint16_t opcode = OP_CODE(op);
    uint16_t length = OP_LENGTH(op);

    if (OP_LENGTH(op) == 0 || op + OP_LENGTH(op) > spv->words + spv->wordCount) {
      return SPV_INVALID;
    }

    switch (opcode) {
      case 6: // OpMemberName
        if (length < 4 || op[1] > spv->bound) {
          return SPV_INVALID;
        } else if (op[1] == structure[1] && op[2] < info->pushConstantCount) {
          info->pushConstants[op[2]].name = (char*) &op[3];
        }
        break;
      case 72: // OpMemberDecorate
        if (length < 5 || op[1] > spv->bound) {
          return SPV_INVALID;
        } else if (op[1] == structure[1] && op[2] < info->pushConstantCount && op[3] == 35) { // Offset
          info->pushConstants[op[2]].offset = op[4];
        }
        break;
      case 59: // OpVariable, can exit
        op = spv->words + spv->wordCount;
        length = 0;
        break;
    }

    op += length;
  }

  return SPV_OK;
}

static bool spv_load_type(spv_context* spv, uint32_t id, const uint32_t** word) {
  if (id > spv->bound || spv->cache[id].type.word == 0xffff || spv->words + spv->cache[id].type.word > spv->edge) {
    return false;
  }

  *word = spv->words + spv->cache[id].type.word;

  return true;
}
