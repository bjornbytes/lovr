#include "api.h"
#include "data/blob.h"
#include "filesystem/filesystem.h"
#include "util.h"
#include <stdlib.h>

static int l_lovrFileGetMode(lua_State* L) {
  File* file = luax_checktype(L, 1, File);
  OpenMode mode = lovrFileGetMode(file);
  luax_pushenum(L, OpenMode, mode);
  return 1;
}

static int l_lovrFileGetPath(lua_State* L) {
  File* file = luax_checktype(L, 1, File);
  const char* path = lovrFileGetPath(file);
  lua_pushstring(L, path);
  return 1;
}

static int l_lovrFileGetSize(lua_State* L) {
  File* file = luax_checktype(L, 1, File);
  uint64_t size;
  if (lovrFileGetSize(file, &size)) {
    if (size >= 1ull << 53) {
      lua_pushnil(L);
      lua_pushstring(L, "Too big");
      return 2;
    } else {
      lua_pushinteger(L, size);
      return 1;
    }
  } else {
    lua_pushnil(L);
    lua_pushstring(L, lovrGetError());
    return 2;
  }
}

static int l_lovrFileRead(lua_State* L) {
  File* file = luax_checktype(L, 1, File);
  size_t size;
  if (lua_type(L, 2) == LUA_TNUMBER) {
    lua_Number n = lua_tonumber(L, 2);
    luax_check(L, n >= 0, "Number of bytes to read can not be negative");
    luax_check(L, n < 9007199254740992.0, "Number of bytes to read must be less than 2^53");
    size = (size_t) n;
  } else {
    luax_assert(L, lovrFileGetSize(file, &size));
    size -= lovrFileTell(file);
  }
  size_t count;
  void* data = lovrMalloc(size);
  bool success = lovrFileRead(file, data, size, &count);
  if (success) {
    lua_pushlstring(L, data, count);
    lua_pushnumber(L, count);
    lovrFree(data);
    return 2;
  } else {
    lovrFree(data);
    lua_pushnil(L);
    lua_pushstring(L, lovrGetError());
    return 2;
  }
}

static int l_lovrFileWrite(lua_State* L) {
  File* file = luax_checktype(L, 1, File);
  const void* data;
  size_t size;
  Blob* blob = luax_totype(L, 2, Blob);
  if (blob) {
    data = blob->data;
    size = blob->size;
  } else if (lua_type(L, 2) == LUA_TSTRING) {
    data = lua_tolstring(L, 2, &size);
  } else {
    return luax_typeerror(L, 2, "string or Blob");
  }
  if (lua_type(L, 3) == LUA_TNUMBER) {
    lua_Number n = lua_tonumber(L, 2);
    luax_check(L, n >= 0, "Number of bytes to write can not be negative");
    luax_check(L, n < 9007199254740992.0, "Number of bytes to write must be less than 2^53");
    luax_check(L, n <= size, "Number of bytes to write is bigger than the size of the source");
    size = (size_t) n;
  }
  size_t count;
  bool success = lovrFileWrite(file, data, size, &count);
  if (success) {
    lua_pushboolean(L, true);
    return 1;
  } else {
    lua_pushboolean(L, false);
    lua_pushstring(L, lovrGetError());
    return 2;
  }
}

static int l_lovrFileSeek(lua_State* L) {
  File* file = luax_checktype(L, 1, File);
  lua_Number offset = luaL_checknumber(L, 2);
  luax_check(L, offset >= 0 && offset < 9007199254740992.0, "Invalid seek position");
  if (lovrFileSeek(file, offset)) {
    lua_pushboolean(L, true);
    return 1;
  } else {
    lua_pushboolean(L, false);
    lua_pushstring(L, lovrGetError());
    return 2;
  }
}

static int l_lovrFileTell(lua_State* L) {
  File* file = luax_checktype(L, 1, File);
  uint64_t offset = lovrFileTell(file);
  if (offset >= 1ull << 53) {
    lua_pushnil(L);
  } else {
    lua_pushinteger(L, offset);
  }
  return 1;
}

static int l_lovrFileIsEOF(lua_State* L) {
  File* file = luax_checktype(L, 1, File);
  OpenMode mode = lovrFileGetMode(file);
  if (mode == OPEN_READ) {
    uint64_t size;
    if (!lovrFileGetSize(file, &size)) {
      lua_pushboolean(L, true);
    } else {
      uint64_t offset = lovrFileTell(file);
      lua_pushboolean(L, offset >= size);
    }
  } else {
    lua_pushboolean(L, false);
  }
  return 1;
}

const luaL_Reg lovrFile[] = {
  { "getMode", l_lovrFileGetMode },
  { "getPath", l_lovrFileGetPath },
  { "getSize", l_lovrFileGetSize },
  { "read", l_lovrFileRead },
  { "write", l_lovrFileWrite },
  { "seek", l_lovrFileSeek },
  { "tell", l_lovrFileTell },
  { "isEOF", l_lovrFileIsEOF },
  { NULL, NULL }
};
