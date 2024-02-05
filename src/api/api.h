#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include <lua.h>
#include <lauxlib.h>

#pragma once

// Enums
typedef struct {
  uint8_t length;
  char string[31];
} StringEntry;

#define ENTRY(s) { sizeof(s) - 1, s }

extern StringEntry lovrAnimationProperty[];
extern StringEntry lovrArcMode[];
extern StringEntry lovrAttributeType[];
extern StringEntry lovrAudioMaterial[];
extern StringEntry lovrAudioShareMode[];
extern StringEntry lovrAudioType[];
extern StringEntry lovrBlendAlphaMode[];
extern StringEntry lovrBlendMode[];
extern StringEntry lovrBlockType[];
extern StringEntry lovrBufferLayout[];
extern StringEntry lovrChannelLayout[];
extern StringEntry lovrCompareMode[];
extern StringEntry lovrCullMode[];
extern StringEntry lovrDataType[];
extern StringEntry lovrDefaultAttribute[];
extern StringEntry lovrDefaultShader[];
extern StringEntry lovrDevice[];
extern StringEntry lovrDeviceAxis[];
extern StringEntry lovrDeviceButton[];
extern StringEntry lovrDrawMode[];
extern StringEntry lovrDrawStyle[];
extern StringEntry lovrEffect[];
extern StringEntry lovrEventType[];
extern StringEntry lovrFilterMode[];
extern StringEntry lovrHeadsetDriver[];
extern StringEntry lovrHorizontalAlign[];
extern StringEntry lovrJointType[];
extern StringEntry lovrKeyboardKey[];
extern StringEntry lovrMeshStorage[];
extern StringEntry lovrModelDrawMode[];
extern StringEntry lovrOpenMode[];
extern StringEntry lovrOriginType[];
extern StringEntry lovrPassType[];
extern StringEntry lovrPermission[];
extern StringEntry lovrSampleFormat[];
extern StringEntry lovrShaderStage[];
extern StringEntry lovrShapeType[];
extern StringEntry lovrSmoothMode[];
extern StringEntry lovrStackType[];
extern StringEntry lovrStencilAction[];
extern StringEntry lovrTextureFeature[];
extern StringEntry lovrTextureFormat[];
extern StringEntry lovrTextureType[];
extern StringEntry lovrTextureUsage[];
extern StringEntry lovrTimeUnit[];
extern StringEntry lovrUniformAccess[];
extern StringEntry lovrVerticalAlign[];
extern StringEntry lovrViewMask[];
extern StringEntry lovrVolumeUnit[];
extern StringEntry lovrWinding[];
extern StringEntry lovrWrapMode[];

// General helpers

typedef struct {
  const char* name;
  void (*destructor)(void*);
} TypeInfo;

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

#ifdef LOVR_UNCHECKED
#define luax_checku32(L, i) (uint32_t) lua_tonumber(L, i)
#define luax_optu32(L, i, x) (uint32_t) luaL_optnumber(L, i, x)
#else
#define luax_checku32(L, i) _luax_checku32(L, i)
#define luax_optu32(L, i, x) _luax_optu32(L, i, x)
#endif

#define luax_registertype(L, T) _luax_registertype(L, #T, lovr ## T, lovr ## T ## Destroy)
#define luax_totype(L, i, T) (T*) _luax_totype(L, i, hash64(#T, sizeof(#T) - 1))
#define luax_checktype(L, i, T) (T*) _luax_checktype(L, i, hash64(#T, sizeof(#T) - 1), #T)
#define luax_pushtype(L, T, o) _luax_pushtype(L, #T, hash64(#T, sizeof(#T) - 1), o)
#define luax_checkenum(L, i, T, x) _luax_checkenum(L, i, lovr ## T, x, #T)
#define luax_pushenum(L, T, x) lua_pushlstring(L, (lovr ## T)[x].string, (lovr ## T)[x].length)
#define luax_checkfloat(L, i) (float) luaL_checknumber(L, i)
#define luax_optfloat(L, i, x) (float) luaL_optnumber(L, i, x)
#define luax_tofloat(L, i) (float) lua_tonumber(L, i)

void luax_preload(lua_State* L);
void _luax_registertype(lua_State* L, const char* name, const luaL_Reg* functions, void (*destructor)(void*));
void* _luax_totype(lua_State* L, int index, uint64_t hash);
void* _luax_checktype(lua_State* L, int index, uint64_t hash, const char* debug);
int luax_typeerror(lua_State* L, int index, const char* expected);
void _luax_pushtype(lua_State* L, const char* name, uint64_t hash, void* object);
int _luax_checkenum(lua_State* L, int index, const StringEntry* map, const char* fallback, const char* label);
void luax_registerloader(lua_State* L, int (*loader)(lua_State* L), int index);
int luax_resume(lua_State* T, int n);
int luax_loadbufferx(lua_State* L, const char* buffer, size_t size, const char* name, const char* mode);
void luax_vthrow(void* L, const char* format, va_list args);
void luax_vlog(void* context, int level, const char* tag, const char* format, va_list args);
void luax_traceback(lua_State* L, lua_State* T, const char* message, int level);
int luax_getstack(lua_State* L);
void luax_pushconf(lua_State* L);
int luax_setconf(lua_State* L);
void luax_setmainthread(lua_State* L);
void luax_atexit(lua_State* L, void (*finalizer)(void));
uint32_t _luax_checku32(lua_State* L, int index);
uint32_t _luax_optu32(lua_State* L, int index, uint32_t fallback);
void luax_readcolor(lua_State* L, int index, float color[4]);
void luax_optcolor(lua_State* L, int index, float color[4]);
int luax_readmesh(lua_State* L, int index, float** vertices, uint32_t* vertexCount, uint32_t** indices, uint32_t* indexCount, bool* shouldFree);

// Module helpers

#ifndef LOVR_DISABLE_DATA
struct Blob;
struct Image;
struct ModelData;
struct Blob* luax_readblob(lua_State* L, int index, const char* debug);
struct Image* luax_checkimage(lua_State* L, int index);
uint32_t luax_checkcodepoint(lua_State* L, int index);
uint32_t luax_checkanimationindex(lua_State* L, int index, struct ModelData* model);
uint32_t luax_checkmaterialindex(lua_State* L, int index, struct ModelData* model);
uint32_t luax_checknodeindex(lua_State* L, int index, struct ModelData* model);
#endif

#ifndef LOVR_DISABLE_EVENT
struct Variant;
void luax_checkvariant(lua_State* L, int index, struct Variant* variant);
int luax_pushvariant(lua_State* L, struct Variant* variant);
#endif

#ifndef LOVR_DISABLE_FILESYSTEM
void* luax_readfile(const char* filename, size_t* bytesRead);
bool luax_writefile(const char* filename, const void* data, size_t size);
#endif

#ifndef LOVR_DISABLE_GRAPHICS
struct DataField;
struct Material;
struct ColoredString;
void luax_checkfieldn(lua_State* L, int index, int type, void* data);
void luax_checkfieldv(lua_State* L, int index, int type, void* data);
void luax_checkfieldt(lua_State* L, int index, int type, void* data);
uint32_t luax_checkfieldarray(lua_State* L, int index, const struct DataField* array, char* data);
void luax_checkdataflat(lua_State* L, int index, int subindex, uint32_t count, const struct DataField* format, char* data);
void luax_checkdatatuples(lua_State* L, int index, int start, uint32_t count, const struct DataField* format, char* data);
void luax_checkdatakeys(lua_State* L, int index, int start, uint32_t count, const struct DataField* array, char* data);
void luax_checkstruct(lua_State* L, int index, const struct DataField* fields, uint32_t count, char* data);
int luax_pushbufferdata(lua_State* L, const struct DataField* format, uint32_t count, char* data);
void luax_pushbufferformat(lua_State* L, const struct DataField* fields, uint32_t count);
uint32_t luax_gettablestride(lua_State* L, int index, int subindex, struct DataField* fields, uint32_t count);
uint32_t luax_checkcomparemode(lua_State* L, int index);
struct Material* luax_optmaterial(lua_State* L, int index);
struct ColoredString* luax_checkcoloredstrings(lua_State* L, int index, uint32_t* count, struct ColoredString* stack);
#endif

#ifndef LOVR_DISABLE_MATH
#include "math/math.h" // TODO
float* luax_tovector(lua_State* L, int index, VectorType* type);
float* luax_checkvector(lua_State* L, int index, VectorType type, const char* expected);
float* luax_newtempvector(lua_State* L, VectorType type);
inline void luax_readobjarr(lua_State* L, int index, size_t n, float* out, const char* name);
int luax_readvec2(lua_State* L, int index, float* v, const char* expected);
int luax_readvec3(lua_State* L, int index, float* v, const char* expected);
int luax_readvec4(lua_State* L, int index, float* v, const char* expected);
int luax_readscale(lua_State* L, int index, float* v, int components, const char* expected);
int luax_readquat(lua_State* L, int index, float* q, const char* expected);
int luax_readmat4(lua_State* L, int index, float* m, int scaleComponents);
uint64_t luax_checkrandomseed(lua_State* L, int index);
#endif

#ifndef LOVR_DISABLE_PHYSICS
struct Joint;
struct Shape;
void luax_pushjoint(lua_State* L, struct Joint* joint);
void luax_pushshape(lua_State* L, struct Shape* shape);
struct Joint* luax_checkjoint(lua_State* L, int index);
struct Shape* luax_checkshape(lua_State* L, int index);
struct Shape* luax_newsphereshape(lua_State* L, int index);
struct Shape* luax_newboxshape(lua_State* L, int index);
struct Shape* luax_newcapsuleshape(lua_State* L, int index);
struct Shape* luax_newcylindershape(lua_State* L, int index);
struct Shape* luax_newmeshshape(lua_State* L, int index);
struct Shape* luax_newterrainshape(lua_State* L, int index);
#endif
