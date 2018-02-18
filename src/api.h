#include "luax.h"
#include "data/blob.h"
#include "graphics/mesh.h"
#include "math/math.h"
#include "math/randomGenerator.h"
#include "physics/physics.h"
#include "lib/map/map.h"

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
extern const luaL_Reg lovrModel[];
extern const luaL_Reg lovrModelData[];
extern const luaL_Reg lovrRandomGenerator[];
extern const luaL_Reg lovrRasterizer[];
extern const luaL_Reg lovrShader[];
extern const luaL_Reg lovrShape[];
extern const luaL_Reg lovrSliderJoint[];
extern const luaL_Reg lovrSource[];
extern const luaL_Reg lovrSphereShape[];
extern const luaL_Reg lovrTexture[];
extern const luaL_Reg lovrTextureData[];
extern const luaL_Reg lovrThread[];
extern const luaL_Reg lovrTransform[];
extern const luaL_Reg lovrVertexData[];
extern const luaL_Reg lovrWorld[];

// Enums
extern map_int_t ArcModes;
extern map_int_t AttributeTypes;
extern map_int_t BlendAlphaModes;
extern map_int_t BlendModes;
extern map_int_t CompareModes;
extern map_int_t ControllerAxes;
extern map_int_t ControllerButtons;
extern map_int_t ControllerHands;
extern map_int_t DrawModes;
extern map_int_t EventTypes;
extern map_int_t FilterModes;
extern map_int_t HeadsetEyes;
extern map_int_t HeadsetOrigins;
extern map_int_t HeadsetTypes;
extern map_int_t HorizontalAligns;
extern map_int_t JointTypes;
extern map_int_t MaterialColors;
extern map_int_t MaterialScalars;
extern map_int_t MaterialTextures;
extern map_int_t MatrixTypes;
extern map_int_t MeshDrawModes;
extern map_int_t MeshUsages;
extern map_int_t PolygonWindings;
extern map_int_t ShapeTypes;
extern map_int_t TextureFormats;
extern map_int_t TextureTypes;
extern map_int_t TimeUnits;
extern map_int_t VerticalAligns;
extern map_int_t WrapModes;

// Shared helpers
void luax_checkvertexformat(lua_State* L, int index, VertexFormat* format);
int luax_pushvertexformat(lua_State* L, VertexFormat* format);
int luax_pushvertexattribute(lua_State* L, VertexPointer* vertex, Attribute attribute);
int luax_pushvertex(lua_State* L, VertexPointer* vertex, VertexFormat* format);
void luax_setvertexattribute(lua_State* L, int index, VertexPointer* vertex, Attribute attribute);
void luax_setvertex(lua_State* L, int index, VertexPointer* vertex, VertexFormat* format);
int luax_readtransform(lua_State* L, int index, mat4 transform, bool uniformScale);
Blob* luax_readblob(lua_State* L, int index, const char* debug);
int luax_pushshape(lua_State* L, Shape* shape);
int luax_pushjoint(lua_State* L, Joint* joint);
Seed luax_checkrandomseed(lua_State* L, int index);
