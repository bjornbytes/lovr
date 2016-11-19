#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "vendor/map/map.h"

map_int_t BufferAttributeTypes;
map_int_t BufferDrawModes;
map_int_t BufferUsages;
map_int_t DrawModes;
map_int_t PolygonWindings;
map_int_t FilterModes;
map_int_t WrapModes;

extern const luaL_Reg lovrGraphics[];
int l_lovrGraphicsInit(lua_State* L);
int l_lovrGraphicsReset(lua_State* L);
int l_lovrGraphicsClear(lua_State* L);
int l_lovrGraphicsPresent(lua_State* L);
int l_lovrGraphicsGetBackgroundColor(lua_State* L);
int l_lovrGraphicsSetBackgroundColor(lua_State* L);
int l_lovrGraphicsGetColor(lua_State* L);
int l_lovrGraphicsSetColor(lua_State* L);
int l_lovrGraphicsGetColorMask(lua_State* L);
int l_lovrGraphicsSetColorMask(lua_State* L);
int l_lovrGraphicsGetScissor(lua_State* L);
int l_lovrGraphicsSetScissor(lua_State* L);
int l_lovrGraphicsGetShader(lua_State* L);
int l_lovrGraphicsSetShader(lua_State* L);
int l_lovrGraphicsSetProjection(lua_State* L);
int l_lovrGraphicsGetLineWidth(lua_State* L);
int l_lovrGraphicsSetLineWidth(lua_State* L);
int l_lovrGraphicsGetPointSize(lua_State* L);
int l_lovrGraphicsSetPointSize(lua_State* L);
int l_lovrGraphicsIsCullingEnabled(lua_State* L);
int l_lovrGraphicsSetCullingEnabled(lua_State* L);
int l_lovrGraphicsGetPolygonWinding(lua_State* L);
int l_lovrGraphicsSetPolygonWinding(lua_State* L);
int l_lovrGraphicsPush(lua_State* L);
int l_lovrGraphicsPop(lua_State* L);
int l_lovrGraphicsOrigin(lua_State* L);
int l_lovrGraphicsTranslate(lua_State* L);
int l_lovrGraphicsRotate(lua_State* L);
int l_lovrGraphicsScale(lua_State* L);
int l_lovrGraphicsPoints(lua_State* L);
int l_lovrGraphicsLine(lua_State* L);
int l_lovrGraphicsPlane(lua_State* L);
int l_lovrGraphicsCube(lua_State* L);
int l_lovrGraphicsGetWidth(lua_State* L);
int l_lovrGraphicsGetHeight(lua_State* L);
int l_lovrGraphicsGetDimensions(lua_State* L);
int l_lovrGraphicsNewBuffer(lua_State* L);
int l_lovrGraphicsNewModel(lua_State* L);
int l_lovrGraphicsNewShader(lua_State* L);
int l_lovrGraphicsNewSkybox(lua_State* L);
int l_lovrGraphicsNewTexture(lua_State* L);
