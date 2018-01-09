#include "api.h"
#include "graphics/graphics.h"
#include "graphics/canvas.h"

int l_lovrCanvasRenderTo(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  luaL_checktype(L, 2, LUA_TFUNCTION);
  int nargs = lua_gettop(L) - 2;
  lovrGraphicsPushView();
  lovrCanvasBind(canvas);
  lua_call(L, nargs, 0);
  lovrCanvasResolveMSAA(canvas);
  lovrGraphicsPopView();
  return 0;
}

int l_lovrCanvasGetFormat(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  TextureFormat format = lovrCanvasGetFormat(canvas);
  luax_pushenum(L, &TextureFormats, format);
  return 1;
}

int l_lovrCanvasGetMSAA(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  lua_pushinteger(L, lovrCanvasGetMSAA(canvas));
  return 1;
}

const luaL_Reg lovrCanvas[] = {
  { "renderTo", l_lovrCanvasRenderTo },
  { "getFormat", l_lovrCanvasGetFormat },
  { "getMSAA", l_lovrCanvasGetMSAA },
  { NULL, NULL }
};
