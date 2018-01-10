#include "api.h"
#include "graphics/graphics.h"
#include "graphics/shader.h"
#include "math/transform.h"

struct TempData {
  void* data;
  size_t size;
};

static struct TempData tempData;

int l_lovrShaderHasUniform(lua_State* L) {
  Shader* shader = luax_checktype(L, 1, Shader);
  const char* name = luaL_checkstring(L, 2);
  lua_pushboolean(L, lovrShaderGetUniform(shader, name) != NULL);
  return 1;
}

int l_lovrShaderSend(lua_State* L) {
  Shader* shader = luax_checktype(L, 1, Shader);
  const char* name = luaL_checkstring(L, 2);
  lua_settop(L, 3);

  Uniform* uniform = lovrShaderGetUniform(shader, name);

  if (!uniform) {
    return luaL_error(L, "Unknown shader variable '%s'", name);
  }

  if (tempData.size < uniform->size) {
    tempData.size = uniform->size;
    tempData.data = realloc(tempData.data, tempData.size);
  }

  int* ints = (int*) tempData.data;
  float* floats = (float*) tempData.data;
  Texture** textures = (Texture**) tempData.data;

  int n = 1;
  int components = uniform->components;

  if (components > 1) {
    luaL_checktype(L, 3, LUA_TTABLE);
    lua_rawgeti(L, 3, 1);
    if (!lua_istable(L, -1)) {
      lua_newtable(L);
      lua_pushvalue(L, 3);
      lua_rawseti(L, -2, 1);
    } else {
      lua_pop(L, 1);
    }

    n = lua_objlen(L, -1);
    if (n != uniform->count) {
      const char* elements = uniform->count == 1 ? "element" : "elements";
      return luaL_error(L, "Expected %d %s for array '%s', got %d", uniform->count, elements, uniform->name, n);
    }
  }

  switch (uniform->type) {
    case UNIFORM_FLOAT:
      if (components == 1) {
        floats[0] = luaL_checknumber(L, 3);
      } else {
        luaL_checktype(L, 3, LUA_TTABLE);
        for (int i = 0; i < n; i++) {
          lua_rawgeti(L, -1, i + 1);
          if (lua_type(L, -1) != LUA_TTABLE || (int) lua_objlen(L, -1) != components) {
            return luaL_error(L, "Expected %d components for uniform '%s' #%d, got %d", components, uniform->name, lua_objlen(L, -1));
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
      if (components == 1) {
        ints[0] = luaL_checkinteger(L, 3);
      } else {
        luaL_checktype(L, 3, LUA_TTABLE);
        for (int i = 0; i < n; i++) {
          lua_rawgeti(L, -1, i + 1);
          if (lua_type(L, -1) != LUA_TTABLE || (int) lua_objlen(L, -1) != components) {
            return luaL_error(L, "Expected %d components for uniform '%s' #%d, got %d", components, uniform->name, lua_objlen(L, -1));
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
      if (components == 4 && lua_isuserdata(L, 3)) {
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
        textures[0] = luax_checktypeof(L, 3, Texture);
      } else {
        luaL_checktype(L, 3, LUA_TTABLE);
        for (int i = 0; i < n; i++) {
          lua_rawgeti(L, -1, i + 1);
          textures[i] = luax_checktypeof(L, -1, Texture);
          lua_pop(L, 1);
        }
      }
      lovrShaderSetTexture(shader, name, textures, n);
      break;

    default: break;
  }

  return 0;
}

const luaL_Reg lovrShader[] = {
  { "hasUniform", l_lovrShaderHasUniform },
  { "send", l_lovrShaderSend },
  { NULL, NULL }
};
