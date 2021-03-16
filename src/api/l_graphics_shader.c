#include "api.h"
#include "graphics/buffer.h"
#include "graphics/shader.h"
#include "data/blob.h"
#include "core/maf.h"
#include <lua.h>
#include <lauxlib.h>
#include <stdlib.h>

struct TempData {
  void* data;
  int size;
};

// Not thread safe
static struct TempData tempData;

int luax_checkuniform(lua_State* L, int index, const Uniform* uniform, void* dest, const char* debug) {
  Blob* blob = luax_totype(L, index, Blob);
  UniformType uniformType = uniform->type;
  int components = uniform->components;
  int count = uniform->count;

  if (uniformType == UNIFORM_MATRIX) {
    components *= components;
  }

  if (blob) {
    size_t elements = count * components;
    const char* s = elements == 1 ? "" : "s";
    size_t capacity;

    switch (uniformType) {
      case UNIFORM_FLOAT:
      case UNIFORM_MATRIX:
        capacity = blob->size / sizeof(float);
        lovrAssert(capacity >= elements, "Blob can only hold %d float%s, at least %d needed for uniform '%s'", capacity, s, elements, debug);
        memcpy(dest, blob->data, elements * sizeof(float));
        break;

      case UNIFORM_INT:
        capacity = blob->size / sizeof(int);
        lovrAssert(capacity >= elements, "Blob can only hold %d int%s, at least %d needed for uniform '%s'", capacity, s, elements, debug);
        memcpy(dest, blob->data, elements * sizeof(int));
        break;

      case UNIFORM_SAMPLER: lovrThrow("Sampler uniform '%s' can not be updated with a Blob", debug);
      case UNIFORM_IMAGE: lovrThrow("Image uniform '%s' can not be updated with a Blob", debug);
    }

    return 0;
  }

  if (components == 1) {
    bool isTable = lua_istable(L, index);
    int length = isTable ? luax_len(L, index) : count;
    length = MIN(length, count);
    for (int i = 0; i < count; i++) {
      int j = index + i;
      if (isTable) {
        lua_rawgeti(L, index, i + 1);
        j = -1;
      }

      switch (uniformType) {
        case UNIFORM_FLOAT: *((float*) dest + i) = luax_optfloat(L, j, 0.f); break;
        case UNIFORM_INT: *((int*) dest + i) = luaL_optinteger(L, j, 0); break;
        case UNIFORM_SAMPLER: {
          *((Texture**) dest + i) = luax_checktype(L, j, Texture);
          TextureType type = lovrTextureGetType(*((Texture**) dest + i));
          lovrAssert(type == uniform->textureType, "Attempt to send %s texture to %s sampler uniform", lovrTextureType[type].string, lovrTextureType[uniform->textureType].string);
          break;
        }

        case UNIFORM_IMAGE: {
          StorageImage* image = (StorageImage*) dest + i;
          image->texture = luax_checktype(L, j, Texture);
          image->slice = -1;
          image->mipmap = 0;
          image->access = ACCESS_READ_WRITE;
          TextureType type = lovrTextureGetType(image->texture);
          lovrAssert(type == uniform->textureType, "Attempt to send %s texture to %s storage image uniform", lovrTextureType[type], lovrTextureType[uniform->textureType]);
          break;
        }

        default: break;
      }

      if (isTable) {
        lua_pop(L, 1);
      }
    }
  } else {
    bool wrappedTable = false;

    if (lua_istable(L, index)) {
      lua_rawgeti(L, index, 1);
      wrappedTable = !lua_isnumber(L, -1);
      lua_pop(L, 1);
    }

    if (wrappedTable) {
      int length = luax_len(L, index);
      length = MIN(length, count);
      for (int i = 0; i < length; i++) {
        lua_rawgeti(L, index, i + 1);

        if (uniformType == UNIFORM_MATRIX && components == 16) {
          VectorType type;
          mat4 m = luax_tovector(L, -1, &type);
          if (m && type == V_MAT4) {
            mat4_init((float*) dest + i * components, m);
            lua_pop(L, 1);
            continue;
          }
        } else if (uniformType == UNIFORM_FLOAT && components == 3) {
          VectorType type;
          vec3 v = luax_tovector(L, -1, &type);
          if (v && type == V_VEC3) {
            vec3_init((float*) dest + i * components, v);
            lua_pop(L, 1);
            continue;
          }
        }

        luaL_checktype(L, -1, LUA_TTABLE);
        for (int j = 0; j < components; j++) {
          lua_rawgeti(L, -1, j + 1);
          switch (uniformType) {
            case UNIFORM_FLOAT:
            case UNIFORM_MATRIX:
              *((float*) dest + i * components + j) = luax_optfloat(L, -1, 0.f);
              break;

            case UNIFORM_INT:
              *((int*) dest + i * components + j) = luaL_optinteger(L, -1, 0);
              break;

            case UNIFORM_SAMPLER:
            case UNIFORM_IMAGE:
              lovrThrow("Unreachable");
          }
          lua_pop(L, 1);
        }
        lua_pop(L, 1);
      }
    } else {
      for (int i = 0; i < count; i++) {
        if (uniformType == UNIFORM_MATRIX && components == 16) {
          VectorType type;
          mat4 m = luax_tovector(L, index + i, &type);
          if (m && type == V_MAT4) {
            mat4_init((float*) dest + i * components, m);
            continue;
          }
        } else if (uniformType == UNIFORM_FLOAT && components == 3) {
          VectorType type;
          vec3 v = luax_tovector(L, index + i, &type);
          if (v && type == V_VEC3) {
            vec3_init((float*) dest + i * components, v);
            continue;
          }
        }

        luaL_checktype(L, index + i, LUA_TTABLE);
        for (int j = 0; j < components; j++) {
          lua_rawgeti(L, index + i, j + 1);
          switch (uniformType) {
            case UNIFORM_FLOAT:
            case UNIFORM_MATRIX:
              *((float*) dest + i * components + j) = luax_optfloat(L, -1, 0.f);
              break;

            case UNIFORM_INT:
              *((int*) dest + i * components + j) = luaL_optinteger(L, -1, 0);
              break;

            case UNIFORM_SAMPLER:
            case UNIFORM_IMAGE:
              lovrThrow("Unreachable");
          }
        }
      }
    }
  }

  return 0;
}

static int l_lovrShaderGetType(lua_State* L) {
  Shader* shader = luax_checktype(L, 1, Shader);
  luax_pushenum(L, ShaderType, lovrShaderGetType(shader));
  return 1;
}

static int l_lovrShaderHasUniform(lua_State* L) {
  Shader* shader = luax_checktype(L, 1, Shader);
  const char* name = luaL_checkstring(L, 2);
  lua_pushboolean(L, lovrShaderHasUniform(shader, name));
  return 1;
}

static int l_lovrShaderHasBlock(lua_State* L) {
  Shader* shader = luax_checktype(L, 1, Shader);
  const char* name = luaL_checkstring(L, 2);
  lua_pushboolean(L, lovrShaderHasBlock(shader, name));
  return 1;
}

static int l_lovrShaderSend(lua_State* L) {
  Shader* shader = luax_checktype(L, 1, Shader);
  const char* name = luaL_checkstring(L, 2);
  const Uniform* uniform = lovrShaderGetUniform(shader, name);
  if (!uniform) {
    lua_pushboolean(L, false);
    return 1;
  }

  if (tempData.size < uniform->size) {
    tempData.size = uniform->size;
    tempData.data = realloc(tempData.data, tempData.size);
  }

  luax_checkuniform(L, 3, uniform, tempData.data, name);
  switch (uniform->type) {
    case UNIFORM_FLOAT: lovrShaderSetFloats(shader, uniform->name, tempData.data, 0, uniform->count * uniform->components); break;
    case UNIFORM_INT: lovrShaderSetInts(shader, uniform->name, tempData.data, 0, uniform->count * uniform->components); break;
    case UNIFORM_MATRIX: lovrShaderSetMatrices(shader, uniform->name, tempData.data, 0, uniform->count * uniform->components * uniform->components); break;
    case UNIFORM_SAMPLER: lovrShaderSetTextures(shader, uniform->name, tempData.data, 0, uniform->count); break;
    case UNIFORM_IMAGE: lovrShaderSetImages(shader, uniform->name, tempData.data, 0, uniform->count); break;
  }
  lua_pushboolean(L, true);
  return 1;
}

static int l_lovrShaderSendBlock(lua_State* L) {
  Shader* shader = luax_checktype(L, 1, Shader);
  const char* name = luaL_checkstring(L, 2);
  lovrAssert(lovrShaderHasBlock(shader, name), "Unknown shader block '%s'", name);
  ShaderBlock* block = luax_checktype(L, 3, ShaderBlock);
  UniformAccess access = luax_checkenum(L, 4, UniformAccess, "readwrite");
  Buffer* buffer = lovrShaderBlockGetBuffer(block);
  lovrShaderSetBlock(shader, name, buffer, 0, lovrBufferGetSize(buffer), access);
  return 0;
}

static int l_lovrShaderSendImage(lua_State* L) {
  int index = 1;
  Shader* shader = luax_checktype(L, index++, Shader);
  const char* name = luaL_checkstring(L, index++);

  int start = 0;
  if (lua_type(L, index) == LUA_TNUMBER) {
    start = lua_tointeger(L, index++);
  }

  Texture* texture = luax_checktype(L, index++, Texture);
  int slice = luaL_optinteger(L, index++, 0) - 1; // Default is -1
  int mipmap = luax_optmipmap(L, index++, texture);
  UniformAccess access = luax_checkenum(L, index++, UniformAccess, "readwrite");
  StorageImage image = { .texture = texture, .slice = slice, .mipmap = mipmap, .access = access };
  lovrShaderSetImages(shader, name, &image, start, 1);
  return 0;
}

const luaL_Reg lovrShader[] = {
  { "getType", l_lovrShaderGetType },
  { "hasUniform", l_lovrShaderHasUniform },
  { "hasBlock", l_lovrShaderHasBlock },
  { "send", l_lovrShaderSend },
  { "sendBlock", l_lovrShaderSendBlock },
  { "sendImage", l_lovrShaderSendImage },
  { NULL, NULL }
};
