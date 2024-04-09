#include "api.h"
#include "physics/physics.h"
#include "data/image.h"
#include "core/maf.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>

void luax_pushshape(lua_State* L, Shape* shape) {
  switch (lovrShapeGetType(shape)) {
    case SHAPE_SPHERE: luax_pushtype(L, SphereShape, shape); break;
    case SHAPE_BOX: luax_pushtype(L, BoxShape, shape); break;
    case SHAPE_CAPSULE: luax_pushtype(L, CapsuleShape, shape); break;
    case SHAPE_CYLINDER: luax_pushtype(L, CylinderShape, shape); break;
    case SHAPE_CONVEX: luax_pushtype(L, ConvexShape, shape); break;
    case SHAPE_MESH: luax_pushtype(L, MeshShape, shape); break;
    case SHAPE_TERRAIN: luax_pushtype(L, TerrainShape, shape); break;
    case SHAPE_COMPOUND: luax_pushtype(L, CompoundShape, shape); break;
    default: lovrUnreachable();
  }
}

Shape* luax_checkshape(lua_State* L, int index) {
  Proxy* p = lua_touserdata(L, index);

  if (p) {
    const uint64_t hashes[] = {
      hash64("SphereShape", strlen("SphereShape")),
      hash64("BoxShape", strlen("BoxShape")),
      hash64("CapsuleShape", strlen("CapsuleShape")),
      hash64("CylinderShape", strlen("CylinderShape")),
      hash64("ConvexShape", strlen("ConvexShape")),
      hash64("MeshShape", strlen("MeshShape")),
      hash64("TerrainShape", strlen("TerrainShape")),
      hash64("CompoundShape", strlen("CompoundShape"))
    };

    for (size_t i = 0; i < COUNTOF(hashes); i++) {
      if (p->hash == hashes[i]) {
        return p->object;
      }
    }
  }

  luax_typeerror(L, index, "Shape");
  return NULL;
}

Shape* luax_newsphereshape(lua_State* L, int index) {
  float radius = luax_optfloat(L, index, 1.f);
  return lovrSphereShapeCreate(radius);
}

Shape* luax_newboxshape(lua_State* L, int index) {
  float size[3];
  luax_readscale(L, index, size, 3, NULL);
  return lovrBoxShapeCreate(size);
}

Shape* luax_newcapsuleshape(lua_State* L, int index) {
  float radius = luax_optfloat(L, index + 0, 1.f);
  float length = luax_optfloat(L, index + 1, 1.f);
  return lovrCapsuleShapeCreate(radius, length);
}

Shape* luax_newcylindershape(lua_State* L, int index) {
  float radius = luax_optfloat(L, index + 0, 1.f);
  float length = luax_optfloat(L, index + 1, 1.f);
  return lovrCylinderShapeCreate(radius, length);
}

Shape* luax_newconvexshape(lua_State* L, int index) {
  float* points;
  uint32_t count;
  bool shouldFree;
  luax_readmesh(L, index, &points, &count, NULL, NULL, &shouldFree);
  ConvexShape* shape = lovrConvexShapeCreate(points, count);
  if (shouldFree) lovrFree(points);
  return shape;
}

Shape* luax_newmeshshape(lua_State* L, int index) {
  float* vertices;
  uint32_t* indices;
  uint32_t vertexCount;
  uint32_t indexCount;
  bool shouldFree;

  luax_readmesh(L, index, &vertices, &vertexCount, &indices, &indexCount, &shouldFree);

  // If we do not own the mesh data, we must make a copy
  // ode's trimesh collider needs to own the triangle info for the lifetime of the geom
  // Note that if shouldFree is true, we don't free the data and let the physics module do it when
  // the collider/shape is destroyed
  if (!shouldFree) {
    float* v = vertices;
    uint32_t* i = indices;
    vertices = lovrMalloc(3 * vertexCount * sizeof(float));
    indices = lovrMalloc(indexCount * sizeof(uint32_t));
    memcpy(vertices, v, 3 * vertexCount * sizeof(float));
    memcpy(indices, i, indexCount * sizeof(uint32_t));
  }

  return lovrMeshShapeCreate(vertexCount, vertices, indexCount, indices);
}

Shape* luax_newterrainshape(lua_State* L, int index) {
  float scaleXZ = luax_checkfloat(L, index++);
  int type = lua_type(L, index);
  if (type == LUA_TNIL || type == LUA_TNONE) {
    float vertices[4] = { 0.f };
    return lovrTerrainShapeCreate(vertices, 2, scaleXZ, 1.f);
  } else if (type == LUA_TFUNCTION) {
    uint32_t n = luax_optu32(L, index + 1, 100);
    float* vertices = lovrMalloc(sizeof(float) * n * n);
    for (uint32_t i = 0; i < n * n; i++) {
      float x = scaleXZ * (-.5f + ((float) (i % n)) / n);
      float z = scaleXZ * (-.5f + ((float) (i / n)) / n);
      lua_pushvalue(L, index);
      lua_pushnumber(L, x);
      lua_pushnumber(L, z);
      lua_call(L, 2, 1);
      lovrCheck(lua_type(L, -1) == LUA_TNUMBER, "Expected TerrainShape callback to return a number");
      vertices[i] = luax_tofloat(L, -1);
      lua_pop(L, 1);
    }
    TerrainShape* shape = lovrTerrainShapeCreate(vertices, n, scaleXZ, 1.f);
    lovrFree(vertices);
    return shape;
  } else if (type == LUA_TUSERDATA) {
    Image* image = luax_checktype(L, index, Image);
    uint32_t n = lovrImageGetWidth(image, 0);
    lovrCheck(lovrImageGetHeight(image, 0) == n, "TerrainShape images must be square");
    float scaleY = luax_optfloat(L, index + 1, 1.f);
    float* vertices = lovrMalloc(sizeof(float) * n * n);
    for (uint32_t y = 0; y < n; y++) {
      for (uint32_t x = 0; x < n; x++) {
        float pixel[4];
        lovrImageGetPixel(image, x, y, pixel);
        vertices[x + y * n] = pixel[0];
      }
    }
    TerrainShape* shape = lovrTerrainShapeCreate(vertices, n, scaleXZ, scaleY);
    lovrFree(vertices);
    return shape;
  } else {
    luax_typeerror(L, index, "Image, number, or function");
    return NULL;
  }
}

Shape* luax_newcompoundshape(lua_State* L, int index) {
  if (lua_isnoneornil(L, index)) {
    return lovrCompoundShapeCreate(NULL, NULL, NULL, 0, false);
  }

  luaL_checktype(L, index, LUA_TTABLE);
  int length = luax_len(L, index);

  uint32_t defer = lovrDeferPush();
  Shape** shapes = lovrMalloc(length * sizeof(Shape*));
  float* positions = lovrMalloc(length * 3 * sizeof(float));
  float* orientations = lovrMalloc(length * 4 * sizeof(float));
  lovrDefer(lovrFree, shapes);
  lovrDefer(lovrFree, positions);
  lovrDefer(lovrFree, orientations);

  for (int i = 0; i < length; i++) {
    lua_rawgeti(L, index, i + 1);
    lovrCheck(lua_istable(L, -1), "Expected table of tables for compound shape");

    lua_rawgeti(L, -1, 1);
    shapes[i] = luax_checkshape(L, -1);
    lua_pop(L, 1);

    int index = 2;
    lua_rawgeti(L, -1, index);
    switch (lua_type(L, -1)) {
      case LUA_TNIL:
        vec3_set(&positions[3 * i], 0.f, 0.f, 0.f);
        lua_pop(L, 1);
        break;
      case LUA_TNUMBER:
        lua_rawgeti(L, -2, index + 1);
        lua_rawgeti(L, -3, index + 2);
        vec3_set(&positions[3 * i], luax_tofloat(L, -3), luax_tofloat(L, -2), luax_tofloat(L, -1));
        lua_pop(L, 3);
        index += 3;
        break;
      default: {
        float* v = luax_checkvector(L, -1, V_VEC3, "nil, number, or vec3");
        vec3_init(&positions[3 * i], v);
        lua_pop(L, 1);
        break;
      }
    }

    lua_rawgeti(L, -1, index);
    switch (lua_type(L, -1)) {
      case LUA_TNIL:
        quat_identity(&orientations[4 * i]);
        lua_pop(L, 1);
        break;
      case LUA_TNUMBER:
        lua_rawgeti(L, -2, index);
        lua_rawgeti(L, -3, index);
        lua_rawgeti(L, -4, index);
        quat_set(&orientations[4 * i], luax_tofloat(L, -4), luax_tofloat(L, -3), luax_tofloat(L, -2), luax_tofloat(L, -1));
        lua_pop(L, 4);
        break;
      default: {
        float* q = luax_checkvector(L, -1, V_QUAT, "nil, number, or quat");
        quat_init(&positions[4 * i], q);
        lua_pop(L, 1);
        break;
      }
    }

    lua_pop(L, 1);
  }

  lua_getfield(L, index, "freeze");
  bool freeze = lua_toboolean(L, -1);
  lua_pop(L, 1);

  CompoundShape* shape = lovrCompoundShapeCreate(shapes, positions, orientations, length, freeze);
  lovrDeferPop(defer);
  return shape;
}

static int l_lovrShapeDestroy(lua_State* L) {
  Shape* shape = luax_checkshape(L, 1);
  lovrShapeDestroyData(shape);
  return 0;
}

static int l_lovrShapeGetType(lua_State* L) {
  Shape* shape = luax_checkshape(L, 1);
  luax_pushenum(L, ShapeType, lovrShapeGetType(shape));
  return 1;
}

static void luax_pushshapestash(lua_State* L) {
  lua_getfield(L, LUA_REGISTRYINDEX, "_lovrshapestash");

  if (lua_isnil(L, -1)) {
    lua_newtable(L);
    lua_replace(L, -2);

    // metatable
    lua_newtable(L);
    lua_pushliteral(L, "k");
    lua_setfield(L, -2, "__mode");
    lua_setmetatable(L, -2);

    lua_pushvalue(L, -1);
    lua_setfield(L, LUA_REGISTRYINDEX, "_lovrshapestash");
  }
}

static int l_lovrShapeGetUserData(lua_State* L) {
  luax_checktype(L, 1, Shape);
  luax_pushshapestash(L);
  lua_pushvalue(L, 1);
  lua_rawget(L, -2);
  return 1;
}

static int l_lovrShapeSetUserData(lua_State* L) {
  luax_checktype(L, 1, Shape);
  luax_pushshapestash(L);
  lua_pushvalue(L, 1);
  lua_pushvalue(L, 2);
  lua_rawset(L, -3);
  return 0;
}

static int l_lovrShapeGetMass(lua_State* L) {
  Shape* shape = luax_checkshape(L, 1);
  float density = luax_checkfloat(L, 2);
  float centerOfMass[3], mass, inertia[6];
  lovrShapeGetMass(shape, density, centerOfMass, &mass, inertia);
  lua_pushnumber(L, centerOfMass[0]);
  lua_pushnumber(L, centerOfMass[1]);
  lua_pushnumber(L, centerOfMass[2]);
  lua_pushnumber(L, mass);
  lua_newtable(L);
  for (int i = 0; i < 6; i++) {
    lua_pushnumber(L, inertia[i]);
    lua_rawseti(L, -2, i + 1);
  }
  return 5;
}

static int l_lovrShapeGetAABB(lua_State* L) {
  Shape* shape = luax_checkshape(L, 1);
  float position[3], orientation[4], aabb[6];
  if (lua_gettop(L) >= 2) {
    int index = 2;
    index = luax_readvec3(L, index, position, NULL);
    index = luax_readquat(L, index, orientation, NULL);
    lovrShapeGetAABB(shape, position, orientation, aabb);
  } else {
    lovrShapeGetAABB(shape, NULL, NULL, aabb);
  }
  for (int i = 0; i < 6; i++) {
    lua_pushnumber(L, aabb[i]);
  }
  return 6;
}

#define lovrShape \
  { "destroy", l_lovrShapeDestroy }, \
  { "getType", l_lovrShapeGetType }, \
  { "getUserData", l_lovrShapeGetUserData }, \
  { "setUserData", l_lovrShapeSetUserData }, \
  { "getMass", l_lovrShapeGetMass }, \
  { "getAABB", l_lovrShapeGetAABB }

static int l_lovrSphereShapeGetRadius(lua_State* L) {
  SphereShape* sphere = luax_checktype(L, 1, SphereShape);
  lua_pushnumber(L, lovrSphereShapeGetRadius(sphere));
  return 1;
}

const luaL_Reg lovrSphereShape[] = {
  lovrShape,
  { "getRadius", l_lovrSphereShapeGetRadius },
  { NULL, NULL }
};

static int l_lovrBoxShapeGetDimensions(lua_State* L) {
  BoxShape* box = luax_checktype(L, 1, BoxShape);
  float dimensions[3];
  lovrBoxShapeGetDimensions(box, dimensions);
  lua_pushnumber(L, dimensions[0]);
  lua_pushnumber(L, dimensions[1]);
  lua_pushnumber(L, dimensions[2]);
  return 3;
}

const luaL_Reg lovrBoxShape[] = {
  lovrShape,
  { "getDimensions", l_lovrBoxShapeGetDimensions },
  { NULL, NULL }
};

static int l_lovrCapsuleShapeGetRadius(lua_State* L) {
  CapsuleShape* capsule = luax_checktype(L, 1, CapsuleShape);
  lua_pushnumber(L, lovrCapsuleShapeGetRadius(capsule));
  return 1;
}

static int l_lovrCapsuleShapeGetLength(lua_State* L) {
  CapsuleShape* capsule = luax_checktype(L, 1, CapsuleShape);
  lua_pushnumber(L, lovrCapsuleShapeGetLength(capsule));
  return 1;
}

const luaL_Reg lovrCapsuleShape[] = {
  lovrShape,
  { "getRadius", l_lovrCapsuleShapeGetRadius },
  { "getLength", l_lovrCapsuleShapeGetLength },
  { NULL, NULL }
};

static int l_lovrCylinderShapeGetRadius(lua_State* L) {
  CylinderShape* cylinder = luax_checktype(L, 1, CylinderShape);
  lua_pushnumber(L, lovrCylinderShapeGetRadius(cylinder));
  return 1;
}

static int l_lovrCylinderShapeGetLength(lua_State* L) {
  CylinderShape* cylinder = luax_checktype(L, 1, CylinderShape);
  lua_pushnumber(L, lovrCylinderShapeGetLength(cylinder));
  return 1;
}

const luaL_Reg lovrCylinderShape[] = {
  lovrShape,
  { "getRadius", l_lovrCylinderShapeGetRadius },
  { "getLength", l_lovrCylinderShapeGetLength },
  { NULL, NULL }
};

const luaL_Reg lovrConvexShape[] = {
  lovrShape,
  { NULL, NULL }
};

const luaL_Reg lovrMeshShape[] = {
  lovrShape,
  { NULL, NULL }
};

const luaL_Reg lovrTerrainShape[] = {
  lovrShape,
  { NULL, NULL }
};

static int l_lovrCompoundShapeIsFrozen(lua_State* L) {
  CompoundShape* shape = luax_checktype(L, 1, CompoundShape);
  bool frozen = lovrCompoundShapeIsFrozen(shape);
  lua_pushboolean(L, frozen);
  return 1;
}

static int l_lovrCompoundShapeAddChild(lua_State* L) {
  CompoundShape* shape = luax_checktype(L, 1, CompoundShape);
  Shape* child = luax_checkshape(L, 2);
  float position[3], orientation[4];
  int index = 3;
  index = luax_readvec3(L, index, position, NULL);
  index = luax_readquat(L, index, orientation, NULL);
  lovrCompoundShapeAddChild(shape, child, position, orientation);
  return 0;
}

static int l_lovrCompoundShapeReplaceChild(lua_State* L) {
  CompoundShape* shape = luax_checktype(L, 1, CompoundShape);
  uint32_t index = luax_checku32(L, 2) - 1;
  Shape* child = luax_checkshape(L, 3);
  float position[3], orientation[4];
  int i = 4;
  i = luax_readvec3(L, i, position, NULL);
  i = luax_readquat(L, i, orientation, NULL);
  lovrCompoundShapeReplaceChild(shape, index, child, position, orientation);
  return 0;
}

static int l_lovrCompoundShapeRemoveChild(lua_State* L) {
  CompoundShape* shape = luax_checktype(L, 1, CompoundShape);
  uint32_t index = luax_checku32(L, 2) - 1;
  lovrCompoundShapeRemoveChild(shape, index);
  return 0;
}

static int l_lovrCompoundShapeGetChild(lua_State* L) {
  CompoundShape* shape = luax_checktype(L, 1, CompoundShape);
  uint32_t index = luax_checku32(L, 2) - 1;
  Shape* child = lovrCompoundShapeGetChild(shape, index);
  luax_pushshape(L, child);
  return 1;
}

static int l_lovrCompoundShapeGetChildren(lua_State* L) {
  CompoundShape* shape = luax_checktype(L, 1, CompoundShape);
  int count = (int) lovrCompoundShapeGetChildCount(shape);
  lua_createtable(L, count, 0);
  for (int i = 0; i < count; i++) {
    Shape* child = lovrCompoundShapeGetChild(shape, (uint32_t) i);
    luax_pushshape(L, child);
    lua_rawseti(L, -2, i + 1);
  }
  return 1;
}

static int l_lovrCompoundShapeGetChildCount(lua_State* L) {
  CompoundShape* shape = luax_checktype(L, 1, CompoundShape);
  uint32_t count = lovrCompoundShapeGetChildCount(shape);
  lua_pushinteger(L, count);
  return 1;
}

static int l_lovrCompoundShapeGetChildOffset(lua_State* L) {
  CompoundShape* shape = luax_checktype(L, 1, CompoundShape);
  uint32_t index = luax_checku32(L, 2) - 1;
  float position[3], orientation[4], angle, ax, ay, az;
  lovrCompoundShapeGetChildOffset(shape, index, position, orientation);
  quat_getAngleAxis(orientation, &angle, &ax, &ay, &az);
  lua_pushnumber(L, position[0]);
  lua_pushnumber(L, position[1]);
  lua_pushnumber(L, position[2]);
  lua_pushnumber(L, angle);
  lua_pushnumber(L, ax);
  lua_pushnumber(L, ay);
  lua_pushnumber(L, az);
  return 7;
}

static int l_lovrCompoundShapeSetChildOffset(lua_State* L) {
  CompoundShape* shape = luax_checktype(L, 1, CompoundShape);
  uint32_t index = luax_checku32(L, 2) - 1;
  float position[3], orientation[4];
  int i = 3;
  i = luax_readvec3(L, i, position, NULL);
  i = luax_readquat(L, i, orientation, NULL);
  lovrCompoundShapeSetChildOffset(shape, index, position, orientation);
  return 0;
}

const luaL_Reg lovrCompoundShape[] = {
  lovrShape,
  { "isFrozen", l_lovrCompoundShapeIsFrozen },
  { "addChild", l_lovrCompoundShapeAddChild },
  { "replaceChild", l_lovrCompoundShapeReplaceChild },
  { "removeChild", l_lovrCompoundShapeRemoveChild },
  { "getChild", l_lovrCompoundShapeGetChild },
  { "getChildren", l_lovrCompoundShapeGetChildren },
  { "getChildCount", l_lovrCompoundShapeGetChildCount },
  { "getChildOffset", l_lovrCompoundShapeGetChildOffset },
  { "setChildOffset", l_lovrCompoundShapeSetChildOffset },
  { "__len", l_lovrCompoundShapeGetChildCount }, // :)
  { NULL, NULL }
};
