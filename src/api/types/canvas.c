#include "api.h"
#include "graphics/canvas.h"
#include "graphics/graphics.h"

Texture* luax_checktexture(lua_State* L, int index) {
  Canvas* canvas = luax_totype(L, index, Canvas);
  if (canvas) {
    const Attachment* attachment = lovrCanvasGetAttachments(canvas, NULL);
    return attachment->texture;
  } else {
    return luax_checktype(L, index, Texture);
  }
}

static int luax_checkattachment(lua_State* L, int index, Attachment* attachment) {
  attachment->texture = luax_checktype(L, index++, Texture);
  attachment->slice = lua_type(L, index) == LUA_TNUMBER ? lua_tointeger(L, index++) - 1 : 0;
  attachment->level = lua_type(L, index) == LUA_TNUMBER ? luax_optmipmap(L, index++, attachment->texture) : 0;
  bool isValidSlice = attachment->slice >= 0 && attachment->slice < lovrTextureGetDepth(attachment->texture, 0);
  lovrAssert(isValidSlice, "Invalid slice %d\n", attachment->slice + 1);
  return index;
}

int l_lovrCanvasNewTextureData(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  int index = luaL_optinteger(L, 2, 1) - 1;
  int count;
  lovrCanvasGetAttachments(canvas, &count);
  lovrAssert(index >= 0 && index < count, "Can not create a TextureData from Texture #%d of Canvas (it only has %d textures)", index, count);
  TextureData* textureData = lovrCanvasNewTextureData(canvas, index);
  luax_pushobject(L, textureData);
  lovrRelease(textureData);
  return 1;
}

int l_lovrCanvasRenderTo(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  luaL_checktype(L, 2, LUA_TFUNCTION);
  int argumentCount = lua_gettop(L) - 2;
  Canvas* old = lovrGraphicsGetCanvas();
  lovrGraphicsSetCanvas(canvas);
  lua_call(L, argumentCount, 0);
  lovrGraphicsSetCanvas(old);
  return 0;
}

int l_lovrCanvasGetTexture(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  int count;
  const Attachment* attachments = lovrCanvasGetAttachments(canvas, &count);
  for (int i = 0; i < count; i++) {
    luax_pushobject(L, attachments[i].texture);
  }
  return count;
}

int l_lovrCanvasSetTexture(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  Attachment attachments[MAX_CANVAS_ATTACHMENTS];
  int count;
  int index = 2;
  int top = lua_gettop(L);
  for (count = 0; count < MAX_CANVAS_ATTACHMENTS && index <= top; count++) {
    index = luax_checkattachment(L, index, attachments + count);
  }

  lovrCanvasSetAttachments(canvas, attachments, count);

  return 0;
}

int l_lovrCanvasGetWidth(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  lua_pushinteger(L, lovrCanvasGetWidth(canvas));
  return 1;
}

int l_lovrCanvasGetHeight(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  lua_pushinteger(L, lovrCanvasGetHeight(canvas));
  return 1;
}

int l_lovrCanvasGetDimensions(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  lua_pushinteger(L, lovrCanvasGetWidth(canvas));
  lua_pushinteger(L, lovrCanvasGetHeight(canvas));
  return 2;
}

int l_lovrCanvasGetDepthFormat(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  DepthFormat format = lovrCanvasGetDepthFormat(canvas);
  lua_pushstring(L, DepthFormats[format]);
  return 1;
}

int l_lovrCanvasGetMSAA(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  int msaa = lovrCanvasGetMSAA(canvas);
  lua_pushinteger(L, msaa);
  return 1;
}

int l_lovrCanvasIsStereo(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  bool stereo = lovrCanvasIsStereo(canvas);
  lua_pushboolean(L, stereo);
  return 1;
}

const luaL_Reg lovrCanvas[] = {
  { "newTextureData", l_lovrCanvasNewTextureData },
  { "renderTo", l_lovrCanvasRenderTo },
  { "getTexture", l_lovrCanvasGetTexture },
  { "setTexture", l_lovrCanvasSetTexture },
  { "getWidth", l_lovrCanvasGetWidth },
  { "getHeight", l_lovrCanvasGetHeight },
  { "getDimensions", l_lovrCanvasGetDimensions },
  { "getDepthFormat", l_lovrCanvasGetDepthFormat },
  { "getMSAA", l_lovrCanvasGetMSAA },
  { "isStereo", l_lovrCanvasIsStereo },
  { NULL, NULL }
};
