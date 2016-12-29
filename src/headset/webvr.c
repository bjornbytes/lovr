#include "headset/headset.h"
#include "event/event.h"
#include "util.h"
#include <stdlib.h>
#include <emscripten.h>
#include <emscripten/vr.h>

typedef struct {
  WebVRDisplay display;
  int isReady;
  headsetRenderCallback renderCallback;
  int renderCallbackId;
} WebVRState;

static WebVRState state;

void lovrHeadsetInit() {
  state.isReady = 0;
  state.renderCallback = NULL;
  state.renderCallbackId = 0;
  emscripten_vr_init();
  lovrEventAddPump(lovrHeadsetPoll);
}

void lovrHeadsetDestroy() {
  if (state.isReady && state.renderCallbackId) {
    emscripten_vr_cancel_animation_frame(state.display.displayId, state.renderCallbackId);
  }
}

// Because navigator.getVRDisplays is async, we need to poll for successful initialization
void lovrHeadsetPoll() {
  if (!state.isReady && emscripten_vr_ready() && emscripten_vr_get_display_count() > 0) {
    state.isReady = !emscripten_vr_get_display(0, &state.display);
  }
}

char lovrHeadsetIsPresent() {
  return (char) state.isReady;
}

const char* lovrHeadsetGetType() {
  if (!state.isReady) {
    return NULL;
  }

  return state.display.displayName;
}

void lovrHeadsetGetDisplayDimensions(int* width, int* height) {
  WebVREyeParameters eyeParameters;
  if (!state.isReady || emscripten_vr_get_eye_parameters(state.display.displayId, &eyeParameters)) {
    *width = *height = 0;
    return;
  }

  *width = eyeParameters.renderWidth;
  *height = eyeParameters.renderHeight;
}

void lovrHeadsetGetClipDistance(float* near, float* far) {
  if (!state.isReady) {
    return;
  }

  *near = state.display.depthNear;
  *far = state.display.depthFar;
}

void lovrHeadsetSetClipDistance(float near, float far) {
  // TODO
}

float lovrHeadsetGetBoundsWidth() {
  if (!state.isReady) {
    return 0.f;
  }

  return state.display.stageParameters.sizeX;
}

float lovrHeadsetGetBoundsDepth() {
  if (!state.isReady) {
    return 0.f;
  }

  return state.display.stageParameters.sizeZ;
}

// TODO
void lovrHeadsetGetBoundsGeometry(float* geometry) {
  geometry = NULL;
}

// TODO
char lovrHeadsetIsBoundsVisible() {
  return 0;
}

// TODO
void lovrHeadsetSetBoundsVisible(char visible) {
  return;
}

void lovrHeadsetGetPosition(float* x, float* y, float* z) {
  WebVRFrameData frameData;
  if (!state.isReady || emscripten_vr_get_frame_data(state.display.displayId, &frameData)) {
    *x = *y = *z = 0.f;
  }

  if (frameData.pose.position) {
    *x = frameData.pose.position[0];
    *y = frameData.pose.position[1];
    *z = frameData.pose.position[2];
  } else {
    *x = *y = *z = 0.f;
  }
}

void lovrHeadsetGetOrientation(float* w, float* x, float* y, float* z) {
  WebVRFrameData frameData;
  if (!state.isReady || emscripten_vr_get_frame_data(state.display.displayId, &frameData)) {
    *w = *x = *y = *z = 0.f;
  }

  if (frameData.pose.orientation) {
    *w = frameData.pose.orientation[0];
    *x = frameData.pose.orientation[1];
    *y = frameData.pose.orientation[2];
    *z = frameData.pose.orientation[3];
  } else {
    *w = *x = *y = *z = 0.f;
  }
}

void lovrHeadsetGetVelocity(float* x, float* y, float* z) {
  WebVRFrameData frameData;
  if (!state.isReady || emscripten_vr_get_frame_data(state.display.displayId, &frameData)) {
    *x = *y = *z = 0.f;
  }

  if (frameData.pose.linearVelocity) {
    *x = frameData.pose.linearVelocity[0];
    *y = frameData.pose.linearVelocity[1];
    *z = frameData.pose.linearVelocity[2];
  } else {
    *x = *y = *z = 0.f;
  }
}

void lovrHeadsetGetAngularVelocity(float* x, float* y, float* z) {
  WebVRFrameData frameData;
  if (!state.isReady || emscripten_vr_get_frame_data(state.display.displayId, &frameData)) {
    *x = *y = *z = 0.f;
  }

  if (frameData.pose.angularVelocity) {
    *x = frameData.pose.angularVelocity[0];
    *y = frameData.pose.angularVelocity[1];
    *z = frameData.pose.angularVelocity[2];
  } else {
    *x = *y = *z = 0.f;
  }
}

static void onAnimationFrame(void* userdata) {
  int display = state.display.displayId;
  state.renderCallbackId = emscripten_vr_request_animation_frame(display, onAnimationFrame, userdata);
  state.renderCallback(0, userdata);
  state.renderCallback(1, userdata);
}

void lovrHeadsetRenderTo(headsetRenderCallback callback, void* userdata) {
  if (state.isReady && callback != state.renderCallback) {
    if (state.renderCallbackId) {
      emscripten_vr_cancel_animation_frame(state.display.displayId, state.renderCallbackId);
    }

    int display = state.display.displayId;
    state.renderCallback = callback;
    state.renderCallbackId = emscripten_vr_request_animation_frame(display, onAnimationFrame, userdata);
  }
}
