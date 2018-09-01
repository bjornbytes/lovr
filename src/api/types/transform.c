#include "api.h"
#include "math/mat4.h"
#include "math/transform.h"

int luax_readtransform(lua_State* L, int index, mat4 m, int scaleComponents) {
  if (lua_isnumber(L, index)) {
    float x = luaL_optnumber(L, index++, 0);
    float y = luaL_optnumber(L, index++, 0);
    float z = luaL_optnumber(L, index++, 0);
    float scale[3] = { 1., 1., 1. };
    if (scaleComponents == 1) {
      scale[0] = scale[1] = scale[2] = luaL_optnumber(L, index++, 1);
    } else {
      for (int i = 0; i < scaleComponents; i++) {
        scale[i] = luaL_optnumber(L, index++, 1);
      }
    }
    float angle = luaL_optnumber(L, index++, 0);
    float ax = luaL_optnumber(L, index++, 0);
    float ay = luaL_optnumber(L, index++, 1);
    float az = luaL_optnumber(L, index++, 0);
    mat4_setTransform(m, x, y, z, scale[0], scale[1], scale[2], angle, ax, ay, az);
    return index;
  } else if (lua_isnoneornil(L, index)) {
    mat4_identity(m);
    return index;
  } else {
    Transform* transform = luax_checktype(L, index, Transform);
    mat4_init(m, transform->matrix);
    return ++index;
  }
}

int l_lovrTransformGetMatrix(lua_State* L) {
  Transform* transform = luax_checktype(L, 1, Transform);
  bool table = lua_istable(L, 2);
  lua_settop(L, 2);

  float matrix[16];
  lovrTransformGetMatrix(transform, matrix);

  for (int i = 0; i < 16; i++) {
    lua_pushnumber(L, matrix[i]);
    if (table) {
      lua_rawseti(L, 2, i + 1);
    }
  }

  return table ? 1 : 16;
}

int l_lovrTransformSetMatrix(lua_State* L) {
  Transform* transform = luax_checktype(L, 1, Transform);

  float matrix[16];
  if (lua_istable(L, 2)) {
    for (int i = 0; i < 16; i++) {
      lua_rawgeti(L, 2, i + 1);
      matrix[i] = luaL_checknumber(L, -1);
      lua_pop(L, 1);
    }
  } else {
    for (int i = 0; i < 16; i++) {
      matrix[i] = luaL_checknumber(L, 2 + i);
    }
  }

  lovrTransformSetMatrix(transform, matrix);
  return 0;
}

int l_lovrTransformClone(lua_State* L) {
  Transform* transform = luax_checktype(L, 1, Transform);
  Transform* clone = lovrTransformCreate(transform->matrix);
  luax_pushobject(L, clone);
  lovrRelease(clone);
  return 1;
}

int l_lovrTransformInverse(lua_State* L) {
  Transform* transform = luax_checktype(L, 1, Transform);
  Transform* inverse = lovrTransformCreate(lovrTransformInverse(transform));
  luax_pushobject(L, inverse);
  lovrRelease(inverse);
  return 1;
}

int l_lovrTransformApply(lua_State* L) {
  Transform* transform = luax_checktype(L, 1, Transform);
  Transform* other = luax_checktype(L, 2, Transform);
  lovrTransformApply(transform, other);
  lua_pushvalue(L, 1);
  return 1;
}

int l_lovrTransformOrigin(lua_State* L) {
  lovrTransformOrigin(luax_checktype(L, 1, Transform));
  lua_pushvalue(L, 1);
  return 1;
}

int l_lovrTransformTranslate(lua_State* L) {
  Transform* transform = luax_checktype(L, 1, Transform);
  float x = luaL_checknumber(L, 2);
  float y = luaL_checknumber(L, 3);
  float z = luaL_checknumber(L, 4);
  lovrTransformTranslate(transform, x, y, z);
  lua_pushvalue(L, 1);
  return 1;
}

int l_lovrTransformRotate(lua_State* L) {
  Transform* transform = luax_checktype(L, 1, Transform);
  float angle = luaL_checknumber(L, 2);
  float x = luaL_checknumber(L, 3);
  float y = luaL_checknumber(L, 4);
  float z = luaL_checknumber(L, 5);
  lovrTransformRotate(transform, angle, x, y, z);
  lua_pushvalue(L, 1);
  return 1;
}

int l_lovrTransformScale(lua_State* L) {
  Transform* transform = luax_checktype(L, 1, Transform);
  float x = luaL_checknumber(L, 2);
  float y = lua_gettop(L) > 2 ? luaL_checknumber(L, 3) : x;
  float z = lua_gettop(L) > 2 ? luaL_checknumber(L, 4) : x;
  lovrTransformScale(transform, x, y, z);
  lua_pushvalue(L, 1);
  return 1;
}

int l_lovrTransformSetTransformation(lua_State* L) {
  Transform* transform = luax_checktype(L, 1, Transform);
  lovrTransformOrigin(transform); // Dirty the Transform
  luax_readtransform(L, 2, transform->matrix, 3);
  lua_pushvalue(L, 1);
  return 1;
}

int l_lovrTransformTransformPoint(lua_State* L) {
  Transform* transform = luax_checktype(L, 1, Transform);
  float x = luaL_checknumber(L, 2);
  float y = luaL_checknumber(L, 3);
  float z = luaL_checknumber(L, 4);
  lovrTransformTransformPoint(transform, &x, &y, &z);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, z);
  return 3;
}

int l_lovrTransformInverseTransformPoint(lua_State* L) {
  Transform* transform = luax_checktype(L, 1, Transform);
  float x = luaL_checknumber(L, 2);
  float y = luaL_checknumber(L, 3);
  float z = luaL_checknumber(L, 4);
  lovrTransformInverseTransformPoint(transform, &x, &y, &z);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, z);
  return 3;
}

const luaL_Reg lovrTransform[] = {
  { "getMatrix", l_lovrTransformGetMatrix },
  { "setMatrix", l_lovrTransformSetMatrix },
  { "clone", l_lovrTransformClone },
  { "inverse", l_lovrTransformInverse },
  { "apply", l_lovrTransformApply },
  { "origin", l_lovrTransformOrigin },
  { "translate", l_lovrTransformTranslate },
  { "rotate", l_lovrTransformRotate },
  { "scale", l_lovrTransformScale },
  { "setTransformation", l_lovrTransformSetTransformation },
  { "transformPoint", l_lovrTransformTransformPoint },
  { "inverseTransformPoint", l_lovrTransformInverseTransformPoint },
  { NULL, NULL }
};
