#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <stdint.h>
#include <string.h>
#include "core/util.h"

#pragma once

// Modules
LOVR_EXPORT int luaopen_lovr(lua_State* L);
LOVR_EXPORT int luaopen_lovr_audio(lua_State* L);
LOVR_EXPORT int luaopen_lovr_data(lua_State* L);
LOVR_EXPORT int luaopen_lovr_event(lua_State* L);
LOVR_EXPORT int luaopen_lovr_filesystem(lua_State* L);
LOVR_EXPORT int luaopen_lovr_graphics(lua_State* L);
LOVR_EXPORT int luaopen_lovr_headset(lua_State* L);
LOVR_EXPORT int luaopen_lovr_math(lua_State* L);
LOVR_EXPORT int luaopen_lovr_physics(lua_State* L);
LOVR_EXPORT int luaopen_lovr_thread(lua_State* L);
LOVR_EXPORT int luaopen_lovr_timer(lua_State* L);
extern const luaL_Reg lovrModules[];

// Objects
extern const luaL_Reg lovrAudioStream[];
extern const luaL_Reg lovrBallJoint[];
extern const luaL_Reg lovrBlob[];
extern const luaL_Reg lovrBoxShape[];
extern const luaL_Reg lovrCanvas[];
extern const luaL_Reg lovrCapsuleShape[];
extern const luaL_Reg lovrChannel[];
extern const luaL_Reg lovrCollider[];
extern const luaL_Reg lovrCurve[];
extern const luaL_Reg lovrCylinderShape[];
extern const luaL_Reg lovrDistanceJoint[];
extern const luaL_Reg lovrFont[];
extern const luaL_Reg lovrHingeJoint[];
extern const luaL_Reg lovrMat4[];
extern const luaL_Reg lovrMaterial[];
extern const luaL_Reg lovrMesh[];
extern const luaL_Reg lovrMicrophone[];
extern const luaL_Reg lovrModel[];
extern const luaL_Reg lovrModelData[];
extern const luaL_Reg lovrQuat[];
extern const luaL_Reg lovrRandomGenerator[];
extern const luaL_Reg lovrRasterizer[];
extern const luaL_Reg lovrShader[];
extern const luaL_Reg lovrShaderBlock[];
extern const luaL_Reg lovrSliderJoint[];
extern const luaL_Reg lovrSoundData[];
extern const luaL_Reg lovrSource[];
extern const luaL_Reg lovrSphereShape[];
extern const luaL_Reg lovrMeshShape[];
extern const luaL_Reg lovrTexture[];
extern const luaL_Reg lovrTextureData[];
extern const luaL_Reg lovrThread[];
extern const luaL_Reg lovrVec2[];
extern const luaL_Reg lovrVec4[];
extern const luaL_Reg lovrVec3[];
extern const luaL_Reg lovrWorld[];

// Enums
typedef struct {
  uint8_t length;
  char string[31];
} StringEntry;

#define ENTRY(s) { sizeof(s) - 1, s }

extern StringEntry lovrArcMode[];
extern StringEntry lovrAttributeType[];
extern StringEntry lovrBlendAlphaMode[];
extern StringEntry lovrBlendMode[];
extern StringEntry lovrBlockType[];
extern StringEntry lovrBufferUsage[];
extern StringEntry lovrCompareMode[];
extern StringEntry lovrCoordinateSpace[];
extern StringEntry lovrDevice[];
extern StringEntry lovrDeviceAxe[];
extern StringEntry lovrDeviceButton[];
extern StringEntry lovrDrawMode[];
extern StringEntry lovrDrawStyle[];
extern StringEntry lovrEventType[];
extern StringEntry lovrFilterMode[];
extern StringEntry lovrHeadsetDriver[];
extern StringEntry lovrHeadsetOrigin[];
extern StringEntry lovrHorizontalAlign[];
extern StringEntry lovrJointType[];
extern StringEntry lovrMaterialColor[];
extern StringEntry lovrMaterialScalar[];
extern StringEntry lovrMaterialTexture[];
extern StringEntry lovrShaderType[];
extern StringEntry lovrShapeType[];
extern StringEntry lovrSourceType[];
extern StringEntry lovrStencilAction[];
extern StringEntry lovrTextureFormat[];
extern StringEntry lovrTextureType[];
extern StringEntry lovrTimeUnit[];
extern StringEntry lovrUniformAccess[];
extern StringEntry lovrVerticalAlign[];
extern StringEntry lovrWinding[];
extern StringEntry lovrWrapMode[];

// General helpers

typedef struct {
  uint64_t hash;
  void* object;
} Proxy;

#if LUA_VERSION_NUM > 501
#define luax_len(L, i) (int) lua_rawlen(L, i)
#define luax_register(L, f) luaL_setfuncs(L, f, 0)
#else
#define luax_len(L, i) (int) lua_objlen(L, i)
#define luax_register(L, f) luaL_register(L, NULL, f)
#define LUA_RIDX_MAINTHREAD 1
#endif

#define luax_registertype(L, T) _luax_registertype(L, #T, lovr ## T, lovr ## T ## Destroy)
#define luax_totype(L, i, T) (T*) _luax_totype(L, i, hash64(#T, strlen(#T)))
#define luax_checktype(L, i, T) (T*) _luax_checktype(L, i, hash64(#T, strlen(#T)), #T)
#define luax_pushtype(L, T, o) _luax_pushtype(L, #T, hash64(#T, strlen(#T)), o)
#define luax_checkenum(L, i, T, x) _luax_checkenum(L, i, lovr ## T, x, #T)
#define luax_pushenum(L, T, x) lua_pushlstring(L, (lovr ## T)[x].string, (lovr ## T)[x].length)
#define luax_checkfloat(L, i) (float) luaL_checknumber(L, i)
#define luax_optfloat(L, i, x) (float) luaL_optnumber(L, i, x)
#define luax_geterror(L) lua_getfield(L, LUA_REGISTRYINDEX, "_lovrerror")
#define luax_seterror(L) lua_setfield(L, LUA_REGISTRYINDEX, "_lovrerror")
#define luax_clearerror(L) lua_pushnil(L), luax_seterror(L)

void _luax_registertype(lua_State* L, const char* name, const luaL_Reg* functions, void (*destructor)(void*));
void* _luax_totype(lua_State* L, int index, uint64_t hash);
void* _luax_checktype(lua_State* L, int index, uint64_t hash, const char* debug);
int luax_typeerror(lua_State* L, int index, const char* expected);
void _luax_pushtype(lua_State* L, const char* name, uint64_t hash, void* object);
int _luax_checkenum(lua_State* L, int index, const StringEntry* map, const char* fallback, const char* label);
void luax_registerloader(lua_State* L, lua_CFunction loader, int index);
int luax_resume(lua_State* T, int n);
void luax_vthrow(void* L, const char* format, va_list args);
void luax_vlog(void* context, int level, const char* tag, const char* format, va_list args);
void luax_traceback(lua_State* L, lua_State* T, const char* message, int level);
int luax_getstack(lua_State* L);
void luax_pushconf(lua_State* L);
int luax_setconf(lua_State* L);
void luax_setmainthread(lua_State* L);
void luax_atexit(lua_State* L, void (*destructor)(void));
void luax_readcolor(lua_State* L, int index, struct Color* color);

// Module helpers

#ifdef LOVR_ENABLE_DATA
struct Blob;
struct Blob* luax_readblob(lua_State* L, int index, const char* debug);
#endif

#ifdef LOVR_ENABLE_EVENT
struct Variant;
void luax_checkvariant(lua_State* L, int index, struct Variant* variant);
int luax_pushvariant(lua_State* L, struct Variant* variant);
#endif

#ifdef LOVR_ENABLE_FILESYSTEM
void* luax_readfile(const char* filename, size_t* bytesRead);
#endif

#ifdef LOVR_ENABLE_GRAPHICS
struct Attachment;
struct Texture;
struct Uniform;
int luax_checkuniform(lua_State* L, int index, const struct Uniform* uniform, void* dest, const char* debug);
int luax_optmipmap(lua_State* L, int index, struct Texture* texture);
void luax_readattachments(lua_State* L, int index, struct Attachment* attachments, int* count);
#endif

#ifdef LOVR_ENABLE_MATH
#include "math/pool.h" // TODO
float* luax_tovector(lua_State* L, int index, VectorType* type);
float* luax_checkvector(lua_State* L, int index, VectorType type, const char* expected);
float* luax_newtempvector(lua_State* L, VectorType type);
int luax_readvec3(lua_State* L, int index, float* v, const char* expected);
int luax_readscale(lua_State* L, int index, float* v, int components, const char* expected);
int luax_readquat(lua_State* L, int index, float* q, const char* expected);
int luax_readmat4(lua_State* L, int index, float* m, int scaleComponents);
uint64_t luax_checkrandomseed(lua_State* L, int index);
#endif

#ifdef LOVR_ENABLE_PHYSICS
struct Joint;
struct Shape;
void luax_pushjoint(lua_State* L, struct Joint* joint);
void luax_pushshape(lua_State* L, struct Shape* shape);
struct Joint* luax_checkjoint(lua_State* L, int index);
struct Shape* luax_checkshape(lua_State* L, int index);
#endif
