#include "headset/headset.h"
#include "oculus_mobile_bridge.h"
#include "math.h"
#include "graphics/graphics.h"
#include "graphics/canvas.h"
#include "lib/glad/glad.h"
#include <assert.h>
#include "platform.h"

// Data passed from bridge code to headset code

static struct {
  BridgeLovrDimensions displayDimensions;
  BridgeLovrDevice deviceType;
  BridgeLovrUpdateData updateData;
} bridgeLovrMobileData;

// Headset

static struct {
  void (*renderCallback)(void*);
  void* renderUserdata;
  float offset;
} state;

// Headset driver object

static bool init(float offset, int msaa) {
  state.offset = offset;
  return true;
}

static void destroy() {
  //
}

static bool getName(char* buffer, size_t length) {
  const char* name;
  switch (bridgeLovrMobileData.deviceType) {
    case BRIDGE_LOVR_DEVICE_GEAR: name = "Gear VR"; break;
    case BRIDGE_LOVR_DEVICE_GO: name = "Oculus Go"; break;
    default: return false;
  }

  strncpy(buffer, name, length - 1);
  buffer[length - 1] = '\0';
  return true;
}

static HeadsetOrigin getOriginType() {
  return ORIGIN_HEAD;
}

static void getDisplayDimensions(uint32_t* width, uint32_t* height) {
  *width = bridgeLovrMobileData.displayDimensions.width;
  *height = bridgeLovrMobileData.displayDimensions.height;
}

static void getClipDistance(float* clipNear, float* clipFar) {
  // TODO
}

static void setClipDistance(float clipNear, float clipFar) {
  // TODO
}

static void getBoundsDimensions(float* width, float* depth) {
  *width = 0.f;
  *depth = 0.f;
}

static const float* getBoundsGeometry(int* count) {
  *count = 0;
  return NULL;
}

static bool getPose(const char* path, float* x, float* y, float* z, float* angle, float* ax, float* ay, float* az) {
  BridgeLovrPose* pose;

  if (!strcmp(path, "head")) {
    pose = &bridgeLovrMobileData.updateData.lastHeadPose;
  } else if (!strcmp(path, "hand")) {
    pose = &bridgeLovrMobileData.updateData.goPose;
  } else {
    return false;
  }

  if (x) {
    *x = pose->x;
    *y = pose->y + state.offset; // Correct for head height
    *z = pose->z;
  }

  if (angle) {
    quat_getAngleAxis(pose->q, angle, ax, ay, az);
  }

  return true;
}

static bool getVelocity(const char* path, float* vx, float* vy, float* vz, float* vax, float* vay, float* vaz) {
  BridgeLovrVel* velocity;

  if (!strcmp(path, "head")) {
    velocity = &bridgeLovrMobileData.updateData.lastHeadVelocity;
  } else if (!strcmp(path, "hand")) {
    velocity = &bridgeLovrMobileData.updateData.goVelocity;
  } else {
    return false;
  }

  if (vx) {
    *vx = velocity->x;
    *vy = velocity->y;
    *vz = velocity->z;
  }

  if (vax) {
    *vax = velocity->ax;
    *vay = velocity->ay;
    *vaz = velocity->az;
  }

  return true;
}

static bool buttonCheck(BridgeLovrButton field, const char* path, bool* result) {
  if (!strcmp("hand", path, strlen("hand"))) {
    path += strlen("hand/");
    if (!strcmp(path, "menu")) { return *result = (field & BRIDGE_LOVR_BUTTON_MENU), true; }
    else if (!strcmp(path, "trigger")) { return *result = (field & BRIDGE_LOVR_BUTTON_SHOULDER), true; }
    else if (!strcmp(path, "trackpad")) { return *result = (field & BRIDGE_LOVR_BUTTON_TOUCHPAD), true; }
  }

  return false;
}

static bool isDown(const char* path, bool* down) {
  return buttonCheck(bridgeLovrMobileData.updateData.goButtonDown, path, down);
}

static bool isTouched(const char* path, bool* touched) {
  return buttonCheck(bridgeLovrMobileData.updateData.goButtonTouch, path, touched);
}

static int getAxis(const char* path, float* x, float* y, float* z) {
  if (!strcmp("hand", path, strlen("hand"))) {
    path += strlen("hand/");
    if (!strcmp(path, "trackpad")) {
      *x = (bridgeLovrMobileData.updateData.goTrackpad.x - 160.f) / 160.f;
      *y = (bridgeLovrMobileData.updateData.goTrackpad.y - 160.f) / 160.f;
      return 2;
    } else if (!strcmp(path, "trigger")) {
      *x = bridgeLovrMobileData.updateData.goButtonDown ? 1.f : 0.f;
      return 1;
    }
  }

  return 0;
}

static bool vibrate(const char* path, float strength, float duration, float frequency) {
  return false;
}

static ModelData* newModelData(const char* path) {
  return NULL;
}

// TODO: need to set up swap chain textures for the eyes and finish view transforms
static void renderTo(void (*callback)(void*), void* userdata) {
  state.renderCallback = callback;
  state.renderUserdata = userdata;
}

HeadsetInterface lovrHeadsetOculusMobileDriver = {
  .driverType = DRIVER_OCULUS_MOBILE,
  .init = init,
  .destroy = destroy,
  .getName = getName,
  .getOriginType = getOriginType,
  .getDisplayDimensions = getDisplayDimensions,
  .getClipDistance = getClipDistance,
  .setClipDistance = setClipDistance,
  .getBoundsDimensions = getBoundsDimensions,
  .getBoundsGeometry = getBoundsGeometry,
  .getPose = getPose,
  .getVelocity = getVelocity,
  .isDown = isDown,
  .isTouched = isTouched,
  .getAxis = getAxis,
  .vibrate = vibrate,
  .newModelData = newModelData,
  .renderTo = renderTo
};

// Oculus-specific platform functions

static double timeOffset;

void lovrPlatformSetTime(double time) {
  timeOffset = bridgeLovrMobileData.updateData.displayTime - time;
}

double lovrPlatformGetTime(void) {
  return bridgeLovrMobileData.updateData.displayTime - timeOffset;
}

void lovrPlatformGetFramebufferSize(int* width, int* height) {
  *width = bridgeLovrMobileData.displayDimensions.width;
  *height = bridgeLovrMobileData.displayDimensions.height;
}

bool lovrPlatformHasWindow() {
  return false;
}

// "Bridge" (see oculus_mobile_bridge.h)

#include <stdio.h>
#include <android/log.h>
#include "physfs.h"
#include <sys/stat.h>
#include <assert.h>

#include "oculus_mobile_bridge.h"
#include "luax.h"
#include "lib/sds/sds.h"

#include "api.h"
#include "lib/lua-cjson/lua_cjson.h"
#include "lib/lua-enet/enet.h"
#include "headset/oculus_mobile.h"

// Implicit from boot.lua.h
extern unsigned char boot_lua[];
extern unsigned int boot_lua_len;

static lua_State *L, *T;
static int coroutineRef = LUA_NOREF;
static int coroutineStartFunctionRef = LUA_NOREF;

static char *apkPath;

// Expose to filesystem.h
char *lovrOculusMobileWritablePath;

// Used for resume (pausing the app and returning to the menu) logic. This is needed for two reasons
// 1. The GLFW time should rewind after a pause so that the app cannot perceive time passed
// 2. There is a bug in the Mobile SDK https://developer.oculus.com/bugs/bug/189155031962759/
//    On the first frame after a resume, the time will be total nonsense
static double lastPauseAt, lastPauseAtRaw; // platform time and oculus time at last pause
enum {
  PAUSESTATE_NONE,   // Normal state
  PAUSESTATE_PAUSED, // A pause has been issued -- waiting for resume
  PAUSESTATE_BUG,    // We have resumed, but the next frame will be the bad frame
  PAUSESTATE_RESUME  // We have resumed, and the next frame will need to adjust the clock
} pauseState;

int lovr_luaB_print_override (lua_State *L);

#define SDS(...) sdscatfmt(sdsempty(), __VA_ARGS__)

static void android_vthrow(lua_State* L, const char* format, va_list args) {
  #define MAX_ERROR_LENGTH 1024
  char lovrErrorMessage[MAX_ERROR_LENGTH];
  vsnprintf(lovrErrorMessage, MAX_ERROR_LENGTH, format, args);
  lovrWarn("Error: %s\n", lovrErrorMessage);
  assert(0);
}

static int luax_custom_atpanic(lua_State *L) {
  // This doesn't appear to get a sensible stack. Maybe Luajit would work better?
  luax_traceback(L, L, lua_tostring(L, -1), 0); // Pushes the traceback onto the stack
  lovrThrow("Lua panic: %s", lua_tostring(L, -1));
  return 0;
}

static void bridgeLovrInitState() {
  // Ready to actually go now.
  // Copypaste the init sequence from lovrRun:
  // Load libraries
  L = luaL_newstate(); // FIXME: Can this be handed off to main.c?
  luax_setmainthread(L);
  lua_atpanic(L, luax_custom_atpanic);
  luaL_openlibs(L);
  lovrLog("\n OPENED LIB\n");

  lovrSetErrorCallback((lovrErrorHandler) android_vthrow, L);

  // Install custom print
  lua_pushcfunction(L, luax_print);
  lua_setglobal(L, "print");

  lovrPlatformSetTime(0);

  // Set "arg" global (see main.c)
  {
    lua_newtable(L);
    lua_pushliteral(L, "lovr");
    lua_pushvalue(L, -1); // Double at named key
    lua_setfield(L, -3, "exe");
    lua_rawseti(L, -2, -3);

    // Mimic the arguments "--root /assets" as parsed by lovrInit
    lua_pushliteral(L, "--root");
    lua_rawseti(L, -2, -2);
    lua_pushliteral(L, "/assets");
    lua_pushvalue(L, -1); // Double at named key
    lua_setfield(L, -3, "root");
    lua_rawseti(L, -2, -1);

    lua_pushstring(L, apkPath);
    lua_rawseti(L, -2, 0);

    lua_setglobal(L, "arg");
  }

  // Populate package.preload with built-in modules
  lua_getglobal(L, "package");
  lua_getfield(L, -1, "preload");
  luaL_register(L, NULL, lovrModules);
  lua_pop(L, 2);

  // Run init

  lua_pushcfunction(L, luax_getstack);
  if (luaL_loadbuffer(L, (const char*) boot_lua, boot_lua_len, "boot.lua") || lua_pcall(L, 0, 1, -2)) {
    lovrWarn("\n LUA STARTUP FAILED: %s\n", lua_tostring(L, -1));
    lua_close(L);
    assert(0);
  }

  coroutineStartFunctionRef = luaL_ref(L, LUA_REGISTRYINDEX); // Value returned by boot.lua
  T = lua_newthread(L); // Leave L clear to be used by the draw function
  lua_atpanic(T, luax_custom_atpanic);
  coroutineRef = luaL_ref(L, LUA_REGISTRYINDEX); // Hold on to the Lua-side coroutine object so it isn't GC'd

  lovrLog("\n STATE INIT COMPLETE\n");
}

void bridgeLovrInit(BridgeLovrInitData *initData) {
  lovrLog("\n INSIDE LOVR\n");

  // Save writable data directory for LovrFilesystemInit later
  {
    lovrOculusMobileWritablePath = sdsRemoveFreeSpace(SDS("%s/data", initData->writablePath));
    mkdir(lovrOculusMobileWritablePath, 0777);
  }

  // Unpack init data
  bridgeLovrMobileData.displayDimensions = initData->suggestedEyeTexture;
  bridgeLovrMobileData.updateData.displayTime = initData->zeroDisplayTime;
  bridgeLovrMobileData.deviceType = initData->deviceType;

  free(apkPath);
  apkPath = strdup(initData->apkPath);

  bridgeLovrInitState();

  lovrLog("\n BRIDGE INIT COMPLETE\n");
}

void bridgeLovrUpdate(BridgeLovrUpdateData *updateData) {
  // Unpack update data
  bridgeLovrMobileData.updateData = *updateData;

  if (pauseState == PAUSESTATE_BUG) { // Bad frame-- replace bad time with last known good oculus time
    bridgeLovrMobileData.updateData.displayTime = lastPauseAtRaw;
    pauseState = PAUSESTATE_RESUME;
  } else if (pauseState == PAUSESTATE_RESUME) { // Resume frame-- adjust platform time to be equal to last good platform time
    lovrPlatformSetTime(lastPauseAt);
    pauseState = PAUSESTATE_NONE;
  }

  // Go
  if (coroutineStartFunctionRef != LUA_NOREF) {
    lua_rawgeti(T, LUA_REGISTRYINDEX, coroutineStartFunctionRef);
    luaL_unref (T, LUA_REGISTRYINDEX, coroutineStartFunctionRef);
    coroutineStartFunctionRef = LUA_NOREF; // No longer needed
  }

  luax_geterror(T);
  luax_clearerror(T);
  if (lua_resume(T, 1) != LUA_YIELD) {
    if (lua_type(T, -1) == LUA_TSTRING && !strcmp(lua_tostring(T, -1), "restart")) {
      lua_close(L);
      bridgeLovrInitState();
    } else {
      lovrLog("\n LUA REQUESTED A QUIT\n");
      assert(0);
    }
  }
}

static void lovrOculusMobileDraw(int framebuffer, int width, int height, float *eyeViewMatrix, float *projectionMatrix) {
  lovrGpuDirtyTexture();

  Canvas canvas = { 0 };
  lovrCanvasInitFromHandle(&canvas, width, height, (CanvasFlags) { 0 }, framebuffer, 0, 0, 1, true);

  Camera camera = { .canvas = &canvas, .stereo = false };
  memcpy(camera.viewMatrix[0], eyeViewMatrix, sizeof(camera.viewMatrix[0]));
  mat4_translate(camera.viewMatrix[0], 0, -state.offset, 0);

  memcpy(camera.projection[0], projectionMatrix, sizeof(camera.projection[0]));

  lovrGraphicsSetCamera(&camera, true);

  if (state.renderCallback) {
    state.renderCallback(state.renderUserdata);
  }

  lovrGraphicsSetCamera(NULL, false);
  lovrCanvasDestroy(&canvas);
}

void bridgeLovrDraw(BridgeLovrDrawData *drawData) {
  int eye = drawData->eye;
  lovrOculusMobileDraw(drawData->framebuffer, bridgeLovrMobileData.displayDimensions.width, bridgeLovrMobileData.displayDimensions.height,
    bridgeLovrMobileData.updateData.eyeViewMatrix[eye], bridgeLovrMobileData.updateData.projectionMatrix[eye]); // Is this indexing safe?
}

// Android activity has been stopped or resumed
// In order to prevent weird dt jumps, we need to freeze and reset the clock
static bool armedUnpause;
void bridgeLovrPaused(bool paused) {
  if (paused) { // Save last platform and oculus times and wait for resume
    lastPauseAt = lovrPlatformGetTime();
    lastPauseAtRaw = bridgeLovrMobileData.updateData.displayTime;
    pauseState = PAUSESTATE_PAUSED;
  } else {
    if (pauseState != PAUSESTATE_NONE) { // Got a resume-- set flag to start the state machine in bridgeLovrUpdate
      pauseState = PAUSESTATE_BUG;
    }
  }
}

// Android activity has been "destroyed" (but process will probably not quit)
void bridgeLovrClose() {
  pauseState = PAUSESTATE_NONE;
  lua_close(L);
}
