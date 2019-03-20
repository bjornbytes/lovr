#include "headset/headset.h"
#include "oculus_mobile_bridge.h"
#include "math.h"
#include "graphics/graphics.h"
#include "graphics/canvas.h"
#include "lib/glad/glad.h"
#include <assert.h>
#include "platform.h"

// Data passed from bridge code to headset code

typedef struct {
  BridgeLovrDimensions displayDimensions;
  BridgeLovrDevice deviceType;
  BridgeLovrUpdateData updateData;
} BridgeLovrMobileData;
BridgeLovrMobileData bridgeLovrMobileData;

// Headset

static void (*renderCallback)(void*);
static void* renderUserdata;

static float offset;

// Headset driver object

static bool oculusMobileInit(float _offset, int msaa) {
  // Make sure HeadsetDriver and BridgeLovrDevice have not gone out of sync
  assert(BRIDGE_LOVR_DEVICE_UNKNOWN == HEADSET_UNKNOWN);
  assert(BRIDGE_LOVR_DEVICE_GEAR == HEADSET_GEAR);
  assert(BRIDGE_LOVR_DEVICE_GO == HEADSET_GO);

  offset = _offset;
  return true;
}

static void oculusMobileDestroy() {
  //
}

static HeadsetType oculusMobileGetType() {
  return (HeadsetType) bridgeLovrMobileData.deviceType;
}

static HeadsetOrigin oculusMobileGetOriginType() {
  return ORIGIN_HEAD;
}

static bool oculusMobileIsMounted() {
  return true;
}

static void oculusMobileGetDisplayDimensions(uint32_t* width, uint32_t* height) {
  *width = bridgeLovrMobileData.displayDimensions.width;
  *height = bridgeLovrMobileData.displayDimensions.height;
}

static void oculusMobileGetClipDistance(float* clipNear, float* clipFar) {
  // TODO
}

static void oculusMobileSetClipDistance(float clipNear, float clipFar) {
  // TODO
}

static void oculusMobileGetBoundsDimensions(float* width, float* depth) {
  *width = 0.f;
  *depth = 0.f;
}

static const float* oculusMobileGetBoundsGeometry(int* count) {
  *count = 0;
  return NULL;
}

static bool oculusMobileGetPose(float* x, float* y, float* z, float* angle, float* ax, float* ay, float* az) {
  *x = bridgeLovrMobileData.updateData.lastHeadPose.x;
  *y = bridgeLovrMobileData.updateData.lastHeadPose.y + offset; // Correct for head height
  *z = bridgeLovrMobileData.updateData.lastHeadPose.z;
  quat_getAngleAxis(bridgeLovrMobileData.updateData.lastHeadPose.q, angle, ax, ay, az);
  return true;
}

static bool oculusMobileGetVelocity(float* vx, float* vy, float* vz) {
  *vx = bridgeLovrMobileData.updateData.lastHeadVelocity.x;
  *vy = bridgeLovrMobileData.updateData.lastHeadVelocity.y;
  *vz = bridgeLovrMobileData.updateData.lastHeadVelocity.z;
  return true;
}

static bool oculusMobileGetAngularVelocity(float* vx, float* vy, float* vz) {
  *vx = bridgeLovrMobileData.updateData.lastHeadVelocity.ax;
  *vy = bridgeLovrMobileData.updateData.lastHeadVelocity.ay;
  *vz = bridgeLovrMobileData.updateData.lastHeadVelocity.az;
  return true;
}

static Controller *controller;

static Controller** oculusMobileGetControllers(uint8_t* count) {
  if (!controller)
    controller = lovrAlloc(Controller);
  *count = bridgeLovrMobileData.updateData.goPresent; // TODO: Figure out what multi controller Oculus Mobile looks like and support it
  return &controller;
}

static bool oculusMobileControllerIsConnected(Controller* controller) {
  return bridgeLovrMobileData.updateData.goPresent;
}

static ControllerHand oculusMobileControllerGetHand(Controller* controller) {
  return HAND_UNKNOWN;
}

static void oculusMobileControllerGetPose(Controller* controller, float* x, float* y, float* z, float* angle, float* ax, float* ay, float* az) {
  *x = bridgeLovrMobileData.updateData.goPose.x;
  *y = bridgeLovrMobileData.updateData.goPose.y + offset; // Correct for head height
  *z = bridgeLovrMobileData.updateData.goPose.z;
  quat_getAngleAxis(bridgeLovrMobileData.updateData.goPose.q, angle, ax, ay, az);
}

static void oculusMobileControllerGetVelocity(Controller* controller, float* vx, float* vy, float* vz) {
  *vx = bridgeLovrMobileData.updateData.goVelocity.x;
  *vy = bridgeLovrMobileData.updateData.goVelocity.y;
  *vz = bridgeLovrMobileData.updateData.goVelocity.z;
}

static void oculusMobileControllerGetAngularVelocity(Controller* controller, float* vx, float* vy, float* vz) {
  *vx = bridgeLovrMobileData.updateData.goVelocity.ax;
  *vy = bridgeLovrMobileData.updateData.goVelocity.ay;
  *vz = bridgeLovrMobileData.updateData.goVelocity.az;
}

static float oculusMobileControllerGetAxis(Controller* controller, ControllerAxis axis) {
  switch (axis) {
    case CONTROLLER_AXIS_TOUCHPAD_X:
      return (bridgeLovrMobileData.updateData.goTrackpad.x - 160.f) / 160.f;
    case CONTROLLER_AXIS_TOUCHPAD_Y:
      return (bridgeLovrMobileData.updateData.goTrackpad.y - 160.f) / 160.f;
    case CONTROLLER_AXIS_TRIGGER:
      return bridgeLovrMobileData.updateData.goButtonDown ? 1.f : 0.f;
    default:
      return 0.f;
  }
}

static bool buttonCheck(BridgeLovrButton field, ControllerButton button) {
  switch (button) {
    case CONTROLLER_BUTTON_MENU: return field & BRIDGE_LOVR_BUTTON_MENU;
    case CONTROLLER_BUTTON_TRIGGER: return field & BRIDGE_LOVR_BUTTON_SHOULDER;
    case CONTROLLER_BUTTON_TOUCHPAD: return field & BRIDGE_LOVR_BUTTON_TOUCHPAD;
    default: return false;
  }
}

static bool oculusMobileControllerIsDown(Controller* controller, ControllerButton button) {
  return buttonCheck(bridgeLovrMobileData.updateData.goButtonDown, button);
}

static bool oculusMobileControllerIsTouched(Controller* controller, ControllerButton button) {
  return buttonCheck(bridgeLovrMobileData.updateData.goButtonTouch, button);
}

static void oculusMobileControllerVibrate(Controller* controller, float duration, float power) {
  //
}

static ModelData* oculusMobileControllerNewModelData(Controller* controller) {
  return NULL;
}

// TODO: need to set up swap chain textures for the eyes and finish view transforms
static void oculusMobileRenderTo(void (*callback)(void*), void* userdata) {
  renderCallback = callback;
  renderUserdata = userdata;
}

HeadsetInterface lovrHeadsetOculusMobileDriver = {
  .driverType = DRIVER_OCULUS_MOBILE,
  .init = oculusMobileInit,
  .destroy = oculusMobileDestroy,
  .getType = oculusMobileGetType,
  .getOriginType = oculusMobileGetOriginType,
  .isMounted = oculusMobileIsMounted,
  .getDisplayDimensions = oculusMobileGetDisplayDimensions,
  .getClipDistance = oculusMobileGetClipDistance,
  .setClipDistance = oculusMobileSetClipDistance,
  .getBoundsDimensions = oculusMobileGetBoundsDimensions,
  .getBoundsGeometry = oculusMobileGetBoundsGeometry,
  .getPose = oculusMobileGetPose,
  .getVelocity = oculusMobileGetVelocity,
  .getAngularVelocity = oculusMobileGetAngularVelocity,
  .getControllers = oculusMobileGetControllers,
  .controllerIsConnected = oculusMobileControllerIsConnected,
  .controllerGetHand = oculusMobileControllerGetHand,
  .controllerGetPose = oculusMobileControllerGetPose,
  .controllerGetVelocity = oculusMobileControllerGetVelocity,
  .controllerGetAngularVelocity = oculusMobileControllerGetAngularVelocity,
  .controllerGetAxis = oculusMobileControllerGetAxis,
  .controllerIsDown = oculusMobileControllerIsDown,
  .controllerIsTouched = oculusMobileControllerIsTouched,
  .controllerVibrate = oculusMobileControllerVibrate,
  .controllerNewModelData = oculusMobileControllerNewModelData,
  .renderTo = oculusMobileRenderTo
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
  mat4_translate(camera.viewMatrix[0], 0, -offset, 0);

  memcpy(camera.projection[0], projectionMatrix, sizeof(camera.projection[0]));

  lovrGraphicsSetCamera(&camera, true);

  if (renderCallback) {
    renderCallback(renderUserdata);
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
