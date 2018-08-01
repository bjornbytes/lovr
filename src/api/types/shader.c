#include "api.h"
#include "graphics/shader.h"
#include "math/transform.h"

struct TempData {
  void* data;
  int size;
};

static struct TempData tempData;

int luax_checkuniform(lua_State* L, int index, const Uniform* uniform, void* dest, const char* debug) {
  Blob* blob = luax_totype(L, index, Blob);
  int components = uniform->components;
  int count = uniform->count;

  if (uniform->type == UNIFORM_MATRIX) {
    components *= components;
  }

  if (blob) {
    size_t elements = count * components;
    const char* s = elements == 1 ? "" : "s";
    size_t capacity;

    switch (uniform->type) {
      case UNIFORM_FLOAT:
      case UNIFORM_MATRIX:
        capacity = blob->size / sizeof(float);
        lovrAssert(capacity >= elements, "Blob can only hold %d float%s, at least %d needed", capacity, s, elements);
        memcpy(dest, blob->data, elements * sizeof(float));
        break;

      case UNIFORM_INT:
        capacity = blob->size / sizeof(int);
        lovrAssert(capacity >= elements, "Blob can only hold %d int%s, at least %d needed", capacity, s, elements);
        memcpy(dest, blob->data, elements * sizeof(int));
        break;

      default: lovrThrow(""); // TODO
    }

    return 0;
  }

  if (components == 1) {
    bool isTable = lua_istable(L, index);
    if (isTable) {
      int length = lua_objlen(L, index);
      lovrAssert(length == count, ""); // TODO
      for (int i = 0; i < length; i++) {
        lua_rawgeti(L, index, i + 1);
        switch (uniform->type) {
          case UNIFORM_FLOAT: *((float*) dest + i) = luaL_checknumber(L, -1); break;
          case UNIFORM_INT: *((int*) dest + i) = luaL_checkinteger(L, -1); break;
          case UNIFORM_SAMPLER: *((Texture**) dest + i) = luax_checktype(L, -1, Texture); break;
          default: break;
        }
        lua_pop(L, 1);
      }
    } else {
      for (int i = 0; i < count; i++) {
        switch (uniform->type) {
          case UNIFORM_FLOAT: *((float*) dest + i) = luaL_checknumber(L, index + i); break;
          case UNIFORM_INT: *((int*) dest + i) = luaL_checkinteger(L, index + i); break;
          case UNIFORM_SAMPLER: *((Texture**) dest + i) = luax_checktype(L, index + i, Texture); break;
          default: break;
        }
      }
    }
  } else {
    luaL_checktype(L, index, LUA_TTABLE);
    lua_rawgeti(L, index, 1);
    bool wrappedTable = lua_istable(L, -1) || luax_totype(L, -1, Transform);
    lua_pop(L, 1);

    if (wrappedTable) {
      int length = lua_objlen(L, index);
      lovrAssert(length == count, ""); // TODO
      for (int i = 0; i < length; i++) {
        lua_rawgeti(L, index, i + 1);

        if (uniform->type == UNIFORM_MATRIX && lua_isuserdata(L, -1)) {
          Transform* transform = luax_checktype(L, -1, Transform);
          memcpy((float*) dest + i * components, transform->matrix, 16 * sizeof(float));
          continue;
        }

        for (int j = 0; j < components; j++) {
          lua_rawgeti(L, -1, j + 1);
          switch (uniform->type) {
            case UNIFORM_FLOAT:
            case UNIFORM_MATRIX:
              *((float*) dest + i * components + j) = luaL_checknumber(L, -1);

            case UNIFORM_INT:
              *((int*) dest + i * components + j) = luaL_checkinteger(L, -1);
              break;

            default:
              lovrThrow(""); // TODO
          }
          lua_pop(L, 1);
        }
        lua_pop(L, 1);
      }
    } else {
      for (int i = 0; i < count; i++) {
        if (uniform->type == UNIFORM_MATRIX && lua_isuserdata(L, index + i)) {
          Transform* transform = luax_checktype(L, index + i, Transform);
          memcpy((float*) dest + i * components, transform->matrix, 16 * sizeof(float));
          continue;
        }

        luaL_checktype(L, index + i, LUA_TTABLE);
        for (int j = 0; j < components; j++) {
          lua_rawgeti(L, index + i, j + 1);
          switch (uniform->type) {
            case UNIFORM_FLOAT:
            case UNIFORM_MATRIX:
              *((float*) dest + i * components + j) = luaL_checknumber(L, -1);
              break;

            case UNIFORM_INT:
              *((float*) dest + i * components + j) = luaL_checknumber(L, -1);
              break;

            default: lovrThrow(""); // TODO
          }
        }
      }
    }
  }

  return 0;
}

int l_lovrShaderHasUniform(lua_State* L) {
  Shader* shader = luax_checktype(L, 1, Shader);
  const char* name = luaL_checkstring(L, 2);
  lua_pushboolean(L, lovrShaderHasUniform(shader, name));
  return 1;
}

int l_lovrShaderSend(lua_State* L) {
  Shader* shader = luax_checktype(L, 1, Shader);
  const char* name = luaL_checkstring(L, 2);
  const Uniform* uniform = lovrShaderGetUniform(shader, name);
  lovrAssert(uniform, "Unknown shader variable '%s'", name);

  if (tempData.size < uniform->size) {
    tempData.size = uniform->size;
    tempData.data = realloc(tempData.data, tempData.size);
  }

  luax_checkuniform(L, 3, uniform, tempData.data, name);
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
