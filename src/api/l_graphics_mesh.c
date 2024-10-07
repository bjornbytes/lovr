#include "api.h"
#include "graphics/graphics.h"
#include "data/blob.h"
#include "util.h"
#include <string.h>
#include <stdlib.h>

static int l_lovrMeshGetVertexCount(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  const DataField* format = lovrMeshGetVertexFormat(mesh);
  lua_pushinteger(L, format->length);
  return 1;
}

static int l_lovrMeshGetVertexStride(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  const DataField* format = lovrMeshGetVertexFormat(mesh);
  lua_pushinteger(L, format->stride);
  return 1;
}

static int l_lovrMeshGetVertexFormat(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  const DataField* format = lovrMeshGetVertexFormat(mesh);
  lua_createtable(L, (int) format->fieldCount, 0);
  for (uint32_t i = 0; i < format->fieldCount; i++) {
    const DataField* attribute = &format->fields[i];
    lua_createtable(L, 3, 0);
    lua_pushstring(L, attribute->name);
    lua_rawseti(L, -2, 1);
    luax_pushenum(L, DataType, attribute->type);
    lua_rawseti(L, -2, 2);
    lua_pushinteger(L, attribute->offset);
    lua_rawseti(L, -2, 3);
    lua_rawseti(L, -2, (int) i + 1);
  }
  return 1;
}

static int l_lovrMeshGetVertexBuffer(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  Buffer* buffer = lovrMeshGetVertexBuffer(mesh);
  luax_pushtype(L, Buffer, buffer);
  return 1;
}

static int l_lovrMeshGetIndexBuffer(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  Buffer* buffer = lovrMeshGetIndexBuffer(mesh);
  luax_pushtype(L, Buffer, buffer);
  return 1;
}

static int l_lovrMeshSetIndexBuffer(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  Buffer* buffer = luax_checktype(L, 2, Buffer);
  luax_assert(L, lovrMeshSetIndexBuffer(mesh, buffer));
  return 0;
}

static int l_lovrMeshGetVertices(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  uint32_t index = luax_optu32(L, 2, 1) - 1;
  uint32_t count = luax_optu32(L, 3, ~0u);
  char* data = lovrMeshGetVertices(mesh, index, count);
  luax_assert(L, data);
  const DataField* format = lovrMeshGetVertexFormat(mesh);
  count = count == ~0u ? format->length - index : count;
  lua_createtable(L, (int) count, 0);
  for (uint32_t i = 0; i < count; i++, data += format->stride) {
    lua_newtable(L);
    int j = 1;
    const DataField* field = format->fields;
    for (uint32_t f = 0; f < format->fieldCount; f++, field++) {
      int n = luax_pushbufferdata(L, field, 0, data + field->offset);
      for (int c = 0; c < n; c++) {
        lua_rawseti(L, -1 - n + c, j + n - (c + 1));
      }
      j += n;
    }
    lua_rawseti(L, -2, (int) i + 1);
  }
  return 1;
}

static int l_lovrMeshSetVertices(lua_State* L) {
  Blob* blob = NULL;
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  const DataField* format = lovrMeshGetVertexFormat(mesh);
  uint32_t index = luax_optu32(L, 3, 1) - 1;
  if ((blob = luax_totype(L, 2, Blob)) != NULL) {
    uint32_t limit = (uint32_t) MIN(blob->size / format->stride, format->length - index);
    uint32_t count = luax_optu32(L, 4, limit);
    luax_check(L, blob->size / format->stride >= count, "Tried to read past the end of the Blob");
    void* data = lovrMeshSetVertices(mesh, index, count);
    luax_assert(L, data);
    memcpy(data, blob->data, count * format->stride);
  } else if (lua_istable(L, 2)) {
    uint32_t length = luax_len(L, 2);
    uint32_t limit = MIN(length, format->length - index);
    uint32_t count = luax_optu32(L, 4, limit);
    luax_check(L, length <= limit, "Table does not have enough data to set %d items", count);
    void* data = lovrMeshSetVertices(mesh, index, count);
    luax_assert(L, data);
    luax_checkbufferdata(L, 2, format, data);
  } else {
    return luax_typeerror(L, 2, "table or Blob");
  }
  return 0;
}

static int l_lovrMeshGetIndices(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);

  uint32_t count;
  DataType type;
  void* data;
  luax_assert(L, lovrMeshGetIndices(mesh, &data, &count, &type));

  if (!data) {
    lua_pushnil(L);
    return 1;
  }

  uint16_t* u16 = data;
  uint32_t* u32 = data;
  lua_createtable(L, (int) count, 0);
  for (uint32_t i = 0; i < count; i++) {
    switch (type) {
      case TYPE_U16: lua_pushinteger(L, u16[i]); break;
      case TYPE_U32: lua_pushinteger(L, u32[i]); break;
      case TYPE_INDEX16: lua_pushinteger(L, u16[i] + 1); break;
      case TYPE_INDEX32: lua_pushinteger(L, u32[i] + 1); break;
      default: lovrUnreachable();
    }
    lua_rawseti(L, -2, i + 1);
  }

  return 1;
}

static int l_lovrMeshSetIndices(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  const DataField* format = lovrMeshGetVertexFormat(mesh);

  switch (lua_type(L, 2)) {
    case LUA_TNONE:
    case LUA_TNIL:
      lovrMeshSetIndices(mesh, 0, TYPE_U16);
      return 0;
    case LUA_TTABLE: {
      uint32_t count = luax_len(L, 2);
      if (format->length > 0xffff) {
        uint32_t* data = lovrMeshSetIndices(mesh, count, TYPE_INDEX32);
        luax_assert(L, data);
        for (uint32_t i = 0; i < count; i++) {
          lua_rawgeti(L, 2, i + 1);
          lua_Integer x = lua_tointeger(L, -1);
          luax_check(L, x > 0 && x <= format->length, "Mesh index #%d is out of range", i + 1);
          data[i] = x - 1;
          lua_pop(L, 1);
        }
      } else {
        uint16_t* data = lovrMeshSetIndices(mesh, count, TYPE_INDEX16);
        luax_assert(L, data);
        for (uint32_t i = 0; i < count; i++) {
          lua_rawgeti(L, 2, i + 1);
          lua_Integer x = lua_tointeger(L, -1);
          luax_check(L, x > 0 && x <= format->length, "Mesh index #%d is out of range", i + 1);
          data[i] = x - 1;
          lua_pop(L, 1);
        }
      }
      break;
    }
    case LUA_TUSERDATA: {
      Blob* blob = luax_checktype(L, 2, Blob);
      DataType type = luax_checkenum(L, 3, DataType, NULL);
      luax_check(L, type == TYPE_U16 || type == TYPE_U32, "Blob type must be u16 or u32");
      size_t stride = type == TYPE_U16 ? 2 : 4;
      uint32_t count = (uint32_t) MIN(blob->size / stride, UINT32_MAX);
      void* data = lovrMeshSetIndices(mesh, count, type);
      luax_assert(L, data);
      memcpy(data, blob->data, count * stride);
      break;
    }
    default: return luax_typeerror(L, 2, "nil, table, or Blob");
  }

  return 0;
}

static int l_lovrMeshGetBoundingBox(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  float box[6];
  if (lovrMeshGetBoundingBox(mesh, box)) {
    lua_pushnumber(L, box[0]);
    lua_pushnumber(L, box[1]);
    lua_pushnumber(L, box[2]);
    lua_pushnumber(L, box[3]);
    lua_pushnumber(L, box[4]);
    lua_pushnumber(L, box[5]);
    return 6;
  } else {
    lua_pushnil(L);
    return 1;
  }
}

static int l_lovrMeshSetBoundingBox(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  if (lua_isnoneornil(L, 2)) {
    lovrMeshSetBoundingBox(mesh, NULL);
  } else {
    float box[6];
    box[0] = luax_checkfloat(L, 2);
    box[1] = luax_checkfloat(L, 3);
    box[2] = luax_checkfloat(L, 4);
    box[3] = luax_checkfloat(L, 5);
    box[4] = luax_checkfloat(L, 6);
    box[5] = luax_checkfloat(L, 7);
    lovrMeshSetBoundingBox(mesh, box);
  }
  return 0;
}

static int l_lovrMeshComputeBoundingBox(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  bool updated = lovrMeshComputeBoundingBox(mesh);
  lua_pushboolean(L, updated);
  return 1;
}

static int l_lovrMeshGetDrawMode(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  DrawMode mode = lovrMeshGetDrawMode(mesh);
  luax_pushenum(L, DrawMode, mode);
  return 1;
}

static int l_lovrMeshSetDrawMode(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  DrawMode mode = luax_checkenum(L, 2, DrawMode, NULL);
  lovrMeshSetDrawMode(mesh, mode);
  return 0;
}

static int l_lovrMeshGetDrawRange(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);

  uint32_t start, count, offset;
  lovrMeshGetDrawRange(mesh, &start, &count, &offset);

  if (count == 0) {
    return 0;
  }

  lua_pushinteger(L, start + 1);
  lua_pushinteger(L, count);
  lua_pushinteger(L, offset);
  return 3;
}

static int l_lovrMeshSetDrawRange(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);

  if (lua_isnoneornil(L, 2)) {
    lovrMeshSetDrawRange(mesh, 0, 0, 0);
  } else {
    uint32_t start = luax_checku32(L, 2) - 1;
    uint32_t count = luax_checku32(L, 3);
    uint32_t offset = luax_optu32(L, 4, 0);
    luax_assert(L, lovrMeshSetDrawRange(mesh, start, count, offset));
  }

  return 0;
}

static int l_lovrMeshGetMaterial(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  Material* material = lovrMeshGetMaterial(mesh);
  luax_pushtype(L, Material, material);
  return 1;
}

static int l_lovrMeshSetMaterial(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  Material* material = luax_optmaterial(L, 2);
  lovrMeshSetMaterial(mesh, material);
  return 0;
}

const luaL_Reg lovrMesh[] = {
  { "getVertexCount", l_lovrMeshGetVertexCount },
  { "getVertexStride", l_lovrMeshGetVertexStride },
  { "getVertexFormat", l_lovrMeshGetVertexFormat },
  { "getVertexBuffer", l_lovrMeshGetVertexBuffer },
  { "getIndexBuffer", l_lovrMeshGetIndexBuffer },
  { "setIndexBuffer", l_lovrMeshSetIndexBuffer },
  { "getVertices", l_lovrMeshGetVertices },
  { "setVertices", l_lovrMeshSetVertices },
  { "getIndices", l_lovrMeshGetIndices },
  { "setIndices", l_lovrMeshSetIndices },
  { "getBoundingBox", l_lovrMeshGetBoundingBox },
  { "setBoundingBox", l_lovrMeshSetBoundingBox },
  { "computeBoundingBox", l_lovrMeshComputeBoundingBox },
  { "getDrawMode", l_lovrMeshGetDrawMode },
  { "setDrawMode", l_lovrMeshSetDrawMode },
  { "getDrawRange", l_lovrMeshGetDrawRange },
  { "setDrawRange", l_lovrMeshSetDrawRange },
  { "getMaterial", l_lovrMeshGetMaterial },
  { "setMaterial", l_lovrMeshSetMaterial },
  { NULL, NULL }
};
