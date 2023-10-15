#include "api.h"
#include "graphics/graphics.h"
#include "data/blob.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>

static const uint32_t typeComponents[] = {
  [TYPE_I8x4] = 4,
  [TYPE_U8x4] = 4,
  [TYPE_SN8x4] = 4,
  [TYPE_UN8x4] = 4,
  [TYPE_UN10x3] = 3,
  [TYPE_I16] = 1,
  [TYPE_I16x2] = 2,
  [TYPE_I16x4] = 4,
  [TYPE_U16] = 1,
  [TYPE_U16x2] = 2,
  [TYPE_U16x4] = 4,
  [TYPE_SN16x2] = 2,
  [TYPE_SN16x4] = 4,
  [TYPE_UN16x2] = 2,
  [TYPE_UN16x4] = 4,
  [TYPE_I32] = 1,
  [TYPE_I32x2] = 2,
  [TYPE_I32x3] = 3,
  [TYPE_I32x4] = 4,
  [TYPE_U32] = 1,
  [TYPE_U32x2] = 2,
  [TYPE_U32x3] = 3,
  [TYPE_U32x4] = 4,
  [TYPE_F16x2] = 2,
  [TYPE_F16x4] = 4,
  [TYPE_F32] = 1,
  [TYPE_F32x2] = 2,
  [TYPE_F32x3] = 3,
  [TYPE_F32x4] = 4,
  [TYPE_MAT2] = 4,
  [TYPE_MAT3] = 9,
  [TYPE_MAT4] = 16,
  [TYPE_INDEX16] = 1,
  [TYPE_INDEX32] = 1
};

static const uint32_t vectorComponents[] = {
  [V_VEC2] = 2,
  [V_VEC3] = 3,
  [V_VEC4] = 4,
  [V_QUAT] = 4,
  [V_MAT4] = 16
};

typedef union {
  void* raw;
  int8_t* i8;
  uint8_t* u8;
  int16_t* i16;
  uint16_t* u16;
  int32_t* i32;
  uint32_t* u32;
  float* f32;
} DataPointer;

void luax_checkfieldn(lua_State* L, int index, int type, void* data) {
  DataPointer p = { .raw = data };
  for (uint32_t i = 0; i < typeComponents[type]; i++) {
    double x = lua_tonumber(L, index + i);
    switch (type) {
      case TYPE_I8x4: p.i8[i] = (int8_t) x; break;
      case TYPE_U8x4: p.u8[i] = (uint8_t) x; break;
      case TYPE_SN8x4: p.i8[i] = (int8_t) CLAMP(x, -1.f, 1.f) * INT8_MAX; break;
      case TYPE_UN8x4: p.u8[i] = (uint8_t) CLAMP(x, 0.f, 1.f) * UINT8_MAX; break;
      case TYPE_UN10x3: p.u32[0] |= (uint32_t) (CLAMP(x, 0.f, 1.f) * 1023.f) << (10 * (2 - i)); break;
      case TYPE_I16: p.i16[i] = (int16_t) x; break;
      case TYPE_I16x2: p.i16[i] = (int16_t) x; break;
      case TYPE_I16x4: p.i16[i] = (int16_t) x; break;
      case TYPE_U16: p.u16[i] = (uint16_t) x; break;
      case TYPE_U16x2: p.u16[i] = (uint16_t) x; break;
      case TYPE_U16x4: p.u16[i] = (uint16_t) x; break;
      case TYPE_SN16x2: p.i16[i] = (int16_t) CLAMP(x, -1.f, 1.f) * INT16_MAX; break;
      case TYPE_SN16x4: p.i16[i] = (int16_t) CLAMP(x, -1.f, 1.f) * INT16_MAX; break;
      case TYPE_UN16x2: p.u16[i] = (uint16_t) CLAMP(x, 0.f, 1.f) * UINT16_MAX; break;
      case TYPE_UN16x4: p.u16[i] = (uint16_t) CLAMP(x, 0.f, 1.f) * UINT16_MAX; break;
      case TYPE_I32: p.i32[i] = (int32_t) x; break;
      case TYPE_I32x2: p.i32[i] = (int32_t) x; break;
      case TYPE_I32x3: p.i32[i] = (int32_t) x; break;
      case TYPE_I32x4: p.i32[i] = (int32_t) x; break;
      case TYPE_U32: p.u32[i] = (uint32_t) x; break;
      case TYPE_U32x2: p.u32[i] = (uint32_t) x; break;
      case TYPE_U32x3: p.u32[i] = (uint32_t) x; break;
      case TYPE_U32x4: p.i32[i] = (uint32_t) x; break;
      case TYPE_F16x2: p.u16[i] = float32to16(x); break;
      case TYPE_F16x4: p.u16[i] = float32to16(x); break;
      case TYPE_F32: p.f32[i] = (float) x; break;
      case TYPE_F32x2: p.f32[i] = (float) x; break;
      case TYPE_F32x3: p.f32[i] = (float) x; break;
      case TYPE_F32x4: p.f32[i] = (float) x; break;
      case TYPE_MAT2: p.f32[i] = (float) x; break;
      case TYPE_MAT3: p.f32[4 * i / 3 + i % 3] = (float) x; break;
      case TYPE_MAT4: p.f32[i] = (float) x; break;
      case TYPE_INDEX16: p.u16[i] = (uint16_t) x - 1; break;
      case TYPE_INDEX32: p.u32[i] = (uint32_t) x - 1; break;
      default: lovrUnreachable();
    }
  }
}

void luax_checkfieldv(lua_State* L, int index, int type, void* data) {
  DataPointer p = { .raw = data };
  uint32_t n = typeComponents[type];
  lovrCheck(n > 1, "Expected number for scalar data type, got vector");
  VectorType vectorType;
  float* v = luax_tovector(L, index, &vectorType);
  lovrCheck(v, "Expected vector, got non-vector userdata");
  if (n >= TYPE_MAT2 && n <= TYPE_MAT4) {
    lovrCheck(vectorType == V_MAT4, "Tried to send a non-matrix to a matrix type");
  } else {
    lovrCheck(vectorComponents[vectorType] == n, "Expected %d vector components, got %d", n, vectorComponents[vectorType]);
  }
  switch (type) {
    case TYPE_I8x4: for (int i = 0; i < 4; i++) p.i8[i] = (int8_t) v[i]; break;
    case TYPE_U8x4: for (int i = 0; i < 4; i++) p.u8[i] = (uint8_t) v[i]; break;
    case TYPE_SN8x4: for (int i = 0; i < 4; i++) p.i8[i] = (int8_t) CLAMP(v[i], -1.f, 1.f) * INT8_MAX; break;
    case TYPE_UN8x4: for (int i = 0; i < 4; i++) p.u8[i] = (uint8_t) CLAMP(v[i], 0.f, 1.f) * UINT8_MAX; break;
    case TYPE_UN10x3: for (int i = 0; i < 3; i++) p.u32[0] |= (uint32_t) (CLAMP(v[i], 0.f, 1.f) * 1023.f) << (10 * (2 - i)); break;
    case TYPE_I16x2: for (int i = 0; i < 2; i++) p.i16[i] = (int16_t) v[i]; break;
    case TYPE_I16x4: for (int i = 0; i < 4; i++) p.i16[i] = (int16_t) v[i]; break;
    case TYPE_U16x2: for (int i = 0; i < 2; i++) p.u16[i] = (uint16_t) v[i]; break;
    case TYPE_U16x4: for (int i = 0; i < 4; i++) p.u16[i] = (uint16_t) v[i]; break;
    case TYPE_SN16x2: for (int i = 0; i < 2; i++) p.i16[i] = (int16_t) CLAMP(v[i], -1.f, 1.f) * INT16_MAX; break;
    case TYPE_SN16x4: for (int i = 0; i < 4; i++) p.i16[i] = (int16_t) CLAMP(v[i], -1.f, 1.f) * INT16_MAX; break;
    case TYPE_UN16x2: for (int i = 0; i < 2; i++) p.u16[i] = (uint16_t) CLAMP(v[i], 0.f, 1.f) * UINT16_MAX; break;
    case TYPE_UN16x4: for (int i = 0; i < 4; i++) p.u16[i] = (uint16_t) CLAMP(v[i], 0.f, 1.f) * UINT16_MAX; break;
    case TYPE_I32x2: for (int i = 0; i < 2; i++) p.i32[i] = (int32_t) v[i]; break;
    case TYPE_I32x3: for (int i = 0; i < 3; i++) p.i32[i] = (int32_t) v[i]; break;
    case TYPE_I32x4: for (int i = 0; i < 4; i++) p.i32[i] = (int32_t) v[i]; break;
    case TYPE_U32x2: for (int i = 0; i < 2; i++) p.u32[i] = (uint32_t) v[i]; break;
    case TYPE_U32x3: for (int i = 0; i < 3; i++) p.u32[i] = (uint32_t) v[i]; break;
    case TYPE_U32x4: for (int i = 0; i < 4; i++) p.u32[i] = (uint32_t) v[i]; break;
    case TYPE_F16x2: for (int i = 0; i < 2; i++) p.u16[i] = float32to16(v[i]); break;
    case TYPE_F16x4: for (int i = 0; i < 4; i++) p.u16[i] = float32to16(v[i]); break;
    case TYPE_F32x2: memcpy(data, v, 2 * sizeof(float)); break;
    case TYPE_F32x3: memcpy(data, v, 3 * sizeof(float)); break;
    case TYPE_F32x4: memcpy(data, v, 4 * sizeof(float)); break;
    case TYPE_MAT2: for (int i = 0; i < 2; i++) memcpy(p.f32 + 2 * i, v + 4 * i, 2 * sizeof(float)); break;
    case TYPE_MAT3: for (int i = 0; i < 3; i++) memcpy(p.f32 + 4 * i, v + 4 * i, 3 * sizeof(float)); break;
    case TYPE_MAT4: memcpy(data, v, 16 * sizeof(float)); break;
    default: lovrUnreachable();
  }
}

void luax_checkfieldt(lua_State* L, int index, int type, void* data) {
  if (index < 0) index += lua_gettop(L) + 1;
  int n = typeComponents[type];
  for (int i = 0; i < n; i++) {
    lua_rawgeti(L, index, i + 1);
  }
  luax_checkfieldn(L, -n, type, data);
  lua_pop(L, n);
}

uint32_t luax_checkfieldarray(lua_State* L, int index, const DataField* array, char* data) {
  int components = typeComponents[array->type];
  uint32_t length = luax_len(L, index);

  if (components == 1) {
    uint32_t count = MIN(length, array->length);
    for (uint32_t i = 0; i < count; i++, data += array->stride) {
      lua_rawgeti(L, index, i + 1);
      luax_checkfieldn(L, -1, array->type, data);
      lua_pop(L, 1);
    }
    return count;
  }

  lua_rawgeti(L, index, 1);
  int innerType = lua_type(L, -1);
  lua_pop(L, 1);

  uint32_t count;

  switch (innerType) {
    case LUA_TNUMBER:
      if (index < 0) index += lua_gettop(L) + 1;
      count = MIN(array->length, length / components);
      lovrCheck(length % components == 0, "Table length for key '%s' must be divisible by %d", array->name, components);
      for (uint32_t i = 0; i < count; i++, data += array->stride) {
        for (int j = 1; j <= components; j++) {
          lua_rawgeti(L, index, i * components + j);
        }
        luax_checkfieldn(L, -components, array->type, data);
        lua_pop(L, components);
      }
      break;
    case LUA_TUSERDATA:
    case LUA_TLIGHTUSERDATA:
      count = MIN(array->length, length);
      for (uint32_t i = 0; i < count; i++, data += array->stride) {
        lua_rawgeti(L, index, i + 1);
        luax_checkfieldv(L, -1, array->type, data);
        lua_pop(L, 1);
      }
      break;
    case LUA_TTABLE:
      count = MIN(array->length, length);
      for (uint32_t i = 0; i < count; i++, data += array->stride) {
        lua_rawgeti(L, index, i + 1);
        luax_checkfieldt(L, -1, array->type, data);
        lua_pop(L, 1);
      }
      break;
    case LUA_TNIL:
      count = 0;
      break;
  }

  return count;
}

void luax_checkdataflat(lua_State* L, int index, int subindex, uint32_t count, const DataField* format, char* data) {
  for (uint32_t i = 0; i < count; i++, data += format->stride) {
    for (uint32_t f = 0; f < format->fieldCount; f++) {
      int n = 1;
      lua_rawgeti(L, index, subindex++);
      const DataField* field = &format->fields[f];
      if (lua_isuserdata(L, -1)) {
        luax_checkfieldv(L, -1, field->type, data + field->offset);
      } else {
        n = typeComponents[field->type];
        for (int c = 1; c < n; c++) {
          lua_rawgeti(L, index, subindex++);
        }
        luax_checkfieldn(L, -n, field->type, data + field->offset);
      }
      lua_pop(L, n);
    }
  }
}

void luax_checkdatatuples(lua_State* L, int index, int start, uint32_t count, const DataField* format, char* data) {
  for (uint32_t i = 0; i < count; i++, data += format->stride) {
    lua_rawgeti(L, index, start + i);
    lovrCheck(lua_type(L, -1) == LUA_TTABLE, "Expected table of tables");

    for (uint32_t f = 0, subindex = 1; f < format->fieldCount; f++) {
      int n = 1;
      lua_rawgeti(L, -1, subindex);
      const DataField* field = &format->fields[f];
      if (lua_isuserdata(L, -1)) {
        luax_checkfieldv(L, -1, field->type, data + field->offset);
      } else {
        while (n < (int) typeComponents[field->type]) {
          lua_rawgeti(L, -n - 1, subindex + n);
          n++;
        }
        luax_checkfieldn(L, -n, field->type, data + field->offset);
      }
      subindex += n;
      lua_pop(L, n);
    }

    lua_pop(L, 1);
  }
}

void luax_checkdatakeys(lua_State* L, int index, int start, uint32_t count, const DataField* array, char* data) {
  for (uint32_t i = 0; i < count; i++, data += array->stride) {
    lua_rawgeti(L, index, start + i);
    lovrCheck(lua_istable(L, -1), "Expected table of tables");
    luax_checkstruct(L, -1, array->fields, array->fieldCount, data);
    lua_pop(L, 1);
  }
}

void luax_checkstruct(lua_State* L, int index, const DataField* fields, uint32_t fieldCount, char* data) {
  for (uint32_t f = 0; f < fieldCount; f++) {
    const DataField* field = &fields[f];
    int n = field->fieldCount == 0 ? typeComponents[field->type] : 0;
    lua_getfield(L, index, field->name);

    if (lua_isnil(L, -1)) {
      memset(data + field->offset, 0, MAX(field->length, 1) * field->stride);
      lua_pop(L, 1);
      continue;
    }

    if (field->length > 0) {
      lovrCheck(lua_istable(L, -1), "Expected table for key '%s'", field->name);
      uint32_t count;

      if (field->fieldCount > 0) {
        uint32_t tableLength = luax_len(L, -1);
        count = MIN(field->length, tableLength);
        luax_checkdatakeys(L, -1, 1, count, field, data + field->offset);
      } else {
        count = luax_checkfieldarray(L, -1, field, data + field->offset);
      }

      if (count < field->length) {
        memset(data + field->offset + count * field->stride, 0, (field->length - count) * field->stride);
      }
    } else if (field->fieldCount > 0) {
      lovrCheck(lua_istable(L, -1), "Expected table for key '%s'", field->name);
      luax_checkstruct(L, -1, field->fields, field->fieldCount, data + field->offset);
    } else if (n == 1) {
      lovrCheck(lua_type(L, -1) == LUA_TNUMBER, "Expected number for key '%s'", field->name);
      luax_checkfieldn(L, -1, field->type, data + field->offset);
    } else if (lua_isuserdata(L, -1)) {
      luax_checkfieldv(L, -1, field->type, data + field->offset);
    } else if (lua_istable(L, -1)) {
      lovrCheck(luax_len(L, -1) == n, "Table length for key '%s' must be %d", field->name, n);
      luax_checkfieldt(L, -1, field->type, data + field->offset);
    } else {
      lovrThrow("Expected table or vector for key '%s'", field->name);
    }

    lua_pop(L, 1);
  }
}

static int luax_pushcomponents(lua_State* L, DataType type, char* data) {
  DataPointer p = { .raw = data };
  switch (type) {
    case TYPE_I8x4: for (int i = 0; i < 4; i++) lua_pushinteger(L, p.i8[i]); return 4;
    case TYPE_U8x4: for (int i = 0; i < 4; i++) lua_pushinteger(L, p.u8[i]); return 4;
    case TYPE_SN8x4: for (int i = 0; i < 4; i++) lua_pushnumber(L, MAX((float) p.i8[i] / 127, -1.f)); return 4;
    case TYPE_UN8x4: for (int i = 0; i < 4; i++) lua_pushnumber(L, (float) p.u8[i] / 255); return 4;
    case TYPE_UN10x3: for (int i = 0; i < 3; i++) lua_pushnumber(L, (float) ((p.u32[0] >> (10 * (2 - i))) & 0x3ff) / 1023.f); return 3;
    case TYPE_I16x2: for (int i = 0; i < 2; i++) lua_pushinteger(L, p.i16[i]); return 2;
    case TYPE_I16x4: for (int i = 0; i < 4; i++) lua_pushinteger(L, p.i16[i]); return 4;
    case TYPE_U16x2: for (int i = 0; i < 2; i++) lua_pushinteger(L, p.u16[i]); return 2;
    case TYPE_U16x4: for (int i = 0; i < 4; i++) lua_pushinteger(L, p.u16[i]); return 4;
    case TYPE_SN16x2: for (int i = 0; i < 2; i++) lua_pushnumber(L, MAX((float) p.i16[i] / 32767, -1.f)); return 2;
    case TYPE_SN16x4: for (int i = 0; i < 4; i++) lua_pushnumber(L, MAX((float) p.i16[i] / 32767, -1.f)); return 4;
    case TYPE_UN16x2: for (int i = 0; i < 2; i++) lua_pushnumber(L, (float) p.u16[i] / 65535); return 2;
    case TYPE_UN16x4: for (int i = 0; i < 4; i++) lua_pushnumber(L, (float) p.u16[i] / 65535); return 4;
    case TYPE_I32: lua_pushinteger(L, p.i32[0]); return 1;
    case TYPE_I32x2: for (int i = 0; i < 2; i++) lua_pushinteger(L, p.i32[i]); return 2;
    case TYPE_I32x3: for (int i = 0; i < 3; i++) lua_pushinteger(L, p.i32[i]); return 3;
    case TYPE_I32x4: for (int i = 0; i < 4; i++) lua_pushinteger(L, p.i32[i]); return 4;
    case TYPE_U32: lua_pushinteger(L, p.u32[0]); return 1;
    case TYPE_U32x2: for (int i = 0; i < 2; i++) lua_pushinteger(L, p.u32[i]); return 2;
    case TYPE_U32x3: for (int i = 0; i < 3; i++) lua_pushinteger(L, p.u32[i]); return 3;
    case TYPE_U32x4: for (int i = 0; i < 4; i++) lua_pushinteger(L, p.u32[i]); return 4;
    case TYPE_F16x2: for (int i = 0; i < 2; i++) lua_pushnumber(L, float16to32(p.u16[i])); return 2;
    case TYPE_F16x4: for (int i = 0; i < 4; i++) lua_pushnumber(L, float16to32(p.u16[i])); return 4;
    case TYPE_F32: lua_pushnumber(L, p.f32[0]); return 1;
    case TYPE_F32x2: for (int i = 0; i < 2; i++) lua_pushnumber(L, p.f32[i]); return 2;
    case TYPE_F32x3: for (int i = 0; i < 3; i++) lua_pushnumber(L, p.f32[i]); return 3;
    case TYPE_F32x4: for (int i = 0; i < 4; i++) lua_pushnumber(L, p.f32[i]); return 4;
    case TYPE_MAT2: for (int i = 0; i < 4; i++) lua_pushnumber(L, p.f32[i]); return 4;
    case TYPE_MAT3: for (int i = 0; i < 9; i++) lua_pushnumber(L, p.f32[4 * i / 3 + i % 3]); return 9;
    case TYPE_MAT4: for (int i = 0; i < 16; i++) lua_pushnumber(L, p.f32[i]); return 16;
    case TYPE_INDEX16: lua_pushinteger(L, p.u16[0] + 1); return 1;
    case TYPE_INDEX32: lua_pushinteger(L, p.u32[0] + 1); return 1;
    default: lovrUnreachable(); return 0;
  }
}

static int luax_pushstruct(lua_State* L, const DataField* fields, uint32_t count, char* data) {
  lua_createtable(L, 0, count);
  for (uint32_t i = 0; i < count; i++) {
    const DataField* field = &fields[i];
    if (field->length > 0) {
      if (field->fieldCount > 0) {
        lua_createtable(L, field->length, 0);
        for (uint32_t j = 0; j < field->length; j++) {
          luax_pushstruct(L, field->fields, field->fieldCount, data + field->offset + j * field->stride);
          lua_rawseti(L, -2, j + 1);
        }
      } else {
        DataType type = field->type;
        uint32_t n = typeComponents[field->type];
        lua_createtable(L, (int) (field->length * n), 0);
        for (uint32_t j = 0, k = 1; j < field->length; j++, k += n) {
          luax_pushcomponents(L, type, data + field->offset + j * field->stride);
          for (uint32_t c = 0; c < n; c++) {
            lua_rawseti(L, -1 - n + c, k + n - 1 - c);
          }
        }
      }
    } else if (field->fieldCount > 0) {
      luax_pushstruct(L, field->fields, field->fieldCount, data + field->offset);
    } else {
      uint32_t n = typeComponents[field->type];
      if (n > 1) {
        lua_createtable(L, n, 0);
        luax_pushcomponents(L, field->type, data + field->offset);
        for (uint32_t c = 0; c < n; c++) {
          lua_rawseti(L, -1 - n + c, n - c);
        }
      } else {
        luax_pushcomponents(L, field->type, data + field->offset);
      }
    }
    lua_setfield(L, -2, field->name);
  }
  return 1;
}

int luax_pushbufferdata(lua_State* L, const DataField* format, uint32_t count, char* data) {
  lua_createtable(L, count, 0);

  bool nested = false;
  for (uint32_t i = 0; i < format->fieldCount; i++) {
    if (format->fields[i].fields || format->fields[i].length > 0) {
      nested = true;
      break;
    }
  }

  if (format->fieldCount > 1 || typeComponents[format->fields[0].type] > 1 || nested) {
    if (nested) {
      for (uint32_t i = 0; i < count; i++) {
        luax_pushstruct(L, format->fields, format->fieldCount, data);
        lua_rawseti(L, -2, i + 1);
        data += format->stride;
      }
    } else {
      for (uint32_t i = 0; i < count; i++, data += format->stride) {
        lua_newtable(L);
        int j = 1;
        for (uint32_t f = 0; f < format->fieldCount; f++) {
          const DataField* field = &format->fields[f];
          int n = luax_pushcomponents(L, field->type, data + field->offset);
          for (int c = 0; c < n; c++) {
            lua_rawseti(L, -1 - n + c, j + n - 1 - c);
          }
          j += n;
        }
        lua_rawseti(L, -2, i + 1);
      }
    }
  } else {
    for (uint32_t i = 0; i < count; i++, data += format->stride) {
      luax_pushcomponents(L, format->fields[0].type, data + format->fields[0].offset);
      lua_rawseti(L, -2, i + 1);
    }
  }

  return 1;
}

uint32_t luax_gettablestride(lua_State* L, int index, int subindex, DataField* fields, uint32_t count) {
  int stride = 0;
  for (uint32_t i = 0; i < count; i++) {
    lovrCheck(!fields[i].fields && fields[i].length == 0, "This Buffer's format requires data to be given as a table of tables");
    lua_rawgeti(L, index, subindex + stride);
    switch (lua_type(L, -1)) {
      case LUA_TUSERDATA: case LUA_TLIGHTUSERDATA: stride++; break;
      case LUA_TNUMBER: stride += typeComponents[fields[i].type]; break;
      case LUA_TNIL: lovrThrow("Table does not have enough elements for a single element");
      default: lovrThrow("Expected table of numbers and/or vectors");
    }
    lua_pop(L, 1);
  }
  return (uint32_t) stride;
}

static int l_lovrBufferGetSize(lua_State* L) {
  Buffer* buffer = luax_checktype(L, 1, Buffer);
  const BufferInfo* info = lovrBufferGetInfo(buffer);
  lua_pushinteger(L, info->size);
  return 1;
}

static int l_lovrBufferGetLength(lua_State* L) {
  Buffer* buffer = luax_checktype(L, 1, Buffer);
  const DataField* format = lovrBufferGetInfo(buffer)->format;
  if (format) {
    lua_pushinteger(L, format->length);
  } else {
    lua_pushnil(L);
  }
  return 1;
}

static int l_lovrBufferGetStride(lua_State* L) {
  Buffer* buffer = luax_checktype(L, 1, Buffer);
  const DataField* format = lovrBufferGetInfo(buffer)->format;
  if (format) {
    lua_pushinteger(L, format->stride);
  } else {
    lua_pushnil(L);
  }
  return 1;
}

void luax_pushbufferformat(lua_State* L, const DataField* fields, uint32_t count) {
  lua_createtable(L, count, 0);
  for (uint32_t i = 0; i < count; i++) {
    const DataField* field = &fields[i];
    lua_newtable(L);
    lua_pushstring(L, field->name);
    lua_setfield(L, -2, "name");
    if (field->fieldCount > 0) {
      luax_pushbufferformat(L, field->fields, field->fieldCount);
    } else {
      luax_pushenum(L, DataType, field->type);
    }
    lua_setfield(L, -2, "type");
    lua_pushinteger(L, field->offset);
    lua_setfield(L, -2, "offset");
    if (field->length > 0) {
      lua_pushinteger(L, field->length);
      lua_setfield(L, -2, "length");
      lua_pushinteger(L, field->stride);
      lua_setfield(L, -2, "stride");
    }
    lua_rawseti(L, -2, i + 1);
  }
}

static int l_lovrBufferGetFormat(lua_State* L) {
  Buffer* buffer = luax_checktype(L, 1, Buffer);
  const DataField* format = lovrBufferGetInfo(buffer)->format;
  if (format) {
    luax_pushbufferformat(L, format->fields, format->fieldCount);
  } else {
    lua_pushnil(L);
  }
  return 1;
}

static int l_lovrBufferNewReadback(lua_State* L) {
  Buffer* buffer = luax_checktype(L, 1, Buffer);
  uint32_t offset = luax_optu32(L, 2, 0);
  uint32_t extent = luax_optu32(L, 3, ~0u);
  Readback* readback = lovrReadbackCreateBuffer(buffer, offset, extent);
  luax_pushtype(L, Readback, readback);
  lovrRelease(readback, lovrReadbackDestroy);
  return 1;
}

static int l_lovrBufferGetData(lua_State* L) {
  Buffer* buffer = luax_checktype(L, 1, Buffer);
  const DataField* format = lovrBufferGetInfo(buffer)->format;
  lovrCheck(format, "Buffer:getData requires the Buffer to have a format");
  uint32_t index = luax_optu32(L, 2, 1) - 1;
  lovrCheck(index < format->length, "Buffer:getData index exceeds the Buffer's length");
  uint32_t count = luax_optu32(L, 3, format->length - index);
  void* data = lovrBufferGetData(buffer, index * format->stride, count * format->stride);
  return luax_pushbufferdata(L, format, count, data);
}

static int l_lovrBufferSetData(lua_State* L) {
  Buffer* buffer = luax_checktype(L, 1, Buffer);
  const BufferInfo* info = lovrBufferGetInfo(buffer);
  const DataField* format = info->format;
  bool hasNames = format->fields[0].name;

  if (format && format->length == 1) { // When Buffer's length is 1, you can pass a single item
    if (lua_istable(L, 2) && luax_len(L, 2) == 0 && hasNames) {
      luax_checkstruct(L, 2, format->fields, format->fieldCount, lovrBufferSetData(buffer, 0, ~0u));
      return 0;
    } else if (typeComponents[format->fields[0].type] == 1 && lua_type(L, 2) == LUA_TNUMBER) {
      luax_checkfieldn(L, 2, format->fields[0].type, lovrBufferSetData(buffer, 0, ~0u));
      return 0;
    } else if (typeComponents[format->fields[0].type] > 1 && luax_tovector(L, 2, NULL)) {
      luax_checkfieldv(L, 2, format->fields[0].type, lovrBufferSetData(buffer, 0, ~0u));
      return 0;
    }
  }

  if (lua_istable(L, 2)) {
    lovrCheck(format, "Buffer must be created with format information to copy a table to it");

    uint32_t length = luax_len(L, 2);
    uint32_t dstIndex = luax_optu32(L, 3, 1) - 1;
    uint32_t srcIndex = luax_optu32(L, 4, 1) - 1;

    // Fast path for scalar formats
    if (format->fieldCount == 1 && typeComponents[format->fields[0].type] == 1) {
      uint32_t limit = MIN(format->length - dstIndex, length - srcIndex);
      uint32_t count = luax_optu32(L, 5, limit);
      char* data = lovrBufferSetData(buffer, dstIndex * format->stride, count * format->stride);
      for (uint32_t i = 0; i < count; i++, data += format->stride) {
        lua_rawgeti(L, 2, srcIndex + i + 1);
        luax_checkfieldn(L, -1, format->fields[0].type, data);
        lua_pop(L, 1);
      }
      return 0;
    }

    lua_rawgeti(L, 2, 1);
    bool tableOfTables = info->complexFormat || lua_istable(L, -1);
    bool tuples = tableOfTables && !info->complexFormat && (luax_len(L, -1) > 0 || !hasNames);
    lua_pop(L, 1);

    if (tableOfTables) {
      uint32_t limit = MIN(format->length - dstIndex, length - srcIndex);
      uint32_t count = luax_optu32(L, 5, limit);

      lovrCheck(length - srcIndex >= count, "Table does not have enough elements");
      char* data = lovrBufferSetData(buffer, dstIndex * format->stride, count * format->stride);

      if (tuples) {
        luax_checkdatatuples(L, 2, srcIndex + 1, count, format, data);
      } else {
        luax_checkdatakeys(L, 2, srcIndex + 1, count, format, data);
      }
    } else {
      uint32_t tableStride = luax_gettablestride(L, 2, srcIndex + 1, format->fields, format->fieldCount);
      lovrCheck(length % tableStride == 0, "Table length is not aligned -- it either uses inconsistent types for each field or is missing some data");
      uint32_t limit = MIN(format->length - dstIndex, (length - srcIndex) / tableStride);
      uint32_t count = luax_optu32(L, 5, limit);

      lovrCheck((length - srcIndex) / tableStride >= count, "Table does not have enough elements");
      char* data = lovrBufferSetData(buffer, dstIndex * format->stride, count * format->stride);
      luax_checkdataflat(L, 2, srcIndex + 1, count, format, data);
    }

    return 0;
  }

  Blob* blob = luax_totype(L, 2, Blob);

  if (blob) {
    uint32_t dstOffset = luax_optu32(L, 3, 0);
    uint32_t srcOffset = luax_optu32(L, 4, 0);
    lovrCheck(dstOffset < info->size, "Buffer offset is bigger than the size of the Buffer");
    lovrCheck(srcOffset < blob->size, "Blob offset is bigger than the size of the Blob");
    uint32_t limit = (uint32_t) MIN(info->size - dstOffset, blob->size - srcOffset);
    uint32_t extent = luax_optu32(L, 5, limit);
    lovrCheck(extent <= info->size - dstOffset, "Buffer copy range exceeds the size of the target Buffer");
    lovrCheck(extent <= blob->size - srcOffset, "Buffer copy range exceeds the size of the source Blob");
    void* data = lovrBufferSetData(buffer, dstOffset, extent);
    memcpy(data, (char*) blob->data + srcOffset, extent);
    return 0;
  }

  Buffer* src = luax_totype(L, 2, Buffer);

  if (src) {
    Buffer* dst = buffer;
    uint32_t dstOffset = luax_optu32(L, 3, 0);
    uint32_t srcOffset = luax_optu32(L, 4, 0);
    const BufferInfo* dstInfo = info;
    const BufferInfo* srcInfo = lovrBufferGetInfo(src);
    uint32_t limit = MIN(dstInfo->size - dstOffset, srcInfo->size - srcOffset);
    uint32_t extent = luax_optu32(L, 5, limit);
    lovrBufferCopy(src, dst, srcOffset, dstOffset, extent);
    return 0;
  }

  return luax_typeerror(L, 2, "table, Blob, or Buffer");
}

static int l_lovrBufferMapData(lua_State* L) {
  Buffer* buffer = luax_checktype(L, 1, Buffer);
  uint32_t offset = luax_optu32(L, 2, 0);
  uint32_t extent = luax_optu32(L, 3, ~0u);
  void* pointer = lovrBufferSetData(buffer, offset, extent);
  lua_pushlightuserdata(L, pointer);
  return 1;
}

static int l_lovrBufferClear(lua_State* L) {
  Buffer* buffer = luax_checktype(L, 1, Buffer);
  uint32_t offset = luax_optu32(L, 2, 0);
  uint32_t extent = luax_optu32(L, 3, ~0u);
  uint32_t value = (uint32_t) luaL_optinteger(L, 4, 0);
  lovrBufferClear(buffer, offset, extent, value);
  return 0;
}

const luaL_Reg lovrBuffer[] = {
  { "getSize", l_lovrBufferGetSize },
  { "getLength", l_lovrBufferGetLength },
  { "getStride", l_lovrBufferGetStride },
  { "getFormat", l_lovrBufferGetFormat },
  { "newReadback", l_lovrBufferNewReadback },
  { "getData", l_lovrBufferGetData },
  { "setData", l_lovrBufferSetData },
  { "mapData", l_lovrBufferMapData },
  { "clear", l_lovrBufferClear },
  { NULL, NULL }
};
