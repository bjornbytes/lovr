#include "api.h"
#include "graphics/canvas.h"

static int luax_checkattachment(lua_State* L, int index, Attachment* attachment) {
  attachment->texture = luax_checktype(L, index++, Texture);
  attachment->slice = lua_type(L, index) == LUA_TNUMBER ? lua_tointeger(L, index++) - 1 : 0;
  attachment->level = lua_type(L, index) == LUA_TNUMBER ? lua_tointeger(L, index++) - 1 : 0;
  return index;
}

int l_lovrCanvasGetTexture(lua_State* L) {
  return 0;
}

int l_lovrCanvasSetTexture(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);

  int index = 2;
  AttachmentType type = ATTACHMENT_COLOR;
  if (lua_type(L, index) == LUA_TSTRING) {
    type = luaL_checkoption(L, index++, NULL, AttachmentTypes);
  }

  if (type == ATTACHMENT_COLOR) {
    Attachment attachments[MAX_COLOR_ATTACHMENTS];
    int top = lua_gettop(L);
    int count;

    for (count = 0; count < MAX_COLOR_ATTACHMENTS && index <= top; count++) {
      index = luax_checkattachment(L, index, attachments + count);
    }

    //lovrCanvasSetAttachments(type, attachments, count);
  } else if (type == ATTACHMENT_DEPTH) {
    Attachment attachment;
    luax_checkattachment(L, index, &attachment);
    //lovrCanvasSetAttachments(type, &attachment, 1);
  }

  return 0;
}

const luaL_Reg lovrCanvas[] = {
  { "getTexture", l_lovrCanvasGetTexture },
  { "setTexture", l_lovrCanvasSetTexture },
  { NULL, NULL }
};
