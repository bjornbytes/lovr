#include "lib/glfw.h"
#include "headset/headset.h"
#include "BridgeLovr.h"
#include "math/quat.h"
#include "graphics/graphics.h"
#include "math/mat4.h"
#include "lib/glad/glad.h"

static void (*renderCallback)(void*);
static void* renderUserdata;

void lovrOculusMobileDraw(int framebuffer, int width, int height, float *eyeViewMatrix, float *projectionMatrix) {
  CanvasFlags flags = {0};
  Canvas *canvas = lovrCanvasCreateFromHandle(width, height, flags, framebuffer, 1);

  Camera camera = { .canvas = canvas, .stereo = false };
  memcpy(camera.viewMatrix[0], eyeViewMatrix, sizeof(camera.viewMatrix[0]));
  memcpy(camera.projection[0], projectionMatrix, sizeof(camera.projection[0]));

  lovrGraphicsSetCamera(&camera, true);

  renderCallback(renderUserdata);

  lovrGraphicsSetCamera(NULL, false);
  lovrRelease(canvas);
}

// TODO
void lovrHeadsetUpdate(float dt) {
}

// Headset driver object

static bool oculusMobileInit(float offset, int msaa) {
  return true;
}

static void oculusMobileDestroy() {
}

static HeadsetType oculusMobileGetType() {
  return HEADSET_OCULUS_MOBILE;
}

static HeadsetOrigin oculusMobileGetOriginType() {
  return ORIGIN_FLOOR;
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
  *x = bridgeLovrMobileData.lastHeadPose.x;
  *y = bridgeLovrMobileData.lastHeadPose.y;
  *z = bridgeLovrMobileData.lastHeadPose.z;

  quat_getAngleAxis(bridgeLovrMobileData.lastHeadPose.q, angle, ax, ay, az);
}

static void oculusMobileGetEyePose(HeadsetEye eye, float* x, float* y, float* z, float* angle, float* ax, float* ay, float* az) {
  // TODO: This is very wrong!
  oculusMobileGetPose(x, y, z, angle, ax, ay, az);
}

static void oculusMobileGetVelocity(float* x, float* y, float* z) {
  *x = bridgeLovrMobileData.lastHeadVelocity.x;
  *y = bridgeLovrMobileData.lastHeadVelocity.y;
  *z = bridgeLovrMobileData.lastHeadVelocity.z;
}

static void oculusMobileGetAngularVelocity(float* x, float* y, float* z) {
  *x = bridgeLovrMobileData.lastHeadVelocity.ax;
  *y = bridgeLovrMobileData.lastHeadVelocity.ay;
  *z = bridgeLovrMobileData.lastHeadVelocity.az;
}

static Controller** oculusMobileGetControllers(uint8_t* count) {
  *count = 0; // TODO
  return NULL;
}

static bool oculusMobileControllerIsConnected(Controller* controller) {
  return false;
}

static ControllerHand oculusMobileControllerGetHand(Controller* controller) {
  return HAND_UNKNOWN;
}

static void oculusMobileControllerGetPose(Controller* controller, float* x, float* y, float* z, float* angle, float* ax, float* ay, float* az) {
}

static float oculusMobileControllerGetAxis(Controller* controller, ControllerAxis axis) {
  return 0;
}

static bool oculusMobileControllerIsDown(Controller* controller, ControllerButton button) {
  return 0;
}

static bool oculusMobileControllerIsTouched(Controller* controller, ControllerButton button) {
  return false;
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

void glfwSetTime(double time) {
}

double glfwGetTime(void) {
	
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