#include "api.h"
#include "graphics/graphics.h"
#include "data/blob.h"
#include "util.h"
#include <string.h>

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
  const DataField* attributes = format->childCount > 0 ? format->children : format;
  uint32_t attributeCount = MAX(format->childCount, 1);
  lua_createtable(L, (int) attributeCount, 0);
  for (uint32_t i = 0; i < attributeCount; i++) {
    const DataField* attribute = &attributes[i];
    lua_createtable(L, 3, 0);
    lua_pushstring(L, attribute->name);
    lua_rawseti(L, -2, 1);
    luax_pushenum(L, DataType, attribute->type);
    lua_rawseti(L, -2, 2);
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
  lovrMeshSetIndexBuffer(mesh, buffer);
  return 0;
}

static int l_lovrMeshGetVertices(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  uint32_t index = luax_optu32(L, 2, 1) - 1;
  uint32_t count = luax_optu32(L, 3, ~0u);
  DataField format = *lovrMeshGetVertexFormat(mesh);
  void* data = lovrMeshGetVertices(mesh, index, count);
  format.length = count == ~0u ? format.length - index : count;
  return luax_pushbufferdata(L, &format, data);
}

static int l_lovrMeshSetVertices(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  uint32_t index = luax_optu32(L, 3, 1) - 1;
  uint32_t count = luax_optu32(L, 4, ~0u);
  void* data = lovrMeshSetVertices(mesh, index, count);
  luax_checkbufferdata(L, 2, lovrMeshGetVertexFormat(mesh), data);
  return 0;
}

static int l_lovrMeshGetIndices(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  DataField format;
  void* data = lovrMeshGetIndices(mesh, &format);
  return luax_pushbufferdata(L, &format, data);
}

static int l_lovrMeshSetIndices(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);

  switch (lua_type(L, 2)) {
    case LUA_TNONE:
    case LUA_TNIL:
      lovrMeshSetIndices(mesh, 0, TYPE_U16);
      return 0;
    case LUA_TTABLE: {
      uint32_t count = luax_len(L, 2);
      DataType type = lovrMeshGetVertexFormat(mesh)->length > 0xffff ? TYPE_INDEX32 : TYPE_INDEX16;
      void* data = lovrMeshSetIndices(mesh, count, type);
      DataField format = { .type = type, .length = count, .stride = type == TYPE_INDEX32 ? 4 : 2 };
      luax_checkbufferdata(L, 2, &format, data);
      break;
    }
    case LUA_TUSERDATA: {
      Blob* blob = luax_checktype(L, 2, Blob);
      DataType type = luax_checkenum(L, 3, DataType, NULL);
      lovrCheck(type == TYPE_U16 || type == TYPE_U32, "Blob type must be u16 or u32");
      size_t stride = type == TYPE_U16 ? 2 : 4;
      uint32_t count = blob->size / stride;
      void* data = lovrMeshSetIndices(mesh, count, type);
      memcpy(data, blob->data, count * stride);
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
  return 2;
}

static int l_lovrMeshSetDrawRange(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);

  if (lua_isnoneornil(L, 2)) {
    lovrMeshSetDrawRange(mesh, 0, 0, 0);
  } else {
    uint32_t start = luax_checku32(L, 2) - 1;
    uint32_t count = luax_checku32(L, 3);
    uint32_t offset = luax_optu32(L, 3, 0);
    lovrMeshSetDrawRange(mesh, start, count, offset);
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
  Material* material = luax_checktype(L, 2, Material);
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
