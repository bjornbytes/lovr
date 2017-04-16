#include "headset/headset.h"
#include "graphics/graphics.h"
#include <emscripten.h>
#include <emscripten/vr.h>

static headsetRenderCallback renderCallback;

static void onRequestAnimationFrame(void* userdata) {
  lovrGraphicsSetBackgroundColor(1, 0, 0, 1);
  lovrGraphicsClear(1, 1);
  printf("Yay rendering!\n");
  int width = emscripten_vr_get_display_width();
  int height = emscripten_vr_get_display_height();

  glViewport(0, 0, width, height);
  renderCallback(EYE_LEFT, userdata);

  glViewport(width, 0, width, height);
  renderCallback(EYE_RIGHT, userdata);
}

void lovrHeadsetInit() {
  emscripten_vr_init();
}

void lovrHeadsetDestroy() {
  //
}

void lovrHeadsetPoll() {
  //
}

int lovrHeadsetIsPresent() {
  return emscripten_vr_is_present();
}

const char* lovrHeadsetGetType() {
  return "WebVR";
}

int lovrHeadsetIsMirrored() {
  return 0;
}

void lovrHeadsetSetMirrored(int mirror) {
  //
}

void lovrHeadsetGetDisplayDimensions(int* width, int* height) {
  *width = *height = 0;
}

void lovrHeadsetGetClipDistance(float* near, float* far) {
  *near = *far = 0;
}

void lovrHeadsetSetClipDistance(float near, float far) {
  //
}

float lovrHeadsetGetBoundsWidth() {
  return 0;
}

float lovrHeadsetGetBoundsDepth() {
  return 0;
}

void lovrHeadsetGetBoundsGeometry(float* geometry) {
  geometry = NULL;
}

char lovrHeadsetIsBoundsVisible() {
  return 0;
}

void lovrHeadsetSetBoundsVisible(char visible) {
  //
}

void lovrHeadsetGetPosition(float* x, float* y, float* z) {
  *x = *y = *z = 0;
}

void lovrHeadsetGetEyePosition(HeadsetEye eye, float* x, float* y, float* z) {
  *x = *y = *z = 0;
}

void lovrHeadsetGetOrientation(float* angle, float* x, float* y, float* z) {
  *angle = *x = *y = *z = 0;
}

void lovrHeadsetGetVelocity(float* x, float* y, float* z) {
  *x = *y = *z = 0;
}

void lovrHeadsetGetAngularVelocity(float* x, float* y, float* z) {
  *x = *y = *z = 0;
}

vec_controller_t* lovrHeadsetGetControllers() {
  return NULL;
}

int lovrHeadsetControllerIsPresent(Controller* controller) {
  return 0;
}

void lovrHeadsetControllerGetPosition(Controller* controller, float* x, float* y, float* z) {
  *x = *y = *z = 0;
}

void lovrHeadsetControllerGetOrientation(Controller* controller, float* angle, float* x, float* y, float* z) {
  *angle = *x = *y = *z = 0;
}

float lovrHeadsetControllerGetAxis(Controller* controller, ControllerAxis axis) {
  return 0;
}

int lovrHeadsetControllerIsDown(Controller* controller, ControllerButton button) {
  return 0;
}

void lovrHeadsetControllerVibrate(Controller* controller, float duration) {
  //
}

ModelData* lovrHeadsetControllerNewModelData(Controller* controller) {
  return NULL;
}

TextureData* lovrHeadsetControllerNewTextureData(Controller* controller) {
  return NULL;
}

void lovrHeadsetRenderTo(headsetRenderCallback callback, void* userdata) {
  renderCallback = callback;
  emscripten_vr_set_render_callback(onRequestAnimationFrame, userdata);
}
