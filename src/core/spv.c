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
//   - Doesn't support stuff lovr doesn't care about like geometry/tessellation shaders

typedef union {
  struct {
    uint16_t location;
    uint16_t name;
  } attribute;
  struct {
    uint16_t decoration;
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
    union {
      uint16_t name;
      uint16_t arrayStride;
    };
  } type;
} spv_cache;

typedef struct {
  const uint32_t* words;
  const uint32_t* edge;
  size_t wordCount;
  uint32_t bound;
  spv_cache* cache;
  spv_field* fields;
} spv_context;

#define OP_CODE(op) (op[0] & 0xffff)
#define OP_LENGTH(op) (op[0] >> 16)

static spv_result spv_parse_capability(spv_context* spv, const uint32_t* op, spv_info* info);
static spv_result spv_parse_execution_mode(spv_context* spv, const uint32_t* op, spv_info* info);
static spv_result spv_parse_name(spv_context* spv, const uint32_t* op, spv_info* info);
static spv_result spv_parse_decoration(spv_context* spv, const uint32_t* op, spv_info* info);
static spv_result spv_parse_type(spv_context* spv, const uint32_t* op, spv_info* info);
static spv_result spv_parse_spec_constant(spv_context* spv, const uint32_t* op, spv_info* info);
static spv_result spv_parse_constant(spv_context* spv, const uint32_t* op, spv_info* info);
static spv_result spv_parse_variable(spv_context* spv, const uint32_t* op, spv_info* info);
static spv_result spv_parse_field(spv_context* spv, const uint32_t* op, spv_field* field, spv_info* info);
static bool spv_load_type(spv_context* spv, uint32_t id, const uint32_t** op);

spv_result spv_parse(const void* source, size_t size, spv_info* info) {
  spv_context spv;

  spv.words = source;
  spv.wordCount = size / sizeof(uint32_t);
  spv.edge = spv.words + spv.wordCount - 8;
  spv.fields = info->fields;

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
  info->attributeCount = 0;
  info->resourceCount = 0;
  info->fieldCount = 0;

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
      case 16: // OpExecutionMode
        result = spv_parse_execution_mode(&spv, op, info);
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
    case SPV_UNSUPPORTED_SPEC_CONSTANT_TYPE: return "This type of specialization constant is not supported";
    case SPV_UNSUPPORTED_DATA_TYPE: return "Struct fields must be square float matrices, float/int/uint vectors, 32 bit numbers, or bools";
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

static spv_result spv_parse_execution_mode(spv_context* spv, const uint32_t* op, spv_info* info) {
  if (OP_LENGTH(op) != 6 || op[2] != 17) { // LocalSize
    return SPV_OK;
  }

  info->workgroupSize[0] = op[3];
  info->workgroupSize[1] = op[4];
  info->workgroupSize[2] = op[5];
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
    case 1: spv->cache[id].flag.number = op[3]; break; // SpecID
    case 6: spv->cache[id].type.arrayStride = op[3]; break; // ArrayStride (overrides name)
    case 30: spv->cache[id].attribute.location = op[3]; break; // Location
    case 33:
    case 34:
     if (spv->cache[id].variable.decoration == 0xffff) {
       spv->cache[id].variable.decoration = op - spv->words;
     }
     break;
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
    } else {
      constant->name = NULL;
    }

    if (OP_CODE(op) == 50) { // OpSpecConstant
      uint32_t typeId = op[1];
      const uint32_t* type = spv->words + spv->cache[typeId].type.word;

      if (typeId > spv->bound || type > spv->edge) {
        return SPV_INVALID;
      }

      if ((type[0] & 0xffff) == 21 && type[2] == 32) { // OpTypeInt
        constant->type = type[3] == 0 ? SPV_U32 : SPV_I32;
      } else if ((type[0] & 0xffff) == 22 && type[2] == 32) { // OpTypeFloat
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

  // Unwrap the pointer
  const uint32_t* pointer;
  if (!spv_load_type(spv, pointerId, &pointer) || OP_CODE(pointer) != 32) { // OpTypePointer
    return SPV_INVALID;
  }

  // Load the type
  const uint32_t* type;
  uint32_t typeId = pointer[3];
  if (!spv_load_type(spv, typeId, &type)) {
    return SPV_INVALID;
  }

  if (storageClass == 9) { // PushConstant
    if (OP_CODE(type) != 30) { // OpTypeStruct
      return SPV_INVALID;
    }

    if (info->fields) {
      info->pushConstants = spv->fields++;
    }

    info->fieldCount++;
    return spv_parse_field(spv, type, info->pushConstants, info);
  }

  // Ignore output variables (storageClass 3) and anything without a set/binding decoration
  if (storageClass == 3 || spv->cache[variableId].variable.decoration == 0xffff) {
    return SPV_OK;
  }

  if (!info->resources) {
    info->resourceCount++;

    // If it's a buffer, be sure to count how many struct fields it has
    if (storageClass == 2 || storageClass == 12) {
      info->fieldCount++;
      return spv_parse_field(spv, type, NULL, info);
    } else {
      return SPV_OK;
    }
  }

  spv_resource* resource = &info->resources[info->resourceCount++];

  // Resolve the set/binding pointers.  The cache stores the index of the first set/binding
  // decoration word, we need to search for the "other" one.
  const uint32_t* word = spv->words + spv->cache[variableId].variable.decoration;
  bool set = word[2] == 34;
  uint32_t other = set ? 33 : 34;

  if (set) {
    resource->set = &word[3];
    resource->binding = NULL;
  } else {
    resource->set = NULL;
    resource->binding = &word[3];
  }

  for (;;) {
    const uint32_t* next = word + OP_LENGTH(word);

    if (word == next || next > spv->edge || (OP_CODE(next) != 71 && OP_CODE(next) != 72)) {
      break;
    }

    word = next;

    if (OP_CODE(word) == 71 && word[1] == variableId && word[2] == other) {
      if (set) {
        resource->binding = &word[3];
      } else {
        resource->set = &word[3];
      }
      break;
    }
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

    if (OP_CODE(type) != 30) { // OpTypeStruct
      return SPV_INVALID;
    }

    info->fieldCount++;
    resource->bufferFields = spv->fields++;
    resource->bufferFields[0].offset = 0;
    resource->bufferFields[0].name = resource->name;
    return spv_parse_field(spv, type, resource->bufferFields, info);
  } else {
    resource->bufferFields = NULL;
  }

  // Sampler and texture variables are named directly
  if (spv->cache[variableId].variable.name != 0xffff) {
    resource->name = (char*) (spv->words + spv->cache[variableId].variable.name);
  }

  if (OP_CODE(type) == 26) { // OpTypeSampler
    resource->type = SPV_SAMPLER;
    return SPV_OK;
  }

  resource->textureFlags = 0;

  // Unwrap combined image samplers
  if (OP_CODE(type) == 27) { // OpTypeSampledImage
    resource->type = SPV_COMBINED_TEXTURE_SAMPLER;
    if (!spv_load_type(spv, type[2], &type)) {
      return SPV_INVALID;
    }
  } else if (OP_CODE(type) != 25) { // OpTypeImage
    return SPV_INVALID;
  } else {
    // Texel buffers use the DimBuffer dimensionality
    if (type[3] == 5) {
      resource->dimension = SPV_TEXTURE_1D;
      switch (type[7]) {
        case 1: resource->type = SPV_UNIFORM_TEXEL_BUFFER; return SPV_OK;
        case 2: resource->type = SPV_STORAGE_TEXEL_BUFFER; return SPV_OK;
        default: return SPV_INVALID;
      }
    }

    // Input attachments use the DimSubpassData dimensionality, and "Sampled" must be 2
    if (type[3] == 6) {
      if (type[7] == 2) {
        resource->type = SPV_INPUT_ATTACHMENT;
        return SPV_OK;
      } else {
        return SPV_INVALID;
      }
    }

    // Read the Sampled key to determine if it's a sampled image (1) or a storage image (2)
    switch (type[7]) {
      case 1: resource->type = SPV_SAMPLED_TEXTURE; break;
      case 2: resource->type = SPV_STORAGE_TEXTURE; break;
      default: return SPV_INVALID;
    }
  }

  const uint32_t* texelType = NULL;
  if (!spv_load_type(spv, type[2], &texelType)) {
    return SPV_INVALID;
  }

  if (OP_CODE(texelType) == 21) { // OpTypeInt
    resource->textureFlags |= SPV_TEXTURE_INTEGER;
  }

  switch (type[3]) {
    case 0: resource->dimension = SPV_TEXTURE_1D; break;
    case 1: resource->dimension = SPV_TEXTURE_2D; break;
    case 2: resource->dimension = SPV_TEXTURE_3D; break;
    case 3: resource->dimension = SPV_TEXTURE_2D; resource->textureFlags |= SPV_TEXTURE_CUBE; break;
    case 4: resource->dimension = SPV_TEXTURE_2D; break;
    case 5: return SPV_INVALID; // Handled above
    case 6: return SPV_INVALID; // Handled above
    default: return SPV_INVALID;
  }

  if (type[4] == 1) {
    resource->textureFlags |= SPV_TEXTURE_SHADOW;
  }

  if (type[5] == 1) {
    resource->textureFlags |= SPV_TEXTURE_ARRAY;
  }

  if (type[6] == 1) {
    resource->textureFlags |= SPV_TEXTURE_MULTISAMPLE;
  }

  return SPV_OK;
}

static spv_result spv_parse_field(spv_context* spv, const uint32_t* word, spv_field* field, spv_info* info) {
  if (OP_CODE(word) == 28) { // OpTypeArray
    uint32_t lengthId = word[3];
    const uint32_t* lengthWord = spv->words + spv->cache[lengthId].constant.word;

    // Length must be an OpConstant or OpSpecConstant
    if (lengthId > spv->bound || lengthWord > spv->edge || (OP_CODE(lengthWord) != 43 && OP_CODE(lengthWord) != 50)) {
      return SPV_INVALID;
    }

    if (field) {
      field->arrayLength = lengthWord[3];
      field->arrayStride = spv->cache[word[1]].type.arrayStride;
    }

    // Unwrap inner array type and fall through
    if (!spv_load_type(spv, word[2], &word)) {
      return SPV_INVALID;
    }
  } else if (OP_CODE(word) == 29) { // OpTypeRuntimeArray
    if (field) {
      field->arrayLength = ~0u;
      field->arrayStride = spv->cache[word[1]].type.arrayStride;
    }

    // Unwrap inner array type and fall through
    if (!spv_load_type(spv, word[2], &word)) {
      return SPV_INVALID;
    }
  } else if (field) {
    field->arrayLength = 0;
    field->arrayStride = 0;
  }

  // If this is the initial scan, just do a recursive count of all the struct fields
  if (!field) {
    if (OP_CODE(word) == 30) { // OpTypeStruct
      uint32_t childCount = OP_LENGTH(word) - 2;

      const uint32_t* type;
      for (uint32_t i = 0; i < childCount; i++) {
        if (!spv_load_type(spv, word[i + 2], &type)) {
          return SPV_INVALID;
        }

        spv_result result = spv_parse_field(spv, type, NULL, info);

        if (result != SPV_OK) {
          return result;
        }
      }

      info->fieldCount += childCount;
    } else {
      info->fieldCount++;
    }
    return SPV_OK;
  }

  // If it's a struct, recursively parse each member
  if (OP_CODE(word) == 30) { // OpTypeStruct
    uint32_t childCount = OP_LENGTH(word) - 2;
    field->type = SPV_STRUCT;
    field->elementSize = 0;
    field->fieldCount = childCount;
    field->totalFieldCount = childCount;
    field->fields = spv->fields;
    info->fieldCount += childCount;
    spv->fields += childCount;

    for (uint32_t i = 0; i < childCount; i++) {
      field->fields[i].name = NULL;
      field->fields[i].offset = 0;

      const uint32_t* type;
      if (!spv_load_type(spv, word[i + 2], &type)) {
        return SPV_INVALID;
      }

      spv_result result = spv_parse_field(spv, type, &field->fields[i], info);

      if (result != SPV_OK) {
        return result;
      }

      field->totalFieldCount += field->fields[i].totalFieldCount;
    }

    // Collect member names and offsets.  Requires a scan over the name/decoration words, which is
    // kind of sad.  Trying to be fancy resulted in questionable increase in code/branching/storage.

    uint32_t structId = word[1];
    int32_t namesRemaining = childCount;
    int32_t offsetsRemaining = childCount;

    for (word = spv->words + 5; word < spv->edge && (namesRemaining > 0 || offsetsRemaining > 0); word += OP_LENGTH(word)) {
      if (OP_LENGTH(word) == 0 || word + OP_LENGTH(word) > spv->edge) {
        return SPV_INVALID;
      }

      if (OP_CODE(word) == 6 && OP_LENGTH(word) >= 4 && word[1] == structId && word[2] < childCount) {
        field->fields[word[2]].name = (char*) &word[3];
        namesRemaining--;
      } else if (OP_CODE(word) == 72 && OP_LENGTH(word) == 5 && word[1] == structId && word[2] < childCount && word[3] == 35) { // Offset
        spv_field* child = &field->fields[word[2]];
        uint32_t offset = word[4];
        child->offset = offset;
        offsetsRemaining--;

        // Struct size is maximum extent of any of its members
        uint32_t size = child->arrayLength > 0 ? child->arrayLength * child->arrayStride : child->elementSize;
        if (offset + size > field->elementSize) {
          field->elementSize = offset + size;
        }
      } else if (OP_CODE(word) == 59) { // OpVariable, can break
        break;
      }
    }

    return SPV_OK;
  } else {
    field->fieldCount = 0;
    field->totalFieldCount = 0;
    field->fields = NULL;
  }

  uint32_t columnCount = 1;
  uint32_t componentCount = 1;

  if (OP_CODE(word) == 24) { // OpTypeMatrix
    columnCount = word[3];
    if (!spv_load_type(spv, word[2], &word)) {
      return SPV_INVALID;
    }
  }

  if (OP_CODE(word) == 23) { // OpTypeVector
    componentCount = word[3];
    if (!spv_load_type(spv, word[2], &word)) {
      return SPV_INVALID;
    }
  }

  if (OP_CODE(word) == 22 && word[2] == 32) { // OpTypeFloat
    if (columnCount >= 2 && columnCount <= 4 && componentCount >= 2 && componentCount <= 4) {
      field->type = SPV_MAT2x2 + (columnCount - 2) * 3 + (componentCount - 2);
    } else if (columnCount == 1 && componentCount >= 2 && componentCount <= 4) {
      field->type = SPV_F32x2 + componentCount - 2;
    } else if (columnCount == 1 && componentCount == 1) {
      field->type = SPV_F32;
    } else {
      return SPV_UNSUPPORTED_DATA_TYPE;
    }
  } else if (OP_CODE(word) == 21 && word[2] == 32) { // OpTypeInteger
    if (word[3] > 0) { // signed
      if (columnCount == 1 && componentCount >= 2 && componentCount <= 4) {
        field->type = SPV_I32x2 + componentCount - 2;
      } else if (columnCount == 1 && componentCount == 1) {
        field->type = SPV_I32;
      } else {
        return SPV_UNSUPPORTED_DATA_TYPE;
      }
    } else {
      if (columnCount == 1 && componentCount >= 2 && componentCount <= 4) {
        field->type = SPV_U32x2 + componentCount - 2;
      } else if (columnCount == 1 && componentCount == 1) {
        field->type = SPV_U32;
      } else {
        return SPV_UNSUPPORTED_DATA_TYPE;
      }
    }
  } else if (OP_CODE(word) == 20 && columnCount == 1 && componentCount == 1) { // OpTypeBool
    field->type = SPV_B32;
  } else {
    return SPV_UNSUPPORTED_DATA_TYPE;
  }

  field->elementSize = 4 * componentCount * columnCount;

  return SPV_OK;
}

static bool spv_load_type(spv_context* spv, uint32_t id, const uint32_t** word) {
  if (id > spv->bound || spv->cache[id].type.word == 0xffff || spv->words + spv->cache[id].type.word > spv->edge) {
    return false;
  }

  *word = spv->words + spv->cache[id].type.word;

  return true;
}
