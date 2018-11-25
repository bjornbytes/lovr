#include "lib/math.h"
#include "math/pool.h"
#include "math/randomGenerator.h"

float* luax_tolightmathtype(lua_State* L, int index, MathType* type);
void luax_pushlightmathtype(lua_State* L, float* p, MathType type);
float* luax_tomathtype(lua_State* L, int index, MathType* type);
float* luax_checkmathtype(lua_State* L, int index, MathType type, const char* message);
int luax_readtransform(lua_State* L, int index, mat4 transform, int scaleComponents);
Seed luax_checkrandomseed(lua_State* L, int index);
int l_lovrRandomGeneratorRandom(lua_State* L);
int l_lovrRandomGeneratorRandomNormal(lua_State* L);
int l_lovrRandomGeneratorGetSeed(lua_State* L);
int l_lovrRandomGeneratorSetSeed(lua_State* L);
int l_lovrPoolVec3(lua_State* L);
int l_lovrPoolQuat(lua_State* L);
int l_lovrPoolMat4(lua_State* L);
int l_lovrPoolDrain(lua_State* L);
int l_lovrVec3Set(lua_State* L);
int l_lovrQuatSet(lua_State* L);
int l_lovrMat4Set(lua_State* L);
