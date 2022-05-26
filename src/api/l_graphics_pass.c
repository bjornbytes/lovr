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

static int l_lovrPassSetAlphaToCoverage(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  lovrPassSetAlphaToCoverage(pass, lua_toboolean(L, 2));
  return 1;
}

static int l_lovrPassSetBlendMode(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  BlendMode mode = lua_isnoneornil(L, 2) ? BLEND_NONE : luax_checkenum(L, 1, BlendMode, NULL);
  BlendAlphaMode alphaMode = luax_checkenum(L, 3, BlendAlphaMode, "alphamultiply");
  lovrPassSetBlendMode(pass, mode, alphaMode);
  return 0;
}

static int l_lovrPassSetColor(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  float color[4];
  luax_readcolor(L, 2, color);
  lovrPassSetColor(pass, color);
  return 0;
}

static int l_lovrPassSetColorWrite(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  bool r, g, b, a;
  if (lua_gettop(L) <= 1) {
    r = g = b = a = lua_toboolean(L, 2);
  } else {
    r = lua_toboolean(L, 2);
    g = lua_toboolean(L, 3);
    b = lua_toboolean(L, 4);
    a = lua_toboolean(L, 5);
  }
  lovrPassSetColorWrite(pass, r, g, b, a);
  return 0;
}

static int l_lovrPassSetCullMode(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  CullMode mode = luax_checkenum(L, 2, CullMode, "none");
  lovrPassSetCullMode(pass, mode);
  return 0;
}

static int l_lovrPassSetDepthTest(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  CompareMode test = lua_isnoneornil(L, 2) ? COMPARE_NONE : luax_checkenum(L, 2, CompareMode, NULL);
  lovrPassSetDepthTest(pass, test);
  return 0;
}

static int l_lovrPassSetDepthWrite(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  bool write = lua_toboolean(L, 2);
  lovrPassSetDepthWrite(pass, write);
  return 0;
}

static int l_lovrPassSetDepthOffset(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  float offset = luax_optfloat(L, 2, 0.f);
  float sloped = luax_optfloat(L, 3, 0.f);
  lovrPassSetDepthOffset(pass, offset, sloped);
  return 0;
}

static int l_lovrPassSetDepthClamp(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  bool clamp = lua_toboolean(L, 2);
  lovrPassSetDepthClamp(pass, clamp);
  return 0;
}

static int l_lovrPassSetShader(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  switch (lua_type(L, 2)) {
    case LUA_TNONE: case LUA_TNIL: lovrPassSetShader(pass, NULL); return 0;
    default: lovrPassSetShader(pass, luax_checktype(L, 2, Shader)); return 0;
  }
}

static int l_lovrPassSetStencilTest(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  if (lua_isnoneornil(L, 2)) {
    lovrPassSetStencilTest(pass, COMPARE_NONE, 0, 0xff);
  } else {
    CompareMode test = luax_checkenum(L, 2, CompareMode, NULL);
    uint8_t value = luaL_checkinteger(L, 3);
    uint8_t mask = luaL_optinteger(L, 4, 0xff);
    lovrPassSetStencilTest(pass, test, value, mask);
  }
  return 0;
}

static int l_lovrPassSetStencilWrite(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  StencilAction actions[3];
  if (lua_isnoneornil(L, 2)) {
    actions[0] = actions[1] = actions[2] = STENCIL_KEEP;
    lovrPassSetStencilWrite(pass, actions, 0, 0xff);
  } else {
    if (lua_istable(L, 2)) {
      lua_rawgeti(L, 2, 1);
      lua_rawgeti(L, 2, 2);
      lua_rawgeti(L, 2, 3);
      actions[0] = luax_checkenum(L, -3, StencilAction, NULL);
      actions[1] = luax_checkenum(L, -2, StencilAction, NULL);
      actions[2] = luax_checkenum(L, -1, StencilAction, NULL);
      lua_pop(L, 3);
    } else {
      actions[0] = actions[1] = actions[2] = luax_checkenum(L, 2, StencilAction, NULL);
    }
    uint8_t value = luaL_optinteger(L, 3, 1);
    uint8_t mask = luaL_optinteger(L, 4, 0xff);
    lovrPassSetStencilWrite(pass, actions, value, mask);
  }
  return 0;
}

static int l_lovrPassSetWinding(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  Winding winding = luax_checkenum(L, 2, Winding, NULL);
  lovrPassSetWinding(pass, winding);
  return 0;
}

static int l_lovrPassSetWireframe(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  bool wireframe = lua_toboolean(L, 2);
  lovrPassSetWireframe(pass, wireframe);
  return 0;
}

static int l_lovrPassSend(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  const char* name = NULL;
  size_t length = 0;
  uint32_t slot = ~0u;

  switch (lua_type(L, 2)) {
    case LUA_TSTRING: name = lua_tolstring(L, 2, &length); break;
    case LUA_TNUMBER: slot = lua_tointeger(L, 2) - 1; break;
    default: return luax_typeerror(L, 2, "string or number");
  }

  Buffer* buffer = luax_totype(L, 3, Buffer);

  if (buffer) {
    uint32_t offset = lua_tointeger(L, 4);
    uint32_t extent = lua_tointeger(L, 5);
    lovrPassSendBuffer(pass, name, length, slot, buffer, offset, extent);
    return 0;
  }

  Texture* texture = luax_totype(L, 4, Texture);

  if (texture) {
    lovrPassSendTexture(pass, name, length, slot, texture);
    return 0;
  }

  Sampler* sampler = luax_totype(L, 4, Sampler);

  if (sampler) {
    lovrPassSendSampler(pass, name, length, slot, sampler);
    return 0;
  }

  return luax_typeerror(L, 3, "Buffer, Texture, Sampler, or number/vector");
}

static int l_lovrPassClear(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);

  Buffer* buffer = luax_totype(L, 2, Buffer);

  if (buffer) {
    uint32_t offset = luax_optu32(L, 3, 0);
    uint32_t extent = luax_optu32(L, 4, ~0u);
    lovrPassClearBuffer(pass, buffer, offset, extent);
    return 0;
  }

  Texture* texture = luax_totype(L, 2, Texture);

  if (texture) {
    float value[4];
    luax_readcolor(L, 3, value);
    int index = lua_istable(L, 3) ? 4 : 6;
    uint32_t layer = luax_optu32(L, index++, 1) - 1;
    uint32_t layerCount = luax_optu32(L, index++, ~0u);
    uint32_t level = luax_optu32(L, index++, 1) - 1;
    uint32_t levelCount = luax_optu32(L, index++, ~0u);
    lovrPassClearTexture(pass, texture, value, layer, layerCount, level, levelCount);
    return 0;
  }

  return luax_typeerror(L, 2, "Buffer or Texture");
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
  { "setAlphaToCoverage", l_lovrPassSetAlphaToCoverage },
  { "setBlendMode", l_lovrPassSetBlendMode },
  { "setColor", l_lovrPassSetColor },
  { "setColorWrite", l_lovrPassSetColorWrite },
  { "setCullMode", l_lovrPassSetCullMode },
  { "setDepthTest", l_lovrPassSetDepthTest },
  { "setDepthWrite", l_lovrPassSetDepthWrite },
  { "setDepthOffset", l_lovrPassSetDepthOffset },
  { "setDepthClamp", l_lovrPassSetDepthClamp },
  { "setShader", l_lovrPassSetShader },
  { "setStencilTest", l_lovrPassSetStencilTest },
  { "setStencilWrite", l_lovrPassSetStencilWrite },
  { "setWinding", l_lovrPassSetWinding },
  { "setWireframe", l_lovrPassSetWireframe },
  { "send", l_lovrPassSend },
  { "clear", l_lovrPassClear },
  { NULL, NULL }
};
