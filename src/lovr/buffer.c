#include "buffer.h"
#include "graphics.h"
#include "../graphics/buffer.h"

void luax_pushbuffer(lua_State* L, Buffer* buffer) {
  if (buffer == NULL) {
    return lua_pushnil(L);
  }

  Buffer** userdata = (Buffer**) lua_newuserdata(L, sizeof(Buffer*));
  luaL_getmetatable(L, "Buffer");
  lua_setmetatable(L, -2);
  *userdata = buffer;
}

Buffer* luax_checkbuffer(lua_State* L, int index) {
  return *(Buffer**) luaL_checkudata(L, index, "Buffer");
}

int luax_destroybuffer(lua_State* L) {
  Buffer* buffer = luax_checkbuffer(L, 1);
  lovrBufferDestroy(buffer);
  return 0;
}

const luaL_Reg lovrBuffer[] = {
  { "draw", l_lovrBufferDraw },
  { "getVertexCount", l_lovrBufferGetVertexCount },
  { "getVertex", l_lovrBufferGetVertex },
  { "setVertex", l_lovrBufferSetVertex },
  { "getVertexMap", l_lovrBufferGetVertexMap },
  { "setVertexMap", l_lovrBufferSetVertexMap },
  { "getDrawMode", l_lovrBufferGetDrawMode },
  { "setDrawMode", l_lovrBufferSetDrawMode },
  { "getDrawRange", l_lovrBufferGetDrawRange },
  { "setDrawRange", l_lovrBufferSetDrawRange },
  { NULL, NULL }
};

int l_lovrBufferDraw(lua_State* L) {
  Buffer* buffer = luax_checkbuffer(L, 1);
  lovrBufferDraw(buffer);
  return 0;
}

int l_lovrBufferGetDrawMode(lua_State* L) {
  Buffer* buffer = luax_checkbuffer(L, 1);
  BufferDrawMode drawMode = lovrBufferGetDrawMode(buffer);

  switch (drawMode) {
    case BUFFER_POINTS:
      lua_pushstring(L, "points");
      break;

    case BUFFER_TRIANGLE_STRIP:
      lua_pushstring(L, "strip");
      break;

    case BUFFER_TRIANGLES:
      lua_pushstring(L, "triangles");
      break;

    case BUFFER_TRIANGLE_FAN:
      lua_pushstring(L, "fan");
      break;

    default:
      lua_pushstring(L, "unknown");
      break;
  }

  return 1;
}

int l_lovrBufferSetDrawMode(lua_State* L) {
  Buffer* buffer = luax_checkbuffer(L, 1);

  const char* userDrawMode = luaL_checkstring(L, 2);
  BufferDrawMode* drawMode = (BufferDrawMode*) map_get(&BufferDrawModes, userDrawMode);
  if (!drawMode) {
    return luaL_error(L, "Invalid buffer draw mode: '%s'", userDrawMode);
  }

  lovrBufferSetDrawMode(buffer, *drawMode);

  return 0;
}

int l_lovrBufferGetVertexCount(lua_State* L) {
  Buffer* buffer = luax_checkbuffer(L, 1);
  lua_pushnumber(L, lovrBufferGetVertexCount(buffer));
  return 1;
}

int l_lovrBufferGetVertex(lua_State* L) {
  Buffer* buffer = luax_checkbuffer(L, 1);
  int index = luaL_checkint(L, 2) - 1;
  float x, y, z;
  lovrBufferGetVertex(buffer, index, &x, &y, &z);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, z);
  return 3;
}

int l_lovrBufferSetVertex(lua_State* L) {
  Buffer* buffer = luax_checkbuffer(L, 1);
  int index = luaL_checkint(L, 2) - 1;
  float x = luaL_checknumber(L, 3);
  float y = luaL_checknumber(L, 4);
  float z = luaL_checknumber(L, 5);
  lovrBufferSetVertex(buffer, index, x, y, z);
  return 0;
}

int l_lovrBufferGetVertexMap(lua_State* L) {
  Buffer* buffer = luax_checkbuffer(L, 1);
  int count;
  unsigned int* indices = lovrBufferGetVertexMap(buffer, &count);

  if (count == 0) {
    lua_pushnil(L);
    return 1;
  }

  lua_newtable(L);
  for (int i = 0; i < count; i++) {
    lua_pushinteger(L, indices[i]);
    lua_rawseti(L, -2, i + 1);
  }

  return 1;
}

int l_lovrBufferSetVertexMap(lua_State* L) {
  Buffer* buffer = luax_checkbuffer(L, 1);

  if (lua_isnoneornil(L, 2)) {
    lovrBufferSetVertexMap(buffer, NULL, 0);
    return 0;
  }

  luaL_checktype(L, 2, LUA_TTABLE);
  int count = lua_objlen(L, 2);
  unsigned int* indices = malloc(count * sizeof(int));

  for (int i = 0; i < count; i++) {
    lua_rawgeti(L, 2, i + 1);
    if (!lua_isnumber(L, -1)) {
      return luaL_error(L, "Buffer vertex map index #%d must be numeric", i);
    }
    indices[i] = lua_tonumber(L, -1) - 1;
    lua_pop(L, 1);
  }

  lovrBufferSetVertexMap(buffer, indices, count);
  free(indices);
  return 0;
}

int l_lovrBufferGetDrawRange(lua_State* L) {
  Buffer* buffer = luax_checkbuffer(L, 1);
  if (!lovrBufferIsRangeEnabled(buffer)) {
    lua_pushnil(L);
    return 1;
  }

  int start, count;
  lovrBufferGetDrawRange(buffer, &start, &count);
  lua_pushinteger(L, start + 1);
  lua_pushinteger(L, count);
  return 2;
}

int l_lovrBufferSetDrawRange(lua_State* L) {
  Buffer* buffer = luax_checkbuffer(L, 1);
  if (lua_isnoneornil(L, 2)) {
    lovrBufferSetRangeEnabled(buffer, 0);
    return 0;
  }

  lovrBufferSetRangeEnabled(buffer, 1);
  int rangeStart = luaL_checkinteger(L, 2) - 1;
  int rangeCount = luaL_checkinteger(L, 3);
  if (lovrBufferSetDrawRange(buffer, rangeStart, rangeCount)) {
    return luaL_error(L, "Invalid buffer draw range (%d, %d)", rangeStart + 1, rangeCount);
  }

  return 0;
}
