#include "api.h"
#include "graphics/shader.h"
#include "math/transform.h"

struct TempData {
  void* data;
  int size;
};

static struct TempData tempData;

int l_lovrShaderHasUniform(lua_State* L) {
  Shader* shader = luax_checktype(L, 1, Shader);
  const char* name = luaL_checkstring(L, 2);
  lua_pushboolean(L, lovrShaderHasUniform(shader, name));
  return 1;
}

int l_lovrShaderSend(lua_State* L) {
  Shader* shader = luax_checktype(L, 1, Shader);
  const char* name = luaL_checkstring(L, 2);
  lua_settop(L, 3);

  int count;
  int components;
  int size;
  UniformType type;
  bool present = lovrShaderGetUniform(shader, name, &count, &components, &size, &type);

  if (!present) {
    return luaL_error(L, "Unknown shader variable '%s'", name);
  }

  if (tempData.size < size) {
    tempData.size = size;
    tempData.data = realloc(tempData.data, tempData.size);
  }

  int* ints = (int*) tempData.data;
  float* floats = (float*) tempData.data;
  Texture** textures = (Texture**) tempData.data;
  int n = 1;

  if (components > 1 && lua_istable(L, 3)) {
    lua_rawgeti(L, 3, 1);
    if (!lua_istable(L, -1)) {
      lua_newtable(L);
      lua_pushvalue(L, 3);
      lua_rawseti(L, -2, 1);
    } else {
      lua_pop(L, 1);
    }

    n = lua_objlen(L, -1);
    if (n != count) {
      return luaL_error(L, "Expected %d element%s for array '%s', got %d", count, count == 1 ? "" : "s", name, n);
    }
  }

  Blob* blob = luax_totype(L, 3, Blob);

  switch (type) {
    case UNIFORM_FLOAT:
      if (blob) {
        n = count;
        floats = (float*) blob->data;
        size_t count = n * components;
        size_t capacity = blob->size / sizeof(float);
        const char* s = capacity == 1 ? "" : "s";
        lovrAssert(capacity >= count, "Blob can only hold %d float%s, at least %d needed", capacity, s, count);
      } else if (components == 1) {
        floats[0] = luaL_checknumber(L, 3);
      } else {
        luaL_checktype(L, 3, LUA_TTABLE);
        for (int i = 0; i < n; i++) {
          lua_rawgeti(L, -1, i + 1);
          if (lua_type(L, -1) != LUA_TTABLE || (int) lua_objlen(L, -1) != components) {
            return luaL_error(L, "Expected %d components for uniform '%s' #%d, got %d", components, name, lua_objlen(L, -1));
          }
          for (int j = 0; j < components; j++) {
            lua_rawgeti(L, -1, j + 1);
            floats[i * components + j] = luaL_checknumber(L, -1);
            lua_pop(L, 1);
          }
          lua_pop(L, 1);
        }
      }
      lovrShaderSetFloat(shader, name, floats, n * components);
      break;

    case UNIFORM_INT:
      if (blob) {
        n = count;
        ints = (int*) blob->data;
        size_t count = n * components;
        size_t capacity = blob->size / sizeof(int);
        const char* s = capacity == 1 ? "" : "s";
        lovrAssert(capacity >= count, "Blob can only hold %d int%s, at least %d needed", capacity, s, count);
      } else if (components == 1) {
        ints[0] = luaL_checkinteger(L, 3);
      } else {
        luaL_checktype(L, 3, LUA_TTABLE);
        for (int i = 0; i < n; i++) {
          lua_rawgeti(L, -1, i + 1);
          if (lua_type(L, -1) != LUA_TTABLE || (int) lua_objlen(L, -1) != components) {
            return luaL_error(L, "Expected %d components for uniform '%s' #%d, got %d", components, name, lua_objlen(L, -1));
          }
          for (int j = 0; j < components; j++) {
            lua_rawgeti(L, -1, j + 1);
            ints[i * components + j] = luaL_checkinteger(L, -1);
            lua_pop(L, 1);
          }
          lua_pop(L, 1);
        }
      }
      lovrShaderSetInt(shader, name, ints, n * components);
      break;

    case UNIFORM_MATRIX:
      if (blob) {
        n = count;
        floats = (float*) blob->data;
        size_t count = n * components * components;
        size_t capacity = blob->size / sizeof(float);
        const char* s = capacity == 1 ? "x" : "ces";
        lovrAssert(capacity >= count, "Blob can only hold %d matri%s, at least %d needed", capacity, s, count);
      } else if (components == 4 && lua_isuserdata(L, 3)) {
        Transform* transform = luax_checktype(L, 3, Transform);
        memcpy(floats, transform->matrix, 16 * sizeof(float));
      } else {
        luaL_checktype(L, 3, LUA_TTABLE);
        for (int i = 0; i < n; i++) {
          lua_rawgeti(L, -1, i + 1);
          for (int j = 0; j < components * components; j++) {
            lua_rawgeti(L, -1, j + 1);
            floats[i * components * components + j] = luaL_checknumber(L, -1);
            lua_pop(L, 1);
          }
          lua_pop(L, 1);
        }
      }
      lovrShaderSetMatrix(shader, name, floats, n * components * components);
      break;

    case UNIFORM_SAMPLER:
      if (components == 1) {
        textures[0] = luax_checktype(L, 3, Texture);
      } else {
        luaL_checktype(L, 3, LUA_TTABLE);
        for (int i = 0; i < n; i++) {
          lua_rawgeti(L, -1, i + 1);
          textures[i] = luax_checktype(L, -1, Texture);
          lua_pop(L, 1);
        }
      }
      lovrShaderSetTexture(shader, name, textures, n);
      break;

    default: break;
  }

  return 0;
}

int l_lovrShaderGetBlock(lua_State* L) {
  Shader* shader = luax_checktype(L, 1, Shader);
  const char* name = luaL_checkstring(L, 2);
  ShaderBlock* block = lovrShaderGetBlock(shader, name);
  luax_pushobject(L, block);
  return 1;
}

int l_lovrShaderSetBlock(lua_State* L) {
  Shader* shader = luax_checktype(L, 1, Shader);
  const char* name = luaL_checkstring(L, 2);
  ShaderBlock* block = luax_checktype(L, 3, ShaderBlock);
  lovrShaderSetBlock(shader, name, block);
  return 0;
}

const luaL_Reg lovrShader[] = {
  { "hasUniform", l_lovrShaderHasUniform },
  { "send", l_lovrShaderSend },
  { "getBlock", l_lovrShaderGetBlock },
  { "setBlock", l_lovrShaderSetBlock },
  { NULL, NULL }
};
