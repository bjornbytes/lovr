#include "texture.h"
#include "../graphics.h"
#include "../../util.h"

void luax_pushtexture(lua_State* L, Texture* texture) {
  if (texture == NULL) {
    lua_pushnil(L);
    return;
  }

  Texture** userdata = (Texture**) lua_newuserdata(L, sizeof(Texture*));
  luaL_getmetatable(L, "Texture");
  lua_setmetatable(L, -2);
  *userdata = texture;
}

Texture* luax_checktexture(lua_State* L, int index) {
  return *(Texture**) luaL_checkudata(L, index, "Texture");
}

int luax_destroytexture(lua_State* L) {
  Texture* texture = luax_checktexture(L, 1);
  lovrTextureDestroy(texture);
  return 0;
}

const luaL_Reg lovrTexture[] = {
  { "bind", l_lovrTextureBind },
  { "refresh", l_lovrTextureRefresh },
  { "getDimensions", l_lovrTextureGetDimensions },
  { "getFilter", l_lovrTextureGetFilter },
  { "getHeight", l_lovrTextureGetHeight },
  { "getWidth", l_lovrTextureGetWidth },
  { "getWrap", l_lovrTextureGetWrap },
  { "setFilter", l_lovrTextureSetFilter },
  { "setWrap", l_lovrTextureSetWrap },
  { NULL, NULL }
};

int l_lovrTextureBind(lua_State* L) {
  Texture* texture = luax_checktexture(L, 1);
  lovrTextureBind(texture);
  return 0;
}

int l_lovrTextureRefresh(lua_State* L) {
  Texture* texture = luax_checktexture(L, 1);
  lovrTextureRefresh(texture);
  return 0;
}

int l_lovrTextureGetDimensions(lua_State* L) {
  Texture* texture = luax_checktexture(L, 1);
  lua_pushnumber(L, lovrTextureGetWidth(texture));
  lua_pushnumber(L, lovrTextureGetHeight(texture));
  return 2;
}

int l_lovrTextureGetFilter(lua_State* L) {
  Texture* texture = luax_checktexture(L, 1);
  FilterMode min, mag;
  lovrTextureGetFilter(texture, &min, &mag);
  lua_pushstring(L, map_int_find(&FilterModes, min));
  lua_pushstring(L, map_int_find(&FilterModes, mag));
  return 2;
}

int l_lovrTextureGetHeight(lua_State* L) {
  Texture* texture = luax_checktexture(L, 1);
  lua_pushnumber(L, lovrTextureGetHeight(texture));
  return 1;
}

int l_lovrTextureGetWidth(lua_State* L) {
  Texture* texture = luax_checktexture(L, 1);
  lua_pushnumber(L, lovrTextureGetWidth(texture));
  return 1;
}

int l_lovrTextureGetWrap(lua_State* L) {
  Texture* texture = luax_checktexture(L, 1);
  WrapMode horizontal, vertical;
  lovrTextureGetWrap(texture, &horizontal, &vertical);
  lua_pushstring(L, map_int_find(&WrapModes, horizontal));
  lua_pushstring(L, map_int_find(&WrapModes, vertical));
  return 2;
}

int l_lovrTextureSetFilter(lua_State* L) {
  Texture* texture = luax_checktexture(L, 1);
  FilterMode* min = (FilterMode*) luax_checkenum(L, 2, &FilterModes, "filter mode");
  FilterMode* mag = (FilterMode*) luax_optenum(L, 3, luaL_checkstring(L, 2), &FilterModes, "filter mode");
  lovrTextureSetFilter(texture, *min, *mag);
  return 0;
}

int l_lovrTextureSetWrap(lua_State* L) {
  Texture* texture = luax_checktexture(L, 1);
  WrapMode* horizontal = (WrapMode*) luax_checkenum(L, 2, &WrapModes, "wrap mode");
  WrapMode* vertical = (WrapMode*) luax_optenum(L, 3, luaL_checkstring(L, 2), &WrapModes, "wrap mode");
  lovrTextureSetWrap(texture, *horizontal, *vertical);
  return 0;
}
