#include "math/mat4.h"
#include "math/randomGenerator.h"

int luax_readtransform(lua_State* L, int index, mat4 transform, int scaleComponents);
Seed luax_checkrandomseed(lua_State* L, int index);
int l_lovrRandomGeneratorRandom(lua_State* L);
int l_lovrRandomGeneratorRandomNormal(lua_State* L);
int l_lovrRandomGeneratorGetSeed(lua_State* L);
int l_lovrRandomGeneratorSetSeed(lua_State* L);

