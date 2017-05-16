#include "api/lovr.h"
#include "physics/physics.h"

map_int_t ShapeTypes;

int l_lovrPhysicsInit(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, lovrPhysics);
  luax_registertype(L, "World", lovrWorld);
  luax_registertype(L, "Body", lovrBody);
  luax_extendtype(L, "Shape", "SphereShape", lovrShape, lovrSphereShape);
  luax_extendtype(L, "Shape", "BoxShape", lovrShape, lovrBoxShape);
  luax_extendtype(L, "Shape", "CapsuleShape", lovrShape, lovrCapsuleShape);
  luax_extendtype(L, "Shape", "CylinderShape", lovrShape, lovrCylinderShape);

  map_init(&ShapeTypes);
  map_set(&ShapeTypes, "sphere", SHAPE_SPHERE);
  map_set(&ShapeTypes, "box", SHAPE_BOX);
  map_set(&ShapeTypes, "capsule", SHAPE_CAPSULE);
  map_set(&ShapeTypes, "cylinder", SHAPE_CYLINDER);

  lovrPhysicsInit();
  return 1;
}

int l_lovrPhysicsNewWorld(lua_State* L) {
  luax_pushtype(L, World, lovrWorldCreate());
  return 1;
}

int l_lovrPhysicsNewBody(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  luax_pushtype(L, Body, lovrBodyCreate(world));
  return 1;
}

int l_lovrPhysicsNewSphereShape(lua_State* L) {
  float radius = luaL_optnumber(L, 1, 1.f);
  luax_pushtype(L, SphereShape, lovrSphereShapeCreate(radius));
  return 1;
}

int l_lovrPhysicsNewBoxShape(lua_State* L) {
  float x = luaL_optnumber(L, 1, 1.f);
  float y = luaL_optnumber(L, 2, x);
  float z = luaL_optnumber(L, 3, x);
  luax_pushtype(L, BoxShape, lovrBoxShapeCreate(x, y, z));
  return 1;
}

int l_lovrPhysicsNewCapsuleShape(lua_State* L) {
  float radius = luaL_optnumber(L, 1, 1.f);
  float length = luaL_optnumber(L, 2, 1.f);
  luax_pushtype(L, CapsuleShape, lovrCapsuleShapeCreate(radius, length));
  return 1;
}

int l_lovrPhysicsNewCylinderShape(lua_State* L) {
  float radius = luaL_optnumber(L, 1, 1.f);
  float length = luaL_optnumber(L, 2, 1.f);
  luax_pushtype(L, CylinderShape, lovrCylinderShapeCreate(radius, length));
  return 1;
}

const luaL_Reg lovrPhysics[] = {
  { "newWorld", l_lovrPhysicsNewWorld },
  { "newBody", l_lovrPhysicsNewBody },
  { "newSphereShape", l_lovrPhysicsNewSphereShape },
  { "newBoxShape", l_lovrPhysicsNewBoxShape },
  { "newCapsuleShape", l_lovrPhysicsNewCapsuleShape },
  { "newCylinderShape", l_lovrPhysicsNewCylinderShape },
  { NULL, NULL }
};
