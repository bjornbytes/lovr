#include "luax.h"
#include "util.h"

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
extern const luaL_Reg lovrTexture[];
extern const luaL_Reg lovrTextureData[];
extern const luaL_Reg lovrThread[];
extern const luaL_Reg lovrVec2[];
extern const luaL_Reg lovrVec4[];
extern const luaL_Reg lovrVec3[];
extern const luaL_Reg lovrWorld[];

// Enums
extern const char* ArcModes[];
extern const char* AttributeTypes[];
extern const char* BlendAlphaModes[];
extern const char* BlendModes[];
extern const char* BlockTypes[];
extern const char* BufferUsages[];
extern const char* CompareModes[];
extern const char* CoordinateSpaces[];
extern const char* DrawModes[];
extern const char* DrawStyles[];
extern const char* EventTypes[];
extern const char* FilterModes[];
extern const char* HeadsetDrivers[];
extern const char* HeadsetEyes[];
extern const char* HeadsetOrigins[];
extern const char* HeadsetTypes[];
extern const char* HorizontalAligns[];
extern const char* JointTypes[];
extern const char* MaterialColors[];
extern const char* MaterialScalars[];
extern const char* MaterialTextures[];
extern const char* ShaderTypes[];
extern const char* ShapeTypes[];
extern const char* SourceTypes[];
extern const char* StencilActions[];
extern const char* TextureFormats[];
extern const char* TextureTypes[];
extern const char* TimeUnits[];
extern const char* UniformAccesses[];
extern const char* VerticalAligns[];
extern const char* Windings[];
extern const char* WrapModes[];

// Helpers

#ifdef LOVR_ENABLE_DATA
struct Blob;
struct Blob* luax_readblob(lua_State* L, int index, const char* debug);
#endif

#ifdef LOVR_ENABLE_EVENT
struct Variant;
void luax_checkvariant(lua_State* L, int index, struct Variant* variant);
int luax_pushvariant(lua_State* L, struct Variant* variant);
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
#include <stdint.h>
#include "math/pool.h" // TODO
#include "math/randomGenerator.h" // TODO
float* luax_tovector(lua_State* L, int index, VectorType* type);
float* luax_checkvector(lua_State* L, int index, VectorType type, const char* expected);
float* luax_newtempvector(lua_State* L, VectorType type);
int luax_readvec3(lua_State* L, int index, float* v, const char* expected);
int luax_readscale(lua_State* L, int index, float* v, int components, const char* expected);
int luax_readquat(lua_State* L, int index, float* q, const char* expected);
int luax_readmat4(lua_State* L, int index, float* m, int scaleComponents);
Seed luax_checkrandomseed(lua_State* L, int index);
#endif

#ifdef LOVR_ENABLE_PHYSICS
struct Joint;
struct Shape;
void luax_pushjoint(lua_State* L, struct Joint* joint);
void luax_pushshape(lua_State* L, struct Shape* shape);
struct Joint* luax_checkjoint(lua_State* L, int index);
struct Shape* luax_checkshape(lua_State* L, int index);
#endif
