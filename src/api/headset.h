#include "api.h"
#include "headset/headset.h"

int l_lovrHeadsetGetPose(lua_State* L);
int l_lovrHeadsetGetPosition(lua_State* L);
int l_lovrHeadsetGetOrientation(lua_State* L);
int l_lovrHeadsetGetVelocity(lua_State* L);
int l_lovrHeadsetGetAngularVelocity(lua_State* L);
int l_lovrHeadsetIsDown(lua_State* L);
int l_lovrHeadsetIsTouched(lua_State* L);
int l_lovrHeadsetGetAxis(lua_State* L);
int l_lovrHeadsetVibrate(lua_State* L);
int l_lovrHeadsetNewModel(lua_State* L);
Path luax_optpath(lua_State* L, int index, const char* fallback);
void luax_pushpath(lua_State* L, Path path);
