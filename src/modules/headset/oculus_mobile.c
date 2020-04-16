#include "headset/headset.h"
#include "oculus_mobile_bridge.h"
#include "graphics/graphics.h"
#include "graphics/canvas.h"
#include "core/os.h"
#include "core/ref.h"
#include "event/event.h"
#include "lib/glad/glad.h"
#include <android/log.h>
#include <assert.h>
#include <stdlib.h>
#include <math.h>

#define LOG(...)  __android_log_print(ANDROID_LOG_DEBUG, "LOVR", __VA_ARGS__)
#define INFO(...) __android_log_print(ANDROID_LOG_INFO,  "LOVR", __VA_ARGS__)
#define WARN(...) __android_log_print(ANDROID_LOG_WARN,  "LOVR", __VA_ARGS__)

// Data passed from bridge code to headset code

static struct {
  BridgeLovrDimensions displayDimensions;
  float displayFrequency;
  BridgeLovrDevice deviceType;
  BridgeLovrVibrateFunction* vibrateFunction;
  BridgeLovrUpdateData updateData;
  uint32_t textureHandles[4];
  uint32_t textureCount;
  Canvas* canvases[4];
} bridgeLovrMobileData;

// Headset

static struct {
  void (*renderCallback)(void*);
  void* renderUserdata;
  uint32_t msaa;
  float offset;
  Variant nextBootCookie; // Only used during restart event
} state;

// Headset driver object

static bool vrapi_init(float offset, uint32_t msaa) {
  state.msaa = msaa;
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

static float vrapi_getDisplayFrequency(void) {
  return bridgeLovrMobileData.displayFrequency;
}

static const float* vrapi_getDisplayMask(uint32_t* count) {
  *count = 0;
  return NULL;
}

static double vrapi_getDisplayTime(void) {
  return bridgeLovrMobileData.updateData.displayTime;
}

static uint32_t vrapi_getViewCount(void) {
  return 2;
}

static bool vrapi_getViewPose(uint32_t view, float* position, float* orientation) {
  if (view > 1) return false;
  float transform[16];
  mat4_init(transform, bridgeLovrMobileData.updateData.eyeViewMatrix[view]);
  mat4_invert(transform); // :(
  mat4_getPosition(transform, position);
  mat4_getOrientation(transform, orientation);
  return true;
}

static bool vrapi_getViewAngles(uint32_t view, float* left, float* right, float* up, float* down) {
  return false; // TODO decompose projection matrix into fov angles
}

static void vrapi_getClipDistance(float* clipNear, float* clipFar) {
  // TODO
}

static void vrapi_setClipDistance(float clipNear, float clipFar) {
  // TODO
}

static void vrapi_getBoundsDimensions(float* width, float* depth) {
  *width = bridgeLovrMobileData.updateData.boundsWidth;
  *depth = bridgeLovrMobileData.updateData.boundsDepth;
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

static bool vrapi_getVelocity(Device device, vec3 velocity, vec3 angularVelocity) {
  BridgeLovrAngularVector* v;

  if (device == DEVICE_HEAD) {
    v = &bridgeLovrMobileData.updateData.lastHeadMovement.velocity;
  } else {
    int idx = getHandIdx(device);
    if (idx < 0)
      return false;
    v = &bridgeLovrMobileData.updateData.controllers[idx].movement.velocity;
  }

  vec3_set(velocity, v->x, v->y, v->z);
  vec3_set(angularVelocity, v->ax, v->ay, v->az);
  return true;
}

// Notice: Quest has a thumbstick, Go has a touchpad
static bool buttonDown(BridgeLovrButton field, DeviceButton button, bool *result) {
  if (bridgeLovrMobileData.deviceType == BRIDGE_LOVR_DEVICE_QUEST) {
    switch (button) {
      case BUTTON_MENU: *result = field & BRIDGE_LOVR_BUTTON_MENU; break; // Technically "LMENU" but only fires on left controller
      case BUTTON_TRIGGER: *result = field & BRIDGE_LOVR_BUTTON_SHOULDER; break;
      case BUTTON_GRIP: *result = field & BRIDGE_LOVR_BUTTON_GRIP; break;
      case BUTTON_THUMBSTICK: *result = field & BRIDGE_LOVR_BUTTON_JOYSTICK; break;
      case BUTTON_A: *result = field & BRIDGE_LOVR_BUTTON_A; break;
      case BUTTON_B: *result = field & BRIDGE_LOVR_BUTTON_B; break;
      case BUTTON_X: *result = field & BRIDGE_LOVR_BUTTON_X; break;
      case BUTTON_Y: *result = field & BRIDGE_LOVR_BUTTON_Y; break;
      default: return false;
    }
  } else {
    switch (button) {
      case BUTTON_MENU: *result = field & BRIDGE_LOVR_BUTTON_GOMENU; break; // Technically "RMENU" but quest only has one
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
    case BUTTON_TRIGGER: *result = field & (BRIDGE_LOVR_TOUCH_TRIGGER); break;
    case BUTTON_THUMBSTICK: *result = field & (BRIDGE_LOVR_TOUCH_TOUCHPAD | BRIDGE_LOVR_TOUCH_JOYSTICK); break;
    case BUTTON_A: *result = field & BRIDGE_LOVR_TOUCH_A; break;
    case BUTTON_B: *result = field & BRIDGE_LOVR_TOUCH_B; break;
    case BUTTON_X: *result = field & BRIDGE_LOVR_TOUCH_X; break;
    case BUTTON_Y: *result = field & BRIDGE_LOVR_TOUCH_Y; break;
    default: return false;
  }
  return true;
}

static bool vrapi_isDown(Device device, DeviceButton button, bool* down, bool* changed) {
  int idx = getHandIdx(device);
  if (idx < 0)
    return false;

  buttonDown(bridgeLovrMobileData.updateData.controllers[idx].buttonChanged, button, changed);
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
  int controller;
  if (device == DEVICE_HAND_LEFT)
    controller = 0;
  else if (device == DEVICE_HAND_RIGHT)
    controller = 1;
  else
    return false;
  return bridgeLovrMobileData.vibrateFunction(controller, strength, duration); // Frequency currently discarded
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
  .getDisplayFrequency = vrapi_getDisplayFrequency,
  .getDisplayMask = vrapi_getDisplayMask,
  .getDisplayTime = vrapi_getDisplayTime,
  .getViewCount = vrapi_getViewCount,
  .getViewPose = vrapi_getViewPose,
  .getViewAngles = vrapi_getViewAngles,
  .getClipDistance = vrapi_getClipDistance,
  .setClipDistance = vrapi_setClipDistance,
  .getBoundsDimensions = vrapi_getBoundsDimensions,
  .getBoundsGeometry = vrapi_getBoundsGeometry,
  .getPose = vrapi_getPose,
  .getVelocity = vrapi_getVelocity,
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
#include <sys/stat.h>
#include <assert.h>

#include "oculus_mobile_bridge.h"

#include "api/api.h"
#include "lib/lua-cjson/lua_cjson.h"
#include "lib/lua-enet/enet.h"
#include "headset/oculus_mobile.h"

// Implicit from boot.lua.h
extern unsigned char src_resources_boot_lua[];
extern unsigned int src_resources_boot_lua_len;

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

// A version of print that uses LOG, since stdout does not work on Android
int luax_print(lua_State* L) {
  luaL_Buffer buffer;
  int n = lua_gettop(L);
  lua_getglobal(L, "tostring");
  luaL_buffinit(L, &buffer);
  for (int i = 1; i <= n; i++) {
    lua_pushvalue(L, -1);
    lua_pushvalue(L, i);
    lua_call(L, 1, 1);
    lovrAssert(lua_type(L, -1) == LUA_TSTRING, LUA_QL("tostring") " must return a string to " LUA_QL("print"));
    if (i > 1) {
      luaL_addchar(&buffer, '\t');
    }
    luaL_addvalue(&buffer);
  }
  luaL_pushresult(&buffer);
  INFO("%s", lua_tostring(L, -1));
  return 0;
}

static int luax_custom_atpanic(lua_State *L) {
  WARN("PANIC: unprotected error in call to Lua API (%s)\n", lua_tostring(L, -1));
  return 0;
}

static void bridgeLovrInitState() {
  // Ready to actually go now.
  // Copypaste the init sequence from lovrRun:
  // Load libraries
  L = luaL_newstate(); // FIXME: Can this be handed off to main.c?
  luaL_openlibs(L);
  LOG("\n OPENED LIB\n");
  lua_atpanic(L, luax_custom_atpanic);
  luax_setmainthread(L);

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
    if (state.nextBootCookie.type != TYPE_NIL) {
      luax_pushvariant(L, &state.nextBootCookie);
      lovrVariantDestroy(&state.nextBootCookie);
      state.nextBootCookie.type = TYPE_NIL;
      lua_setfield(L, -2, "restart");
    }

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
  if (luaL_loadbuffer(L, (const char*) src_resources_boot_lua, src_resources_boot_lua_len, "@boot.lua") || lua_pcall(L, 0, 1, -2)) {
    WARN("\n LUA STARTUP FAILED: %s\n", lua_tostring(L, -1));
    lua_close(L);
    assert(0);
  }

  coroutineStartFunctionRef = luaL_ref(L, LUA_REGISTRYINDEX); // Value returned by boot.lua
  T = lua_newthread(L); // Leave L clear to be used by the draw function
  coroutineRef = luaL_ref(L, LUA_REGISTRYINDEX); // Hold on to the Lua-side coroutine object so it isn't GC'd

  LOG("\n STATE INIT COMPLETE\n");
}

void bridgeLovrInit(BridgeLovrInitData *initData) {
  LOG("\n INSIDE LOVR\n");

  // Save writable data directory for LovrFilesystemInit later
  {
    size_t length = strlen(initData->writablePath);
    lovrOculusMobileWritablePath = malloc(length + strlen("/data") + 1);
    memcpy(lovrOculusMobileWritablePath, initData->writablePath, length);
    memcpy(lovrOculusMobileWritablePath + length, "/data", strlen("/data") + 1);
    mkdir(lovrOculusMobileWritablePath, 0777);
  }

  // Unpack init data
  bridgeLovrMobileData.displayDimensions = initData->suggestedEyeTexture;
  bridgeLovrMobileData.displayFrequency = initData->displayFrequency;
  bridgeLovrMobileData.updateData.displayTime = initData->zeroDisplayTime;
  bridgeLovrMobileData.deviceType = initData->deviceType;
  bridgeLovrMobileData.vibrateFunction = initData->vibrateFunction;
  memcpy(bridgeLovrMobileData.textureHandles, initData->textureHandles, initData->textureCount * sizeof(uint32_t));
  bridgeLovrMobileData.textureCount = initData->textureCount;

  free(apkPath);
  size_t length = strlen(initData->apkPath);
  apkPath = malloc(length + 1);
  lovrAssert(apkPath, "Out of memory");
  memcpy(apkPath, initData->apkPath, length + 1);

  bridgeLovrInitState();

  LOG("\n BRIDGE INIT COMPLETE\n");
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
  lovrSetErrorCallback(luax_vthrow, T);
  if (lua_resume(T, 1) != LUA_YIELD) {
    if (lua_type(T, -2) == LUA_TSTRING && !strcmp(lua_tostring(T, -2), "restart")) {
      luax_checkvariant(T, -1, &state.nextBootCookie);
      state.renderCallback = NULL;
      state.renderUserdata = NULL;
      lua_close(L);
      bridgeLovrInitState();
    } else {
      LOG("\n LUA REQUESTED A QUIT\n");
      assert(0);
    }
  }
}

void bridgeLovrDraw(BridgeLovrDrawData *drawData) {
  if (!state.renderCallback) // Do not draw if there is nothing to draw.
    return;

  lovrGpuDirtyTexture(); // Clear texture state since LÖVR doesn't completely own the GL context

  // Lazily create Canvas objects on the first frame
  if (!bridgeLovrMobileData.canvases[0]) {
    for (uint32_t i = 0; i < bridgeLovrMobileData.textureCount; i++) {
      uint32_t width = bridgeLovrMobileData.displayDimensions.width;
      uint32_t height = bridgeLovrMobileData.displayDimensions.height;

      bridgeLovrMobileData.canvases[i] = lovrCanvasCreate(width, height, (CanvasFlags) {
        .depth.enabled = true,
        .depth.readable = false,
        .depth.format = FORMAT_D24S8,
        .msaa = state.msaa,
        .stereo = true,
        .mipmaps = false
      });

      uint32_t handle = bridgeLovrMobileData.textureHandles[i];
      Texture* texture = lovrTextureCreateFromHandle(handle, TEXTURE_ARRAY, 2);
      lovrCanvasSetAttachments(bridgeLovrMobileData.canvases[i], &(Attachment) { .texture = texture }, 1);
      lovrRelease(Texture, texture);
    }
  }

  // Set up a camera using the view and projection matrices from lovr-oculus-mobile
  Camera camera = { .canvas = bridgeLovrMobileData.canvases[drawData->textureIndex] };
  mat4_init(camera.viewMatrix[0], bridgeLovrMobileData.updateData.eyeViewMatrix[0]);
  mat4_init(camera.viewMatrix[1], bridgeLovrMobileData.updateData.eyeViewMatrix[1]);
  mat4_init(camera.projection[0], bridgeLovrMobileData.updateData.projectionMatrix[0]);
  mat4_init(camera.projection[1], bridgeLovrMobileData.updateData.projectionMatrix[1]);
  mat4_translate(camera.viewMatrix[0], 0.f, -state.offset, 0.f);
  mat4_translate(camera.viewMatrix[1], 0.f, -state.offset, 0.f);

  lovrGraphicsSetCamera(&camera, true);

  lovrSetErrorCallback(luax_vthrow, L);
  state.renderCallback(state.renderUserdata);

  lovrGraphicsDiscard(false, true, true);
  lovrGraphicsSetCamera(NULL, false);
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
  free(lovrOculusMobileWritablePath);
  for (uint32_t i = 0; i < bridgeLovrMobileData.textureCount; i++) {
    lovrRelease(Canvas, bridgeLovrMobileData.canvases[i]);
  }
  memset(&bridgeLovrMobileData, 0, sizeof(bridgeLovrMobileData));
}
