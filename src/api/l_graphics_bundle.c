#include "api/api.h"
#include "graphics/graphics.h"
#include "core/util.h"
#include <lua.h>
#include <lauxlib.h>

static int l_lovrBundleBind(lua_State* L) {
  Bundle* bundle = luax_checktype(L, 1, Bundle);
  uint32_t group = 0;
  uint32_t id = 0;
  uint32_t item = 0;

  size_t length;
  const char* name = lua_tolstring(L, 1, &length);
  int index;

  if (name) {
    uint64_t hash = hash64(name, length);
    Shader* shader = lovrBundleGetShader(bundle);
    bool exists = lovrShaderResolveName(shader, hash, &group, &id);
    lovrAssert(exists, "Active Shader has no variable named '%s'", name);
    lovrAssert(group == lovrBundleGetGroup(bundle), "Variable '%s' is not in this Bundle's group", name);
    index = 2;
  } else {
    if (lua_type(L, 1) != LUA_TNUMBER || lua_type(L, 2) != LUA_TNUMBER) {
      return luaL_error(L, "Expected a string or two integers");
    }
    group = lua_tointeger(L, 1);
    id = lua_tointeger(L, 2);
    index = 3;
  }

  if (lua_type(L, index) == LUA_TNUMBER) {
    item = lua_tointeger(L, index++) - 1;
  }

  Buffer* buffer = luax_totype(L, index, Buffer);
  Texture* texture = luax_totype(L, index, Texture);

  if (buffer) {
    uint32_t offset = lua_tointeger(L, ++index);
    uint32_t extent = lua_tointeger(L, ++index);
    lovrBundleBindBuffer(bundle, id, item, buffer, offset, extent);
    return 0;
  } else if (texture) {
    lovrBundleBindTexture(bundle, id, item, texture);
    return 0;
  } else {
    return luax_typeerror(L, index, "Buffer or Texture");
  }
}

const luaL_Reg lovrBundle[] = {
  { "bind", l_lovrBundleBind },
  { NULL, NULL }
};
