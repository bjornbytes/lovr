#include "api/lovr.h"
#include "physics/physics.h"

int l_lovrShapeGetType(lua_State* L) {
  Shape* shape = luax_checktypeof(L, 1, Shape);
  luax_pushenum(L, &ShapeTypes, lovrShapeGetType(shape));
  return 1;
}

int l_lovrShapeGetBody(lua_State* L) {
  Shape* shape = luax_checktypeof(L, 1, Shape);
  Body* body = lovrShapeGetBody(shape);

  if (body) {
    luax_pushtype(L, Body, body);
  } else {
    lua_pushnil(L);
  }

  return 1;
}

int l_lovrShapeSetBody(lua_State* L) {
  Shape* shape = luax_checktypeof(L, 1, Shape);
  if (lua_isnoneornil(L, 2)) {
    lovrShapeSetBody(shape, NULL);
  } else {
    Body* body = luax_checktype(L, 2, Body);
    lovrShapeSetBody(shape, body);
  }

  return 0;
}
