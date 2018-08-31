#include "luax.h"
#include "data/blob.h"
#include "event/event.h"
#include "graphics/canvas.h"
#include "graphics/mesh.h"
#include "math/math.h"
#include "math/randomGenerator.h"
#include "physics/physics.h"

// Module loaders
int l_lovrInit(lua_State* L);
int l_lovrAudioInit(lua_State* L);
int l_lovrDataInit(lua_State* L);
int l_lovrEventInit(lua_State* L);
int l_lovrFilesystemInit(lua_State* L);
int l_lovrGraphicsInit(lua_State* L);
int l_lovrHeadsetInit(lua_State* L);
int l_lovrMathInit(lua_State* L);
int l_lovrPhysicsInit(lua_State* L);
int l_lovrThreadInit(lua_State* L);
int l_lovrTimerInit(lua_State* L);

// Modules
extern const luaL_Reg lovr[];
extern const luaL_Reg lovrAudio[];
extern const luaL_Reg lovrData[];
extern const luaL_Reg lovrEvent[];
extern const luaL_Reg lovrFilesystem[];
extern const luaL_Reg lovrGraphics[];
extern const luaL_Reg lovrHeadset[];
extern const luaL_Reg lovrMath[];
extern const luaL_Reg lovrPhysics[];
extern const luaL_Reg lovrThreadModule[];
extern const luaL_Reg lovrTimer[];

// Objects
extern const luaL_Reg lovrAnimator[];
extern const luaL_Reg lovrAudioStream[];
extern const luaL_Reg lovrBallJoint[];
extern const luaL_Reg lovrBlob[];
extern const luaL_Reg lovrBoxShape[];
extern const luaL_Reg lovrCanvas[];
extern const luaL_Reg lovrCapsuleShape[];
extern const luaL_Reg lovrChannel[];
extern const luaL_Reg lovrController[];
extern const luaL_Reg lovrCylinderShape[];
extern const luaL_Reg lovrCollider[];
extern const luaL_Reg lovrDistanceJoint[];
extern const luaL_Reg lovrFont[];
extern const luaL_Reg lovrHingeJoint[];
extern const luaL_Reg lovrJoint[];
extern const luaL_Reg lovrMaterial[];
extern const luaL_Reg lovrMesh[];
extern const luaL_Reg lovrMicrophone[];
extern const luaL_Reg lovrModel[];
extern const luaL_Reg lovrModelData[];
extern const luaL_Reg lovrRandomGenerator[];
extern const luaL_Reg lovrRasterizer[];
extern const luaL_Reg lovrShader[];
extern const luaL_Reg lovrShaderBlock[];
extern const luaL_Reg lovrShape[];
extern const luaL_Reg lovrSliderJoint[];
extern const luaL_Reg lovrSoundData[];
extern const luaL_Reg lovrSource[];
extern const luaL_Reg lovrSphereShape[];
extern const luaL_Reg lovrTexture[];
extern const luaL_Reg lovrTextureData[];
extern const luaL_Reg lovrThread[];
extern const luaL_Reg lovrTransform[];
extern const luaL_Reg lovrVertexData[];
extern const luaL_Reg lovrWorld[];

// Enums
extern const char* ArcModes[];
extern const char* AttributeTypes[];
extern const char* BlendAlphaModes[];
extern const char* BlendModes[];
extern const char* BufferUsages[];
extern const char* CompareModes[];
extern const char* ControllerAxes[];
extern const char* ControllerButtons[];
extern const char* ControllerHands[];
extern const char* DepthFormats[];
extern const char* DrawModes[];
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
extern const char* MeshDrawModes[];
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

// Shared helpers
int luax_loadvertices(lua_State* L, int index, VertexFormat* format, VertexPointer vertices);
bool luax_checkvertexformat(lua_State* L, int index, VertexFormat* format);
int luax_pushvertexformat(lua_State* L, VertexFormat* format);
int luax_pushvertexattribute(lua_State* L, VertexPointer* vertex, Attribute attribute);
int luax_pushvertex(lua_State* L, VertexPointer* vertex, VertexFormat* format);
void luax_setvertexattribute(lua_State* L, int index, VertexPointer* vertex, Attribute attribute);
void luax_setvertex(lua_State* L, int index, VertexPointer* vertex, VertexFormat* format);
int luax_readtransform(lua_State* L, int index, mat4 transform, int scaleComponents);
Blob* luax_readblob(lua_State* L, int index, const char* debug);
Seed luax_checkrandomseed(lua_State* L, int index);
void luax_checkvariant(lua_State* L, int index, Variant* variant);
int luax_pushvariant(lua_State* L, Variant* variant);
int luax_checkuniform(lua_State* L, int index, const Uniform* uniform, void* dest, const char* debug);
void luax_checkuniformtype(lua_State* L, int index, UniformType* baseType, int* components);
int luax_optmipmap(lua_State* L, int index, Texture* texture);
Texture* luax_checktexture(lua_State* L, int index);
void luax_readattachments(lua_State* L, int index, Attachment* attachments, int* count);
