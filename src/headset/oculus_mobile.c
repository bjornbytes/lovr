#include "lib/glfw.h"
#include "headset/headset.h"
#include "BridgeLovr.h"
#include "math/quat.h"
#include "graphics/graphics.h"

void lovrHeadsetInit(HeadsetDriver* drivers, int count) {
}

void lovrHeadsetDestroy() {
}

static HeadsetDriver driver = DRIVER_OVR_MOBILE;

const HeadsetDriver* lovrHeadsetGetDriver() {
  return &driver;
}

void lovrHeadsetPoll() {
}

bool lovrHeadsetIsPresent() {
  return true; // Can't not ?!?
}

HeadsetType lovrHeadsetGetType() {
  return HEADSET_RIFT;
}

HeadsetOrigin lovrHeadsetGetOriginType() {
  return ORIGIN_FLOOR;
}

bool lovrHeadsetIsMirrored() {
  return false; // Can't ever??
}

void lovrHeadsetSetMirrored(bool mirrored) {
  // Don't care?
}

void lovrHeadsetGetDisplayDimensions(int* width, int* height) {
  *width = bridgeLovrMobileData.displayDimensions.width;
  *height = bridgeLovrMobileData.displayDimensions.height;
  return;
}

// TODO
void lovrHeadsetGetClipDistance(float* near, float* far) {
  *near = *far = 0.f;
  return;
}

/// TODO
void lovrHeadsetSetClipDistance(float near, float far) {
}

// TODO
float lovrHeadsetGetBoundsWidth() {
  return 0.f;
}

// TODO
float lovrHeadsetGetBoundsDepth() {
  return 0.f;
}

// TODO
void lovrHeadsetGetPose(float* x, float* y, float* z, float* angle, float* ax, float* ay, float* az) {
  *x = bridgeLovrMobileData.lastHeadPose.x;
  *y = bridgeLovrMobileData.lastHeadPose.y;
  *z = bridgeLovrMobileData.lastHeadPose.z;

  quat_getAngleAxis(bridgeLovrMobileData.lastHeadPose.q, angle, ax, ay, az);
}

// TODO
void lovrHeadsetGetEyePose(HeadsetEye eye, float* x, float* y, float* z, float* angle, float* ax, float* ay, float* az) {
  // This is very wrong!
  lovrHeadsetGetPose(x, y, z, angle, ax, ay, az);
  return;
}

// TODO
void lovrHeadsetGetVelocity(float* x, float* y, float* z) {
  *x = bridgeLovrMobileData.lastHeadVelocity.x;
  *y = bridgeLovrMobileData.lastHeadVelocity.y;
  *z = bridgeLovrMobileData.lastHeadVelocity.z;
  return;
}

// TODO
void lovrHeadsetGetAngularVelocity(float* x, float* y, float* z) {
  *x = bridgeLovrMobileData.lastHeadVelocity.ax;
  *y = bridgeLovrMobileData.lastHeadVelocity.ay;
  *z = bridgeLovrMobileData.lastHeadVelocity.az;
  return;
}

// TODO
vec_controller_t* lovrHeadsetGetControllers() {
  return NULL;
}

// TODO
bool lovrHeadsetControllerIsPresent(Controller* controller) {
  return false;
}

// TODO
ControllerHand lovrHeadsetControllerGetHand(Controller* controller) {
  return HAND_UNKNOWN;
}

// TODO
void lovrHeadsetControllerGetPose(Controller* controller, float* x, float* y, float* z, float* angle, float* ax, float* ay, float* az) {
  *x = *y = *z = *angle = *ax = *ay = *az = 0.f;
  return;
}

// TODO
float lovrHeadsetControllerGetAxis(Controller* controller, ControllerAxis axis) {
  return 0.f;
}

// TODO
bool lovrHeadsetControllerIsDown(Controller* controller, ControllerButton button) {
  return false;
}

// TODO
bool lovrHeadsetControllerIsTouched(Controller* controller, ControllerButton button) {
  return false;
}

// TODO
void lovrHeadsetControllerVibrate(Controller* controller, float duration, float power) {
  return;
}

// TODO
ModelData* lovrHeadsetControllerNewModelData(Controller* controller) {
  return NULL;
}

// TODO
static headsetRenderCallback renderCallback;
static void* renderUserdata;
void lovrHeadsetRenderTo(headsetRenderCallback callback, void* userdata) {
  renderCallback = callback;
  renderUserdata = userdata;
}

void lovrOculusMobileDraw(int eye, int framebuffer, int width, int height, float *eyeViewMatrix, float *projectionMatrix) {
  lovrGraphicsPush();
  lovrGraphicsBindFramebuffer(framebuffer);
  lovrGraphicsMatrixTransform(MATRIX_VIEW, eyeViewMatrix);
  lovrGraphicsSetProjection(projectionMatrix);
  lovrGraphicsClear(true, true, true, lovrGraphicsGetBackgroundColor(), 1., 0); // Needed?

  lovrGraphicsSetViewport(0, 0, width, height);
  glScissor(0, 0, width, height);
  
  renderCallback(eye, renderUserdata);

  lovrGraphicsPop();
}

// TODO
void lovrHeadsetUpdate(float dt) {
}

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
