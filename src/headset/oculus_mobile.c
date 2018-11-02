#include "lib/glfw.h"
#include "headset/headset.h"
#include "BridgeLovr.h"
#include "math/quat.h"
#include "graphics/graphics.h"
#include "math/mat4.h"
#include "lib/glad/glad.h"

static void (*renderCallback)(void*);
static void* renderUserdata;

static float offset;

void lovrOculusMobileDraw(int framebuffer, int width, int height, float *eyeViewMatrix, float *projectionMatrix) {
  lovrGpuDirtyTexture();

  CanvasFlags flags = {0};
  Canvas *canvas = lovrCanvasCreateFromHandle(width, height, flags, framebuffer, 0, 0, 1, true);

  Camera camera = { .canvas = canvas, .stereo = false };
  memcpy(camera.viewMatrix[0], eyeViewMatrix, sizeof(camera.viewMatrix[0]));
  mat4_translate(camera.viewMatrix[0], 0, -offset, 0);

  memcpy(camera.projection[0], projectionMatrix, sizeof(camera.projection[0]));

  lovrGraphicsSetCamera(&camera, true);

  if (renderCallback)
    renderCallback(renderUserdata);

  lovrGraphicsSetCamera(NULL, false);
  lovrRelease(canvas);
}

// Headset driver object

static bool oculusMobileInit(float _offset, int msaa) {
  offset = _offset;
  return true;
}

static void oculusMobileDestroy() {
}

static HeadsetType oculusMobileGetType() {
  return HEADSET_OCULUS_MOBILE;
}

static HeadsetOrigin oculusMobileGetOriginType() {
  return ORIGIN_HEAD;
}

static bool oculusMobileIsMounted() {
  return true; // ???
}

static void oculusMobileIsMirrored(bool* mirrored, HeadsetEye * eye) {
  *mirrored = false; // Can't ever??
  *eye = false;
}

static void oculusMobileSetMirrored(bool mirror, HeadsetEye eye) {
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
  *width = 0;
  *depth = 0;
}

static const float* oculusMobileGetBoundsGeometry(int* count) {
  *count = 0;
  return NULL;
}

static void oculusMobileGetPose(float* x, float* y, float* z, float* angle, float* ax, float* ay, float* az) {
  *x = bridgeLovrMobileData.updateData.lastHeadPose.x;
  *y = bridgeLovrMobileData.updateData.lastHeadPose.y;
  *z = bridgeLovrMobileData.updateData.lastHeadPose.z;

  quat_getAngleAxis(bridgeLovrMobileData.updateData.lastHeadPose.q, angle, ax, ay, az);
}

static void oculusMobileGetEyePose(HeadsetEye eye, float* x, float* y, float* z, float* angle, float* ax, float* ay, float* az) {
  // TODO: This is very wrong!
  oculusMobileGetPose(x, y, z, angle, ax, ay, az);
}

static void oculusMobileGetVelocity(float* x, float* y, float* z) {
  *x = bridgeLovrMobileData.updateData.lastHeadVelocity.x;
  *y = bridgeLovrMobileData.updateData.lastHeadVelocity.y;
  *z = bridgeLovrMobileData.updateData.lastHeadVelocity.z;
}

static void oculusMobileGetAngularVelocity(float* x, float* y, float* z) {
  *x = bridgeLovrMobileData.updateData.lastHeadVelocity.ax;
  *y = bridgeLovrMobileData.updateData.lastHeadVelocity.ay;
  *z = bridgeLovrMobileData.updateData.lastHeadVelocity.az;
}

static Controller *controller;

static Controller** oculusMobileGetControllers(uint8_t* count) {
  if (!controller)
    controller = lovrAlloc(Controller, free);
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
  *y = bridgeLovrMobileData.updateData.goPose.y;
  *z = bridgeLovrMobileData.updateData.goPose.z;

  quat_getAngleAxis(bridgeLovrMobileData.updateData.goPose.q, angle, ax, ay, az);
}

static float oculusMobileControllerGetAxis(Controller* controller, ControllerAxis axis) {
  switch (axis) {
    case CONTROLLER_AXIS_TOUCHPAD_X:
      return (bridgeLovrMobileData.updateData.goTrackpad.x-160)/160.0;
    case CONTROLLER_AXIS_TOUCHPAD_Y:
      return (bridgeLovrMobileData.updateData.goTrackpad.y-160)/160.0;
  }
}

static bool buttonCheck(BridgeLovrButton field, ControllerButton button) {
  switch (button) {
    case CONTROLLER_BUTTON_MENU:
      return field & BridgeLovrButtonMenu;
    case CONTROLLER_BUTTON_TRIGGER:
      return field & BridgeLovrButtonShoulder;
    case CONTROLLER_BUTTON_TOUCHPAD:
      return field & BridgeLovrButtonTouchpad;
    default:
      return false;
  }

}

static bool oculusMobileControllerIsDown(Controller* controller, ControllerButton button) {
  return buttonCheck(bridgeLovrMobileData.updateData.goButtonDown, button);
}

static bool oculusMobileControllerIsTouched(Controller* controller, ControllerButton button) {
  return buttonCheck(bridgeLovrMobileData.updateData.goButtonTouch, button);
}

static void oculusMobileControllerVibrate(Controller* controller, float duration, float power) {
}

static ModelData* oculusMobileControllerNewModelData(Controller* controller) {
  return NULL;
}

// TODO: need to set up swap chain textures for the eyes and finish view transforms
static void oculusMobileRenderTo(void (*callback)(void*), void* userdata) {
  renderCallback = callback;
  renderUserdata = userdata;
}

static void oculusMobileUpdate(float dt) {
}

HeadsetInterface lovrHeadsetOculusMobileDriver = {
  DRIVER_OCULUS_MOBILE,
  oculusMobileInit,
  oculusMobileDestroy,
  oculusMobileGetType,
  oculusMobileGetOriginType,
  oculusMobileIsMounted,
  oculusMobileIsMirrored,
  oculusMobileSetMirrored,
  oculusMobileGetDisplayDimensions,
  oculusMobileGetClipDistance,
  oculusMobileSetClipDistance,
  oculusMobileGetBoundsDimensions,
  oculusMobileGetBoundsGeometry,
  oculusMobileGetPose,
  oculusMobileGetEyePose,
  oculusMobileGetVelocity,
  oculusMobileGetAngularVelocity,
  oculusMobileGetControllers,
  oculusMobileControllerIsConnected,
  oculusMobileControllerGetHand,
  oculusMobileControllerGetPose,
  oculusMobileControllerGetAxis,
  oculusMobileControllerIsDown,
  oculusMobileControllerIsTouched,
  oculusMobileControllerVibrate,
  oculusMobileControllerNewModelData,
  oculusMobileRenderTo,
  oculusMobileUpdate
};

// Pseudo GLFW

void glfwPollEvents() {

}

GLFWAPI void glfwGetCursorPos(GLFWwindow* window, double* xpos, double* ypos) {
  *xpos = 0;
  *ypos = 0;
}

GLFWwindow* glfwGetCurrentContext(void) {
  return NULL;
}

static double timeOffset;

void glfwSetTime(double time) {
  timeOffset = time - bridgeLovrMobileData.updateData.displayTime;
}
double glfwGetTime(void) {
  return bridgeLovrMobileData.updateData.displayTime - timeOffset;
}

static GLFWerrorfun lastErrFun;
GLFWerrorfun glfwSetErrorCallback(GLFWerrorfun cbfun)
{
  GLFWerrorfun cb = lastErrFun;
  lastErrFun = cbfun;
  return cb;
}

int glfwInit(void) {
  return 1;
}
void glfwTerminate(void) {

}

GLFWAPI void glfwGetFramebufferSize(GLFWwindow* window, int* width, int* height)
{
  *width = bridgeLovrMobileData.displayDimensions.width;
  *height = bridgeLovrMobileData.displayDimensions.height; 
}