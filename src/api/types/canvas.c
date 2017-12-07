#include "api/lovr.h"
#include "graphics/graphics.h"
#include "graphics/canvas.h"

int l_lovrCanvasRenderTo(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  lovrGraphicsPushView();
  lovrCanvasBind(canvas);
  lua_settop(L, 2);
  lua_call(L, 0, 0);
  lovrCanvasResolveMSAA(canvas);
  lovrGraphicsPopView();
  return 0;
}

const luaL_Reg lovrCanvas[] = {
  { "renderto", l_lovrCanvasRenderTo },
  { NULL, NULL }
};
