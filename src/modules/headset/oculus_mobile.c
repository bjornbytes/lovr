#include "headset/headset.h"
#include "oculus_mobile_bridge.h"
#include "math.h"
#include "graphics/graphics.h"
#include "graphics/canvas.h"
#include "lib/glad/glad.h"
#include <assert.h>
#include <stdlib.h>
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

static bool vrapi_init(float offset, uint32_t msaa) {
  state.offset = offset;
  return true;
}

static void vrapi_destroy() {
  //
}

static bool vrapi_getName(char* buffer, size_t length) {
  const char* name;
  switch (bridgeLovrMobileData.deviceType) {
    case BRIDGE_LOVR_DEVICE_GEAR: name = "Gear VR"; break;
    case BRIDGE_LOVR_DEVICE_GO: name = "Oculus Go"; break;
    case BRIDGE_LOVR_DEVICE_QUEST: name = "Oculus Quest"; break;
    default: return false;
  }

  strncpy(buffer, name, length - 1);
  buffer[length - 1] = '\0';
  return true;
}

static HeadsetOrigin vrapi_getOriginType() {
  return ORIGIN_HEAD;
}

static void vrapi_getDisplayDimensions(uint32_t* width, uint32_t* height) {
  *width = bridgeLovrMobileData.displayDimensions.width;
  *height = bridgeLovrMobileData.displayDimensions.height;
}

static void vrapi_getClipDistance(float* clipNear, float* clipFar) {
  // TODO
}

static void vrapi_setClipDistance(float clipNear, float clipFar) {
  // TODO
}

static void vrapi_getBoundsDimensions(float* width, float* depth) {
  *width = 0.f;
  *depth = 0.f;
}

static const float* vrapi_getBoundsGeometry(uint32_t* count) {
  *count = 0;
  return NULL;
}

static int getHandIdx(Device device) {
  if (device == DEVICE_HAND_LEFT || device == DEVICE_HAND_RIGHT) {
    for(int c = 0; c < BRIDGE_LOVR_CONTROLLERMAX && c < bridgeLovrMobileData.updateData.controllerCount; c++) {
      BridgeLovrHand hand = (device == DEVICE_HAND_LEFT ? BRIDGE_LOVR_HAND_LEFT : BRIDGE_LOVR_HAND_RIGHT);
      if (bridgeLovrMobileData.updateData.controllers[c].hand & hand)
        return c;
    }
  }
  return -1;
}

static bool vrapi_getPose(Device device, vec3 position, quat orientation) {
  BridgeLovrPose* pose;

  if (device == DEVICE_HEAD) {
    pose = &bridgeLovrMobileData.updateData.lastHeadPose;
  } else {
    int idx = getHandIdx(device);
    if (idx < 0)
      return false;
    pose = &bridgeLovrMobileData.updateData.controllers[idx].pose;
  }

  vec3_set(position, pose->x, pose->y + state.offset, pose->z);
  quat_init(orientation, pose->q);
  return true;
}

static bool vrapi_getBonePose(Device device, DeviceBone bone, vec3 position, quat orientation) {
  return false;
}

// Shared code for velocity/acceleration
static bool vrapi_getAngularVector(Device device, vec3 linear, vec3 angular, bool isAcceleration) {
  BridgeLovrMovement* m;

  if (device == DEVICE_HEAD) {
    m = &bridgeLovrMobileData.updateData.lastHeadMovement;
  } else {
    int idx = getHandIdx(device);
    if (idx < 0)
      return false;
    m = &bridgeLovrMobileData.updateData.controllers[idx].movement;
  }

  BridgeLovrAngularVector* v = isAcceleration ? &m->acceleration : &m->velocity;

  vec3_set(linear, v->x, v->y, v->z);
  vec3_set(angular, v->ax, v->ay, v->az);
  return true;
}

static bool vrapi_getVelocity(Device device, vec3 velocity, vec3 angularVelocity) {
  return vrapi_getAngularVector(device, velocity, angularVelocity, false);
}

static bool vrapi_getAcceleration(Device device, vec3 acceleration, vec3 angularAcceleration) {
  return vrapi_getAngularVector(device, acceleration, angularAcceleration, true);
}

static bool buttonDown(BridgeLovrButton field, DeviceButton button, bool *result) {
  if (bridgeLovrMobileData.deviceType == BRIDGE_LOVR_DEVICE_QUEST) {
    switch (button) {
      case BUTTON_MENU: *result = field & BRIDGE_LOVR_BUTTON_MENU; break; // Technically "LMENU" but only fires on left controller
      case BUTTON_PRIMARY:
      case BUTTON_TRIGGER: *result = field & BRIDGE_LOVR_BUTTON_SHOULDER; break;
      case BUTTON_GRIP: *result = field & BRIDGE_LOVR_BUTTON_GRIP; break;
      case BUTTON_TOUCHPAD: *result = field & BRIDGE_LOVR_BUTTON_JOYSTICK; break;
      case BUTTON_A: *result = field & BRIDGE_LOVR_BUTTON_A; break;
      case BUTTON_B: *result = field & BRIDGE_LOVR_BUTTON_B; break;
      case BUTTON_X: *result = field & BRIDGE_LOVR_BUTTON_X; break;
      case BUTTON_Y: *result = field & BRIDGE_LOVR_BUTTON_Y; break;
      default: return false;
    }
  } else {
    switch (button) {
      case BUTTON_MENU: *result = field & BRIDGE_LOVR_BUTTON_GOMENU; break; // Technically "RMENU" but quest only has one
      case BUTTON_PRIMARY:
      case BUTTON_TRIGGER: *result = field & BRIDGE_LOVR_BUTTON_GOSHOULDER; break;
      case BUTTON_TOUCHPAD: *result = field & BRIDGE_LOVR_BUTTON_TOUCHPAD; break;
      default: return false;
    }
  }
  return true;
}

static bool buttonTouch(BridgeLovrTouch field, DeviceButton button, bool *result) {
  // Only Go touch sensor is the touchpad
  if (bridgeLovrMobileData.deviceType == BRIDGE_LOVR_DEVICE_GO && button != BUTTON_TOUCHPAD)
    return false;

  switch (button) {
    case BUTTON_PRIMARY:
    case BUTTON_TRIGGER: *result = field & (BRIDGE_LOVR_TOUCH_TRIGGER); break;
    case BUTTON_TOUCHPAD: *result = field & (BRIDGE_LOVR_TOUCH_TOUCHPAD | BRIDGE_LOVR_TOUCH_JOYSTICK); break;
    case BUTTON_A: *result = field & BRIDGE_LOVR_TOUCH_A; break;
    case BUTTON_B: *result = field & BRIDGE_LOVR_TOUCH_B; break;
    case BUTTON_X: *result = field & BRIDGE_LOVR_TOUCH_X; break;
    case BUTTON_Y: *result = field & BRIDGE_LOVR_TOUCH_Y; break;
    default: return false;
  }
  return true;
}

static bool vrapi_isDown(Device device, DeviceButton button, bool* down) {
  int idx = getHandIdx(device);
  if (idx < 0)
    return false;

  return buttonDown(bridgeLovrMobileData.updateData.controllers[idx].buttonDown, button, down);
}

static bool vrapi_isTouched(Device device, DeviceButton button, bool* touched) {
  int idx = getHandIdx(device);
  if (idx < 0)
    return false;

  return buttonTouch(bridgeLovrMobileData.updateData.controllers[idx].buttonTouch, button, touched);
}

static bool vrapi_getAxis(Device device, DeviceAxis axis, float* value) {
  int idx = getHandIdx(device);
  if (idx < 0)
    return false;

  BridgeLovrController *data = &bridgeLovrMobileData.updateData.controllers[idx];

  if (bridgeLovrMobileData.deviceType == BRIDGE_LOVR_DEVICE_QUEST) {
    switch (axis) {
      case AXIS_PRIMARY:
      case AXIS_THUMBSTICK:
        value[0] = data->trackpad.x;
        value[1] = data->trackpad.y;
        break;
      case AXIS_TRIGGER: value[0] = data->trigger; break;
      case AXIS_GRIP: value[0] = data->grip; break;
      default: return false;
    }
  } else {
    switch (axis) {
      case AXIS_PRIMARY:
      case AXIS_TOUCHPAD:
        value[0] = (data->trackpad.x - 160) / 160.f;
        value[1] = (data->trackpad.y - 160) / 160.f;
        break;
      case AXIS_TRIGGER: {
        bool down;
        if (!buttonDown(data->buttonDown, BUTTON_TRIGGER, &down))
          return false;
        value[0] = down ? 1.f : 0.f;
        break;
      }
      default: return false;
    }
  }
  return true;
}

static bool vrapi_vibrate(Device device, float strength, float duration, float frequency) {
  return false;
}

static ModelData* vrapi_newModelData(Device device) {
  return NULL;
}

// TODO: need to set up swap chain textures for the eyes and finish view transforms
static void vrapi_renderTo(void (*callback)(void*), void* userdata) {
  state.renderCallback = callback;
  state.renderUserdata = userdata;
}

HeadsetInterface lovrHeadsetOculusMobileDriver = {
  .driverType = DRIVER_OCULUS_MOBILE,
  .init = vrapi_init,
  .destroy = vrapi_destroy,
  .getName = vrapi_getName,
  .getOriginType = vrapi_getOriginType,
  .getDisplayDimensions = vrapi_getDisplayDimensions,
  .getClipDistance = vrapi_getClipDistance,
  .setClipDistance = vrapi_setClipDistance,
  .getBoundsDimensions = vrapi_getBoundsDimensions,
  .getBoundsGeometry = vrapi_getBoundsGeometry,
  .getPose = vrapi_getPose,
  .getBonePose = vrapi_getBonePose,
  .getVelocity = vrapi_getVelocity,
  .getAcceleration = vrapi_getAcceleration,
  .isDown = vrapi_isDown,
  .isTouched = vrapi_isTouched,
  .getAxis = vrapi_getAxis,
  .vibrate = vrapi_vibrate,
  .newModelData = vrapi_newModelData,
  .renderTo = vrapi_renderTo
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
  if (width)
    *width = bridgeLovrMobileData.displayDimensions.width;
  if (height)
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

#include "api/api.h"
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

  lovrSetErrorCallback((errorFn*) android_vthrow, L);

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
  size_t length = strlen(initData->apkPath);
  apkPath = malloc(length + 1);
  lovrAssert(apkPath, "Out of memory");
  memcpy(apkPath, initData->apkPath, length + 1);

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
      state.renderCallback = NULL;
      state.renderUserdata = NULL;
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
