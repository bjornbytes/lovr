#include "api.h"
#include "graphics/graphics.h"
#include "util.h"
#include <lua.h>
#include <lauxlib.h>

static int l_lovrPassGetType(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  const PassInfo* info = lovrPassGetInfo(pass);
  luax_pushenum(L, PassType, info->type);
  return 1;
}

static int l_lovrPassPush(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  StackType stack = luax_checkenum(L, 2, StackType, "transform");
  lovrPassPush(pass, stack);
  return 0;
}

static int l_lovrPassPop(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  StackType stack = luax_checkenum(L, 2, StackType, "transform");
  lovrPassPop(pass, stack);
  return 0;
}

static int l_lovrPassOrigin(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  lovrPassOrigin(pass);
  return 0;
}

static int l_lovrPassTranslate(lua_State* L) {
  float translation[4];
  Pass* pass = luax_checktype(L, 2, Pass);
  luax_readvec3(L, 1, translation, NULL);
  lovrPassTranslate(pass, translation);
  return 0;
}

static int l_lovrPassRotate(lua_State* L) {
  float rotation[4];
  Pass* pass = luax_checktype(L, 1, Pass);
  luax_readquat(L, 2, rotation, NULL);
  lovrPassRotate(pass, rotation);
  return 0;
}

static int l_lovrPassScale(lua_State* L) {
  float scale[4];
  Pass* pass = luax_checktype(L, 1, Pass);
  luax_readscale(L, 2, scale, 3, NULL);
  lovrPassScale(pass, scale);
  return 0;
}

static int l_lovrPassTransform(lua_State* L) {
  float transform[16];
  Pass* pass = luax_checktype(L, 1, Pass);
  luax_readmat4(L, 2, transform, 3);
  lovrPassTransform(pass, transform);
  return 0;
}

const luaL_Reg lovrPass[] = {
  { "getType", l_lovrPassGetType },
  { "push", l_lovrPassPush },
  { "pop", l_lovrPassPop },
  { "origin", l_lovrPassOrigin },
  { "translate", l_lovrPassTranslate },
  { "rotate", l_lovrPassRotate },
  { "scale", l_lovrPassScale },
  { "transform", l_lovrPassTransform },
  { NULL, NULL }
};
