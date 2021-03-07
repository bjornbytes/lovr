#include "headset/headset.h"
#include "event/event.h"
#include "graphics/graphics.h"
#include "graphics/canvas.h"
#include "resources/boot.lua.h"
#include "api/api.h"
#include "core/maf.h"
#include "core/os.h"
#include "core/util.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <android/log.h>
#include <jni.h>

// Platform

static struct {
  fn_permission* onPermissionEvent;
} os;

bool os_init() {
  os_open_console();
  return true;
}

void os_destroy() {
  //
}

const char* os_get_name() {
  return "Android";
}

uint32_t os_get_core_count() {
  return sysconf(_SC_NPROCESSORS_ONLN);
}

// To make regular printing work, a thread makes a pipe and redirects stdout and stderr to the write
// end of the pipe.  The read end of the pipe is forwarded to __android_log_write.
static struct {
  int handles[2];
  pthread_t thread;
} logState;

static void* log_main(void* data) {
  int* fd = data;
  pipe(fd);
  dup2(fd[1], STDOUT_FILENO);
  dup2(fd[1], STDERR_FILENO);
  setvbuf(stdout, 0, _IOLBF, 0);
  setvbuf(stderr, 0, _IONBF, 0);
  ssize_t length;
  char buffer[1024];
  while ((length = read(fd[0], buffer, sizeof(buffer) - 1)) > 0) {
    buffer[length] = '\0';
    __android_log_write(ANDROID_LOG_DEBUG, "LOVR", buffer);
  }
  return 0;
}

void os_open_console() {
  pthread_create(&logState.thread, NULL, log_main, logState.handles);
  pthread_detach(logState.thread);
}

#define NS_PER_SEC 1000000000ULL

double os_get_time() {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (double) t.tv_sec + (t.tv_nsec / (double) NS_PER_SEC);
}

void os_sleep(double seconds) {
  seconds += .5e-9;
  struct timespec t;
  t.tv_sec = seconds;
  t.tv_nsec = (seconds - t.tv_sec) * NS_PER_SEC;
  while (nanosleep(&t, &t));
}

JNIEXPORT void JNICALL Java_org_lovr_app_Activity_lovrPermissionEvent(JNIEnv* jni, jobject activity, jint permission, jboolean granted) {
  if (os.onPermissionEvent) {
    os.onPermissionEvent(permission, granted);
  }
}

void os_request_permission(Permission permission) {
  // TODO
}

void os_poll_events() {
  //
}

void os_on_quit(fn_quit* callback) {
  //
}

void os_on_focus(fn_focus* callback) {
  //
}

void os_on_resize(fn_resize* callback) {
  //
}

void os_on_key(fn_key* callback) {
  //
}

void os_on_text*(fn_text* callback) {
  // TODO
}

void os_on_permission(fn_permission* callback) {
  os.onPermissionEvent = callback;
}

bool os_window_open(const WindowFlags* flags) {
  return true;
}

bool os_window_is_open() {
  return false;
}

void os_window_get_size(int* width, int* height) {
  if (width) *width = 0;
  if (height) *height = 0;
}

void os_window_get_fbsize(int* width, int* height) {
  *width = 0;
  *height = 0;
}

void os_window_set_vsync(int interval) {
  //
}

void os_window_swap() {
  //
}

void* os_get_gl_proc_address(const char* function) {
  return (void*) eglGetProcAddress(function);
}

size_t os_get_home_directory(char* buffer, size_t size) {
  return 0;
}

size_t os_get_data_directory(char* buffer, size_t size) {
  buffer[0] = '\0';
  return 0;
}

size_t os_get_working_directory(char* buffer, size_t size) {
  return getcwd(buffer, size) ? strlen(buffer) : 0;
}

size_t os_get_executable_path(char* buffer, size_t size) {
  ssize_t length = readlink("/proc/self/exe", buffer, size - 1);
  if (length >= 0) {
    buffer[length] = '\0';
    return length;
  } else {
    return 0;
  }
}

static char apkPath[1024];
size_t os_get_bundle_path(char* buffer, size_t size, const char** root) {
  size_t length = strlen(apkPath);
  if (length >= size) return 0;
  memcpy(buffer, apkPath, length);
  buffer[length] = '\0';
    *root = "/assets";
  return length;
}

void os_get_mouse_position(double* x, double* y) {
  *x = *y = 0.;
}

void os_set_mouse_mode(os_mouse_mode mode) {
  //
}

bool os_is_mouse_down(os_mouse_button button) {
  return false;
}

bool os_is_key_down(os_key key) {
  return false;
}

// Headset backend

typedef struct {
  GLint id;
  Canvas* instance;
} NativeCanvas;

static struct {
  float offset;
  float clipNear;
  float clipFar;
  uint32_t displayWidth;
  uint32_t displayHeight;
  float headPosition[4];
  float headOrientation[4];
  float fov;
  float ipd;
  struct {
    bool active;
    uint16_t buttons;
    uint16_t changed;
    float trigger;
    float thumbstick[2];
    float position[4];
    float orientation[4];
    float hapticStrength;
    float hapticDuration;
  } controllers[2];
  arr_t(NativeCanvas) canvases;
  void (*renderCallback)(void*);
  void* renderUserdata;
} state;

static bool pico_init(float supersample, float offset, uint32_t msaa) {
  state.offset = offset;
  state.clipNear = .1f;
  state.clipFar = 100.f;
  return true;
}

static void pico_destroy(void) {
  arr_free(&state.canvases);
  memset(&state, 0, sizeof(state));
}

// TODO use presence of controllers to determine G2 vs Neo (isControllerServiceExisted)
static bool pico_getName(char* name, size_t length) {
  strncpy(name, "Pico", length - 1);
  name[length - 1] = '\0';
  return true;
}

// The Unity/Unreal SDKs expose true origin types (Pvr_SetTrackingOrigin) but there does not appear
// to be a way to access this from the Native SDK.  Pose information appears to be relative to the
// initial head pose.
static HeadsetOrigin pico_getOriginType(void) {
  return ORIGIN_HEAD;
}

static double pico_getDisplayTime(void) {
  return os_get_time();
}

static void pico_getDisplayDimensions(uint32_t* width, uint32_t* height) {
  *width = state.displayWidth;
  *height = state.displayHeight;
}

static const float* pico_getDisplayMask(uint32_t* count) {
  *count = 0;
  return NULL;
}

static uint32_t pico_getViewCount(void) {
  return 2;
}

static bool pico_getViewPose(uint32_t view, float* position, float* orientation) {
  vec3_init(position, state.headPosition);
  quat_init(orientation, state.headOrientation);
  position[1] += state.offset;
  return view < 2;
}

static bool pico_getViewAngles(uint32_t view, float* left, float* right, float* up, float* down) {
  *left = *right = *up = *down = state.fov;
  return view < 2;
}

static void pico_getClipDistance(float* clipNear, float* clipFar) {
  *clipNear = state.clipNear;
  *clipFar = state.clipFar;
}

static void pico_setClipDistance(float clipNear, float clipFar) {
  state.clipNear = clipNear;
  state.clipFar = clipFar;
}

// The Unity/Unreal SDKs expose something called "SeeThrough" that is very similar to the Oculus
// Guardian API, but this does not appear to be in the Native SDK
static void pico_getBoundsDimensions(float* width, float* depth) {
  *width = *depth = 0.f;
}

static const float* pico_getBoundsGeometry(uint32_t* count) {
  *count = 0;
  return NULL;
}

static bool pico_getPose(Device device, float* position, float* orientation) {
  if (device == DEVICE_HEAD) {
    vec3_init(position, state.headPosition);
    quat_init(orientation, state.headOrientation);
    position[1] += state.offset;
    return true;
  }

  if (device == DEVICE_HAND_LEFT || device == DEVICE_HAND_RIGHT) {
    uint32_t index = device - DEVICE_HAND_LEFT;
    vec3_init(position, state.controllers[index].position);
    quat_init(orientation, state.controllers[index].orientation);
    position[1] += state.offset;
    return state.controllers[index].active;
  }

  return false;
}

static bool pico_getVelocity(Device device, float* velocity, float* angularVelocity) {
  return false; // Controllers only expose acceleration and angular velocity, so we skip it
}

static bool pico_isDown(Device device, DeviceButton button, bool* down, bool* changed) {
  if (device != DEVICE_HAND_LEFT && device != DEVICE_HAND_RIGHT) {
    return false;
  }

  uint32_t index = device - DEVICE_HAND_LEFT;

  if (!state.controllers[index].active) {
    return false;
  }

  bool active = true;
  uint16_t mask = 0;

  switch (button) {
    case BUTTON_TRIGGER: mask = 1 << 0; break;
    case BUTTON_THUMBSTICK: mask = 1 << 1; break;
    case BUTTON_GRIP: mask = 1 << 2; break;
    case BUTTON_MENU: mask = 1 << 3; break;
    case BUTTON_A: mask = 1 << 4; active = index == 1; break;
    case BUTTON_X: mask = 1 << 4; active = index == 0; break;
    case BUTTON_B: mask = 1 << 5; active = index == 1; break;
    case BUTTON_Y: mask = 1 << 5; active = index == 0; break;
    default: return false;
  }

  *down = state.controllers[index].buttons & mask;
  *changed = state.controllers[index].changed & mask;
  return active;
}

static bool pico_isTouched(Device device, DeviceButton button, bool* touched) {
  return false;
}

static bool pico_getAxis(Device device, DeviceAxis axis, float* value) {
  if (device != DEVICE_HAND_LEFT && device != DEVICE_HAND_RIGHT) {
    return false;
  }

  uint32_t index = device - DEVICE_HAND_LEFT;

  if (!state.controllers[index].active) {
    return false;
  }

  switch (axis) {
    case AXIS_TRIGGER:
      *value = state.controllers[index].trigger;
      return true;
    case AXIS_THUMBSTICK:
      value[0] = state.controllers[index].thumbstick[0];
      value[1] = state.controllers[index].thumbstick[1];
      return true;
    default:
      return false;
  }
}

static bool pico_vibrate(Device device, float strength, float duration, float frequency) {
  if (device != DEVICE_HAND_LEFT && device != DEVICE_HAND_RIGHT) {
    return false;
  }

  uint32_t index = device - DEVICE_HAND_LEFT;

  state.controllers[index].hapticStrength = strength;
  state.controllers[index].hapticDuration = duration;
  return true;
}

static struct ModelData* pico_newModelData(Device device, bool animated) {
  return NULL;
}

static bool pico_animate(Device device, struct Model* model) {
  return false;
}

static void pico_renderTo(void (*callback)(void*), void* userdata) {
  state.renderCallback = callback;
  state.renderUserdata = userdata;
}

static void pico_update(float dt) {
  //
}

HeadsetInterface lovrHeadsetPicoDriver = {
  .driverType = DRIVER_PICO,
  .init = pico_init,
  .destroy = pico_destroy,
  .getName = pico_getName,
  .getOriginType = pico_getOriginType,
  .getDisplayTime = pico_getDisplayTime,
  .getDisplayDimensions = pico_getDisplayDimensions,
  .getDisplayMask = pico_getDisplayMask,
  .getViewCount = pico_getViewCount,
  .getViewPose = pico_getViewPose,
  .getViewAngles = pico_getViewAngles,
  .getClipDistance = pico_getClipDistance,
  .setClipDistance = pico_setClipDistance,
  .getBoundsDimensions = pico_getBoundsDimensions,
  .getBoundsGeometry = pico_getBoundsGeometry,
  .getPose = pico_getPose,
  .getVelocity = pico_getVelocity,
  .isDown = pico_isDown,
  .isTouched = pico_isTouched,
  .getAxis = pico_getAxis,
  .vibrate = pico_vibrate,
  .newModelData = pico_newModelData,
  .animate = pico_animate,
  .renderTo = pico_renderTo,
  .update = pico_update
};

// Activity callbacks

static lua_State* L;
static lua_State* T;
static Variant cookie;

static void lovrPicoBoot(void) {
  lovrAssert(os_init(), "Failed to initialize platform");

  L = luaL_newstate();
  luax_setmainthread(L);
  luaL_openlibs(L);

  lua_getglobal(L, "package");
  lua_getfield(L, -1, "preload");
  luax_register(L, lovrModules);
  lua_pop(L, 2);

  lua_pushcfunction(L, luax_getstack);
  if (luaL_loadbuffer(L, (const char*) src_resources_boot_lua, src_resources_boot_lua_len, "@boot.lua") || lua_pcall(L, 0, 1, -2)) {
    fprintf(stderr, "%s\n", lua_tostring(L, -1));
    return;
  }

  T = lua_newthread(L);
  lua_pushvalue(L, -2);
  lua_xmove(L, T, 1);

  lovrSetErrorCallback(luax_vthrow, T);
  lovrSetLogCallback(luax_vlog, T);
}

JNIEXPORT void JNICALL Java_org_lovr_app_Activity_lovrPicoOnCreate(JNIEnv* jni, jobject activity, jstring apk) {
  const char* path = (*jni)->GetStringUTFChars(jni, apk, NULL);
  size_t length = strlen(path);
  if (length < sizeof(apkPath)) {
    memcpy(apkPath, path, length);
  }
  lovrPicoBoot();
}

JNIEXPORT void JNICALL Java_org_lovr_app_Activity_lovrPicoSetDisplayDimensions(JNIEnv* jni, jobject activity, int width, int height) {
  state.displayWidth = width;
  state.displayHeight = height;
}

JNIEXPORT void JNICALL Java_org_lovr_app_Activity_lovrPicoUpdateControllerPose(JNIEnv* jni, jobject activity, int hand, bool active, float x, float y, float z, float qx, float qy, float qz, float qw) {
  state.controllers[hand].active = active;
  vec3_set(state.controllers[hand].position, x, y, z);
  quat_set(state.controllers[hand].orientation, -qx, -qy, qz, qw);
}

JNIEXPORT void JNICALL Java_org_lovr_app_Activity_lovrPicoUpdateControllerInput(JNIEnv* jni, jobject activity, int hand, int buttons, float trigger, float thumbstickX, float thumbstickY) {
  state.controllers[hand].changed = state.controllers[hand].buttons ^ buttons;
  state.controllers[hand].buttons = (uint16_t) buttons;
  state.controllers[hand].trigger = trigger;
  state.controllers[hand].thumbstick[0] = thumbstickX;
  state.controllers[hand].thumbstick[1] = thumbstickY;
}

JNIEXPORT void JNICALL Java_org_lovr_app_Activity_lovrPicoOnFrame(JNIEnv* jni, jobject activity, float x, float y, float z, float qx, float qy, float qz, float qw, float fov, float ipd) {
  vec3_set(state.headPosition, x, y, z);
  quat_set(state.headOrientation, qx, qy, qz, qw);
  state.fov = fov * M_PI / 180.f;
  state.ipd = ipd;

  // Haptics
  for (uint32_t i = 0; i < 2; i++) {
    if (state.controllers[i].hapticStrength > 0.f) {
      float strength = state.controllers[i].hapticStrength;
      float duration = state.controllers[i].hapticDuration;
      jclass class = (*jni)->GetObjectClass(jni, activity);
      jmethodID vibrate = (*jni)->GetMethodID(jni, class, "vibrate", "(IFF)V");
      (*jni)->CallObjectMethod(jni, activity, vibrate, i, strength, duration);
      state.controllers[i].hapticStrength = 0.f;
    }
  }

  // Resume the lovr.run coroutine, and if it returns (doesn't yield) then either reboot or exit
  if (L && T) {
    luax_geterror(T);
    luax_clearerror(T);
    if (luax_resume(T, 1) != LUA_YIELD) {
      bool restart = lua_type(T, 1) == LUA_TSTRING && !strcmp(lua_tostring(T, 1), "restart");
      if (restart) {
        luax_checkvariant(T, 2, &cookie);
        if (cookie.type == TYPE_OBJECT) {
          cookie.type = TYPE_NIL;
          memset(&cookie.value, 0, sizeof(cookie.value));
        }
        lua_close(L);
        lovrPicoBoot();
      } else {
        lua_close(L);
        L = NULL;
        T = NULL;

        // Call 'finish' method on the Activity
        jclass class = (*jni)->GetObjectClass(jni, activity);
        jmethodID finish = (*jni)->GetMethodID(jni, class, "finish", "()V");
        (*jni)->CallObjectMethod(jni, activity, finish);
      }
    }
  }
}

JNIEXPORT void JNICALL Java_org_lovr_app_Activity_lovrPicoDrawEye(JNIEnv* jni, jobject object, int eye) {
  if (!state.renderCallback) return;

  // Pico modifies a lot of global OpenGL state, including the framebuffer binding, VAO binding,
  // buffer bindings, blending, and depth test settings.  Since there is no swapchain or texture
  // submission API, we have to render into the currently active OpenGL framebuffer, so a cache of
  // native Canvas objects is used for that.  For the rest of the states, there is a new "nuke all
  // OpenGL state" function added to clear any changes made by Pico (lovrGpuResetState) :(

  GLint framebuffer;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer);

  Canvas* canvas = NULL;
  for (uint32_t i = 0; i < state.canvases.length; i++) {
    if (state.canvases.data[i].id == framebuffer) {
      canvas = state.canvases.data[i].instance;
      break;
    }
  }

  if (!canvas) {
    CanvasFlags flags = { .depth.enabled = true };
    canvas = lovrCanvasCreateFromHandle(state.displayWidth, state.displayHeight, flags, framebuffer, 0, 0, 1, true);
    arr_push(&state.canvases, ((NativeCanvas) { .id = framebuffer, .instance = canvas }));
  }

  // start each eye from origin
  lovrGraphicsOrigin();

  for (uint32_t i = 0; i < 2; i++) {
    float view[16];
    mat4_identity(view);
    mat4_translate(view, state.headPosition[0], state.headPosition[1] + state.offset, state.headPosition[2]);
    mat4_rotateQuat(view, state.headOrientation);
    mat4_translate(view, state.ipd * (eye == 0 ? -.5f : .5f), 0.f, 0.f);
    mat4_invert(view);
    lovrGraphicsSetViewMatrix(i, view);

    float projection[16];
    mat4_fov(projection, state.fov, state.fov, state.fov, state.fov, state.clipNear, state.clipFar);
    lovrGraphicsSetProjection(i, projection);
  }

  lovrGpuResetState();
  lovrGraphicsSetBackbuffer(canvas, false, true);
  state.renderCallback(state.renderUserdata);
  lovrGraphicsSetBackbuffer(NULL, false, false);
}
