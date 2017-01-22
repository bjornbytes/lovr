#include "lovr/types/transform.h"
#include "math/mat4.h"

void luax_readtransform(lua_State* L, int i, mat4 m) {
  if (lua_isnumber(L, i)) {
    float x = luaL_optnumber(L, i++, 0);
    float y = luaL_optnumber(L, i++, 0);
    float z = luaL_optnumber(L, i++, 0);
    float s = luaL_optnumber(L, i++, 1);
    float angle = luaL_optnumber(L, i++, 0);
    float ax = luaL_optnumber(L, i++, 0);
    float ay = luaL_optnumber(L, i++, 1);
    float az = luaL_optnumber(L, i++, 0);
    mat4_setTransform(m, x, y, z, s, angle, ax, ay, az);
  } else if (lua_isnoneornil(L, i)) {
    mat4_identity(m);
  } else {
    Transform* transform = luax_checktype(L, i, Transform);
    mat4_init(m, transform->matrix);
  }
}

const luaL_Reg lovrTransform[] = {
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

int l_lovrTransformClone(lua_State* L) {
  Transform* transform = luax_checktype(L, 1, Transform);
  luax_pushtype(L, Transform, lovrTransformCreate(transform->matrix));
  return 1;
}

int l_lovrTransformInverse(lua_State* L) {
  Transform* transform = luax_checktype(L, 1, Transform);
  Transform* inverse = lovrTransformCreate(lovrTransformInverse(transform));
  luax_pushtype(L, Transform, inverse);
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
  luax_readtransform(L, 2, transform->matrix);
  lua_pushvalue(L, 1);
  return 1;
}

int l_lovrTransformTransformPoint(lua_State* L) {
  Transform* transform = luax_checktype(L, 1, Transform);
  float point[3] = {
    luaL_checknumber(L, 2),
    luaL_checknumber(L, 3),
    luaL_checknumber(L, 4)
  };
  lovrTransformTransformPoint(transform, point);
  lua_pushnumber(L, point[0]);
  lua_pushnumber(L, point[1]);
  lua_pushnumber(L, point[2]);
  return 3;
}

int l_lovrTransformInverseTransformPoint(lua_State* L) {
  Transform* transform = luax_checktype(L, 1, Transform);
  float point[3] = {
    luaL_checknumber(L, 2),
    luaL_checknumber(L, 3),
    luaL_checknumber(L, 4)
  };
  lovrTransformInverseTransformPoint(transform, point);
  lua_pushnumber(L, point[0]);
  lua_pushnumber(L, point[1]);
  lua_pushnumber(L, point[2]);
  return 3;
}
