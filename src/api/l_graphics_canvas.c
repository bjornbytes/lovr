#include "api.h"
#include "graphics/canvas.h"
#include "graphics/graphics.h"
#include "graphics/texture.h"
#include <lua.h>
#include <lauxlib.h>
#include <stdlib.h>

static int luax_checkattachment(lua_State* L, int index, Attachment* attachment) {
  if (lua_istable(L, index)) {
    lua_rawgeti(L, index, 1);
    attachment->texture = luax_checktype(L, -1, Texture);
    lua_pop(L, 1);

    lua_rawgeti(L, index, 2);
    attachment->slice = luaL_optinteger(L, -1, 1) - 1;
    lua_pop(L, 1);

    lua_rawgeti(L, index, 3);
    attachment->level = luax_optmipmap(L, -1, attachment->texture);
    lua_pop(L, 1);

    index++;
  } else {
    attachment->texture = luax_checktype(L, index++, Texture);
    attachment->slice = lua_type(L, index) == LUA_TNUMBER ? lua_tointeger(L, index++) - 1 : 0;
    attachment->level = lua_type(L, index) == LUA_TNUMBER ? luax_optmipmap(L, index++, attachment->texture) : 0;
  }
  return index;
}

void luax_readattachments(lua_State* L, int index, Attachment* attachments, int* count) {
  bool table = lua_istable(L, index);
  int top = table ? -1 : lua_gettop(L);
  int n;

  if (table) {
    n = luax_len(L, index);
    n = MIN(n, 3 * MAX_CANVAS_ATTACHMENTS);
    for (int i = 0; i < n; i++) {
      lua_rawgeti(L, index, i + 1);
    }
    index = -n;
  }

  for (*count = 0; *count < MAX_CANVAS_ATTACHMENTS && index <= top; (*count)++) {
    index = luax_checkattachment(L, index, attachments + *count);
  }

  if (table) {
    lua_pop(L, n);
  }
}

static int l_lovrCanvasNewImage(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  uint32_t index = luaL_optinteger(L, 2, 1) - 1;
  uint32_t count;
  lovrCanvasGetAttachments(canvas, &count);
  lovrAssert(index < count, "Can not create an Image from Texture #%d of Canvas (it only has %d textures)", index, count);
  Image* image = lovrCanvasNewImage(canvas, index);
  luax_pushtype(L, Image, image);
  lovrRelease(image, lovrImageDestroy);
  return 1;
}

static int l_lovrCanvasRenderTo(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  luaL_checktype(L, 2, LUA_TFUNCTION);
  int argumentCount = lua_gettop(L) - 2;
  Canvas* old = lovrGraphicsGetCanvas();
  lovrGraphicsSetCanvas(canvas);
  lua_call(L, argumentCount, 0);
  lovrGraphicsSetCanvas(old);
  return 0;
}

static int l_lovrCanvasGetTexture(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  uint32_t count;
  const Attachment* attachments = lovrCanvasGetAttachments(canvas, &count);
  for (uint32_t i = 0; i < count; i++) {
    luax_pushtype(L, Texture, attachments[i].texture);
  }
  return count;
}

static int l_lovrCanvasSetTexture(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  Attachment attachments[MAX_CANVAS_ATTACHMENTS];
  int count;
  luax_readattachments(L, 2, attachments, &count);
  lovrCanvasSetAttachments(canvas, attachments, count);
  return 0;
}

static int l_lovrCanvasGetWidth(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  lua_pushinteger(L, lovrCanvasGetWidth(canvas));
  return 1;
}

static int l_lovrCanvasGetHeight(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  lua_pushinteger(L, lovrCanvasGetHeight(canvas));
  return 1;
}

static int l_lovrCanvasGetDimensions(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  lua_pushinteger(L, lovrCanvasGetWidth(canvas));
  lua_pushinteger(L, lovrCanvasGetHeight(canvas));
  return 2;
}

static int l_lovrCanvasGetDepthTexture(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  Texture* texture = lovrCanvasGetDepthTexture(canvas);
  luax_pushtype(L, Texture, texture);
  return 1;
}

static int l_lovrCanvasGetMSAA(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  int msaa = lovrCanvasGetMSAA(canvas);
  lua_pushinteger(L, msaa);
  return 1;
}

static int l_lovrCanvasIsStereo(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  bool stereo = lovrCanvasIsStereo(canvas);
  lua_pushboolean(L, stereo);
  return 1;
}

const luaL_Reg lovrCanvas[] = {
  { "newImage", l_lovrCanvasNewImage },
  { "renderTo", l_lovrCanvasRenderTo },
  { "getTexture", l_lovrCanvasGetTexture },
  { "setTexture", l_lovrCanvasSetTexture },
  { "getWidth", l_lovrCanvasGetWidth },
  { "getHeight", l_lovrCanvasGetHeight },
  { "getDimensions", l_lovrCanvasGetDimensions },
  { "getDepthTexture", l_lovrCanvasGetDepthTexture },
  { "getMSAA", l_lovrCanvasGetMSAA },
  { "isStereo", l_lovrCanvasIsStereo },
  { NULL, NULL }
};
