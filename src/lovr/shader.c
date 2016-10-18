#include "shader.h"

void luax_pushshader(lua_State* L, Shader* shader) {
  if (shader == NULL) {
    return lua_pushnil(L);
  }

  Shader** userdata = (Shader**) lua_newuserdata(L, sizeof(Shader*));
  luaL_getmetatable(L, "Shader");
  lua_setmetatable(L, -2);
  *userdata = shader;
}

Shader* luax_checkshader(lua_State* L, int index) {
  return *(Shader**) luaL_checkudata(L, index, "Shader");
}

int luax_destroyshader(lua_State* L) {
  Shader* shader = luax_checkshader(L, 1);
  lovrShaderDestroy(shader);
  return 0;
}

const luaL_Reg lovrShader[] = {
  { "send", l_lovrShaderSend },
  { NULL, NULL }
};

int l_lovrShaderSend(lua_State* L) {
  Shader* shader = luax_checkshader(L, 1);
  const char* name = luaL_checkstring(L, 2);

  int id = lovrShaderGetUniformId(shader, name);
  if (id == -1) {
    return luaL_error(L, "Unknown shader variable '%s'", name);
  }

  GLenum type;
  int size;
  lovrShaderGetUniformType(shader, name, &type, &size);

  float data[16];

  switch (type) {
    case GL_FLOAT:
      lovrShaderSendFloat(shader, id, luaL_checknumber(L, 3));
      break;

    case GL_FLOAT_VEC2:
      luaL_checktype(L, 3, LUA_TTABLE);
      for (int i = 0; i < 2; i++) {
        lua_rawgeti(L, 3, i + 1);
        data[i] = lua_tonumber(L, -1);
        lua_pop(L, 1);
      }
      lovrShaderSendFloatVec2(shader, id, data);
      break;

    case GL_FLOAT_VEC3:
      luaL_checktype(L, 3, LUA_TTABLE);
      for (int i = 0; i < 3; i++) {
        lua_rawgeti(L, 3, i + 1);
        data[i] = lua_tonumber(L, -1);
        lua_pop(L, 1);
      }
      lovrShaderSendFloatVec3(shader, id, data);
      break;

    case GL_FLOAT_VEC4:
      luaL_checktype(L, 3, LUA_TTABLE);
      for (int i = 0; i < 4; i++) {
        lua_rawgeti(L, 3, i + 1);
        data[i] = lua_tonumber(L, -1);
        lua_pop(L, 1);
      }
      lovrShaderSendFloatVec4(shader, id, data);
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
      luaL_checktype(L, 3, LUA_TTABLE);
      for (int i = 0; i < 16; i++) {
        lua_rawgeti(L, 3, i + 1);
        data[i] = lua_tonumber(L, -1);
        lua_pop(L, 1);
      }
      lovrShaderSendFloatMat4(shader, id, data);
      break;

    default:
      return luaL_error(L, "Unknown uniform type %d", type);
  }

  return 0;
}
