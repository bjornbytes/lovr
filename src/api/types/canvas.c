#include "api.h"
#include "graphics/canvas.h"
#include "graphics/graphics.h"

static int luax_checkattachment(lua_State* L, int index, Attachment* attachment) {
  attachment->texture = luax_checktype(L, index++, Texture);
  attachment->slice = lua_type(L, index) == LUA_TNUMBER ? lua_tointeger(L, index++) - 1 : 0;
  attachment->level = lua_type(L, index) == LUA_TNUMBER ? luax_optmipmap(L, index++, attachment->texture) : 0;
  bool isValidSlice = attachment->slice >= 0 && attachment->slice < lovrTextureGetDepth(attachment->texture, 0);
  lovrAssert(isValidSlice, "Invalid slice %d\n", attachment->slice + 1);
  return index;
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

const luaL_Reg lovrCanvas[] = {
  { "getTexture", l_lovrCanvasGetTexture },
  { "setTexture", l_lovrCanvasSetTexture },
  { "renderTo", l_lovrCanvasRenderTo },
  { NULL, NULL }
};
