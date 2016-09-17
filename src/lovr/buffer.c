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
  { "getVertex", l_lovrBufferGetVertex },
  { "setVertex", l_lovrBufferSetVertex },
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

int l_lovrBufferGetDrawRange(lua_State* L) {
  Buffer* buffer = luax_checkbuffer(L, 1);
  int start, end;
  lovrBufferGetDrawRange(buffer, &start, &end);
  lua_pushinteger(L, start);
  lua_pushinteger(L, end);
  return 2;
}

int l_lovrBufferSetDrawRange(lua_State* L) {
  Buffer* buffer = luax_checkbuffer(L, 1);
  int rangeStart = luaL_checkinteger(L, 2);
  int rangeEnd = luaL_checkinteger(L, 3);
  if (lovrBufferSetDrawRange(buffer, rangeStart, rangeEnd)) {
    return luaL_error(L, "Invalid buffer draw range (%d, %d)", rangeStart, rangeEnd);
  }
  return 0;
}
