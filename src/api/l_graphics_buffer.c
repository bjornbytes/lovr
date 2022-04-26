#include "api.h"
#include "graphics/graphics.h"
#include "data/blob.h"
#include "util.h"
#include <lua.h>
#include <lauxlib.h>
#include <stdlib.h>
#include <string.h>

static int l_lovrBufferGetSize(lua_State* L) {
  Buffer* buffer = luax_checktype(L, 1, Buffer);
  const BufferInfo* info = lovrBufferGetInfo(buffer);
  uint32_t size = info->length * MAX(info->stride, 1);
  lua_pushinteger(L, size);
  return 1;
}

static int l_lovrBufferGetLength(lua_State* L) {
  Buffer* buffer = luax_checktype(L, 1, Buffer);
  const BufferInfo* info = lovrBufferGetInfo(buffer);
  lua_pushinteger(L, info->length);
  return 1;
}

static int l_lovrBufferGetStride(lua_State* L) {
  Buffer* buffer = luax_checktype(L, 1, Buffer);
  const BufferInfo* info = lovrBufferGetInfo(buffer);
  lua_pushinteger(L, info->stride);
  return 1;
}

static int l_lovrBufferGetFormat(lua_State* L) {
  Buffer* buffer = luax_checktype(L, 1, Buffer);
  const BufferInfo* info = lovrBufferGetInfo(buffer);
  lua_createtable(L, info->fieldCount, 0);
  for (uint32_t i = 0; i < info->fieldCount; i++) {
    const BufferField* field = &info->fields[i];
    lua_createtable(L, 3, 0);
    lua_pushinteger(L, field->location);
    lua_rawseti(L, -2, 1);
    luax_pushenum(L, FieldType, field->type);
    lua_rawseti(L, -2, 2);
    lua_pushinteger(L, field->offset);
    lua_rawseti(L, -2, 3);
    lua_rawseti(L, -2, i + 1);
  }
  return 1;
}

const luaL_Reg lovrBuffer[] = {
  { "getSize", l_lovrBufferGetSize },
  { "getLength", l_lovrBufferGetLength },
  { "getStride", l_lovrBufferGetStride },
  { "getFormat", l_lovrBufferGetFormat },
  { NULL, NULL }
};
