#include "api.h"
#include "graphics/graphics.h"
#include "util.h"
#include <stdlib.h>

static int l_lovrShaderClone(lua_State* L) {
  Shader* shader = luax_checktype(L, 1, Shader);
  luaL_checktype(L, 2, LUA_TTABLE);
  lua_pushnil(L);

  arr_t(ShaderFlag) flags;
  arr_init(&flags);

  while (lua_next(L, 2) != 0) {
    ShaderFlag flag = { 0 };
    flag.value = lua_isboolean(L, -1) ? (double) lua_toboolean(L, -1) : lua_tonumber(L, -1);
    switch (lua_type(L, -2)) {
      case LUA_TSTRING: flag.name = lua_tostring(L, -2); break;
      case LUA_TNUMBER: flag.id = lua_tointeger(L, -2); break;
      default:
        arr_free(&flags);
        luaL_error(L, "Unexpected ShaderFlag key type (%s)", lua_typename(L, lua_type(L, -2)));
    }
    arr_push(&flags, flag);
    lua_pop(L, 1);
  }

  if (flags.length >= 1000) {
    arr_free(&flags);
    luaL_error(L, "Too many Shader flags");
  }

  Shader* clone = lovrShaderClone(shader, flags.data, (uint32_t) flags.length);
  arr_free(&flags);
  luax_assert(L, clone);
  luax_pushtype(L, Shader, clone);
  lovrRelease(clone, lovrShaderDestroy);
  return 1;
}

static int l_lovrShaderGetLabel(lua_State* L) {
  Shader* shader = luax_checktype(L, 1, Shader);
  const ShaderInfo* info = lovrShaderGetInfo(shader);
  lua_pushstring(L, info->label);
  return 1;
}

static int l_lovrShaderGetType(lua_State* L) {
  Shader* shader = luax_checktype(L, 1, Shader);
  const ShaderInfo* info = lovrShaderGetInfo(shader);
  luax_pushenum(L, ShaderType, info->type);
  return 1;
}

static int l_lovrShaderHasStage(lua_State* L) {
  Shader* shader = luax_checktype(L, 1, Shader);
  ShaderStage stage = luax_checkenum(L, 2, ShaderStage, NULL);
  bool present = lovrShaderHasStage(shader, stage);
  lua_pushboolean(L, present);
  return 1;
}

static int l_lovrShaderHasAttribute(lua_State* L) {
  Shader* shader = luax_checktype(L, 1, Shader);
  const char* name;
  uint32_t location;
  if (lua_type(L, 2) == LUA_TNUMBER) {
    location = luax_checku32(L, 2);
    name = NULL;
  } else {
    name = lua_tostring(L, 2);
    location = 0;
  }
  bool present = lovrShaderHasAttribute(shader, name, location);
  lua_pushboolean(L, present);
  return 1;
}

static int l_lovrShaderHasVariable(lua_State* L) {
  Shader* shader = luax_checktype(L, 1, Shader);
  const char* variable = luaL_checkstring(L, 2);
  bool present = lovrShaderHasVariable(shader, variable);
  lua_pushboolean(L, present);
  return 1;
}

static int l_lovrShaderGetWorkgroupSize(lua_State* L) {
  Shader* shader = luax_checktype(L, 1, Shader);

  if (!lovrShaderHasStage(shader, STAGE_COMPUTE)) {
    lua_pushnil(L);
    return 1;
  }

  uint32_t size[3];
  lovrShaderGetWorkgroupSize(shader, size);
  lua_pushinteger(L, size[0]);
  lua_pushinteger(L, size[1]);
  lua_pushinteger(L, size[2]);
  return 3;
}

static int l_lovrShaderGetBufferFormat(lua_State* L) {
  Shader* shader = luax_checktype(L, 1, Shader);
  const char* name = luaL_checkstring(L, 2);

  uint32_t fieldCount;
  const DataField* format = lovrShaderGetBufferFormat(shader, name, &fieldCount);

  if (!format) {
    lua_pushnil(L);
    return 1;
  }

  // If the buffer just has a single array in it, unwrap it
  if (format->fieldCount == 1 && format->fields->length > 0) {
    const DataField* array = format->fields;

    if (array->fields) { // Array of structs
      luax_pushbufferformat(L, array->fields, array->fieldCount);
    } else {
      lua_createtable(L, 1, 1);
      luax_pushenum(L, DataType, array->type);
      lua_rawseti(L, -2, 1);
    }

    lua_pushinteger(L, array->stride);
    lua_setfield(L, -2, "stride");

    if (array->length == ~0u) {
      lua_pushnil(L);
    } else {
      lua_pushinteger(L, array->length);
    }
  } else {
    luax_pushbufferformat(L, format->fields, format->fieldCount);
    lua_pushnil(L);
  }

  return 2;
}

const luaL_Reg lovrShader[] = {
  { "clone", l_lovrShaderClone },
  { "getLabel", l_lovrShaderGetLabel },
  { "getType", l_lovrShaderGetType },
  { "hasStage", l_lovrShaderHasStage },
  { "hasAttribute", l_lovrShaderHasAttribute },
  { "hasVariable", l_lovrShaderHasVariable },
  { "getWorkgroupSize", l_lovrShaderGetWorkgroupSize },
  { "getBufferFormat", l_lovrShaderGetBufferFormat },
  { NULL, NULL }
};
