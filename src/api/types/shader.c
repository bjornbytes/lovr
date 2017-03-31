#include "api/lovr.h"
#include "graphics/shader.h"
#include "math/transform.h"

int l_lovrShaderSend(lua_State* L) {
  Shader* shader = luax_checktype(L, 1, Shader);
  const char* name = luaL_checkstring(L, 2);
  lua_settop(L, 3);

  int id = lovrShaderGetUniformId(shader, name);
  if (id == -1) {
    return luaL_error(L, "Unknown shader variable '%s'", name);
  }

  GLenum type;
  int size;
  lovrShaderGetUniformType(shader, name, &type, &size);
  lovrShaderBind(shader, shader->transform, shader->projection, shader->color, 0); // Hmm
  float data[16];
  int n;
  vec_float_t values;
  vec_init(&values);

  switch (type) {
    case GL_SAMPLER_2D:
    case GL_SAMPLER_CUBE:
    case GL_INT:
      lovrShaderSendInt(shader, id, luaL_checkinteger(L, 3));
      break;

    case GL_FLOAT:
      lovrShaderSendFloat(shader, id, luaL_checknumber(L, 3));
      break;

    case GL_FLOAT_VEC2:
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
      if (n < size) {
        return luaL_error(L, "Expected %d vec3s, got %d", size, n);
      }

      for (int i = 0; i < size; i++) {
        lua_rawgeti(L, -1, i + 1);
        for (int j = 0; j < 2; j++) {
          lua_rawgeti(L, -1, j + 1);
          vec_push(&values, lua_tonumber(L, -1));
          lua_pop(L, 1);
        }
        lua_pop(L, 1);
      }

      lovrShaderSendFloatVec2(shader, id, size, values.data);
      break;

    case GL_FLOAT_VEC3:
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
      if (n < size) {
        return luaL_error(L, "Expected %d vec3s, got %d", size, n);
      }

      for (int i = 0; i < size; i++) {
        lua_rawgeti(L, -1, i + 1);
        for (int j = 0; j < 3; j++) {
          lua_rawgeti(L, -1, j + 1);
          vec_push(&values, lua_tonumber(L, -1));
          lua_pop(L, 1);
        }
        lua_pop(L, 1);
      }

      lovrShaderSendFloatVec3(shader, id, size, values.data);
      break;

    case GL_FLOAT_VEC4:
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
      if (n < size) {
        return luaL_error(L, "Expected %d vec3s, got %d", size, n);
      }

      for (int i = 0; i < size; i++) {
        lua_rawgeti(L, -1, i + 1);
        for (int j = 0; j < 4; j++) {
          lua_rawgeti(L, -1, j + 1);
          vec_push(&values, lua_tonumber(L, -1));
          lua_pop(L, 1);
        }
        lua_pop(L, 1);
      }

      lovrShaderSendFloatVec4(shader, id, size, values.data);
      break;

    case GL_FLOAT_MAT2:
      luaL_checktype(L, 3, LUA_TTABLE);
      for (int i = 0; i < 4; i++) {
        lua_rawgeti(L, 3, i + 1);
        data[i] = lua_tonumber(L, -1);
        lua_pop(L, 1);
      }
      lovrShaderSendFloatMat2(shader, id, data);
      break;

    case GL_FLOAT_MAT3:
      luaL_checktype(L, 3, LUA_TTABLE);
      for (int i = 0; i < 9; i++) {
        lua_rawgeti(L, 3, i + 1);
        data[i] = lua_tonumber(L, -1);
        lua_pop(L, 1);
      }
      lovrShaderSendFloatMat3(shader, id, data);
      break;

    case GL_FLOAT_MAT4:
      if (lua_isuserdata(L, 3)) {
        Transform* transform = luax_checktype(L, 3, Transform);
        memcpy(data, transform->matrix, 16 * sizeof(float));
      } else {
        luaL_checktype(L, 3, LUA_TTABLE);
        for (int i = 0; i < 16; i++) {
          lua_rawgeti(L, 3, i + 1);
          data[i] = lua_tonumber(L, -1);
          lua_pop(L, 1);
        }
      }
      lovrShaderSendFloatMat4(shader, id, data);
      break;

    default:
      return luaL_error(L, "Unknown uniform type %d", type);
  }

  vec_deinit(&values);

  return 0;
}

const luaL_Reg lovrShader[] = {
  { "send", l_lovrShaderSend },
  { NULL, NULL }
};
