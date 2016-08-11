#include "headset.h"

extern OSVR_ClientContext ctx;

typedef struct {
  OSVR_ClientInterface interface;
  OSVR_DisplayConfig displayConfig;
} HeadsetState;

static HeadsetState headsetState;

void lovrHeadsetInit() {
  if (ctx == NULL) {
    headsetState.interface = NULL;
    return;
  }

  osvrClientGetInterface(ctx, "/me/head", &headsetState.interface);
  osvrClientGetDisplay(ctx, &headsetState.displayConfig);
}

void lovrHeadsetGetPosition(float* x, float* y, float* z) {
  if (headsetState.interface) {
    OSVR_TimeValue timestamp;
    OSVR_PositionState state;
    osvrGetPositionState(headsetState.interface,  &timestamp, &state);
    *x = osvrVec3GetX(&state);
    *y = osvrVec3GetY(&state);
    *z = osvrVec3GetZ(&state);
  } else {
    *x = *y = *z = 0.f;
  }
}

void lovrHeadsetGetOrientation(float* w, float* x, float* y, float* z) {
  if (headsetState.interface) {
    OSVR_TimeValue timestamp;
    OSVR_OrientationState state;
    osvrGetOrientationState(headsetState.interface,  &timestamp, &state);
    *w = osvrQuatGetW(&state);
    *x = osvrQuatGetX(&state);
    *y = osvrQuatGetY(&state);
    *z = osvrQuatGetZ(&state);
  } else {
    *w = *x = *y = *z = 0.f;
  }
}

int lovrHeadsetIsPresent() {
  return headsetState.interface != NULL;
}

void lovrHeadsetRenderTo(headsetRenderCallback callback, void* userdata) {
  if (!headsetState.interface) {
    return;
  }

  OSVR_DisplayConfig displayConfig = headsetState.displayConfig;

  OSVR_ViewerCount viewerCount;
  osvrClientGetNumViewers(displayConfig, &viewerCount);

  for (OSVR_ViewerCount viewer = 0; viewer < viewerCount; viewer++) {
    OSVR_EyeCount eyeCount;
    osvrClientGetNumEyesForViewer(displayConfig, viewer, &eyeCount);

    for (OSVR_EyeCount eye = 0; eye < eyeCount; eye++) {
      double viewMatrix[OSVR_MATRIX_SIZE];
      osvrClientGetViewerEyeViewMatrixd(
        displayConfig, viewer, eye, OSVR_MATRIX_COLMAJOR | OSVR_MATRIX_COLVECTORS, viewMatrix
      );

      OSVR_SurfaceCount surfaceCount;
      osvrClientGetNumSurfacesForViewerEye(displayConfig, viewer, eye, &surfaceCount);
      for (OSVR_SurfaceCount surface = 0; surface < surfaceCount; surface++) {
        OSVR_ViewportDimension left, bottom, width, height;
        osvrClientGetRelativeViewportForViewerEyeSurface(
          displayConfig, viewer, eye, surface, &left, &bottom, &width, &height
        );

        double projectionMatrix[OSVR_MATRIX_SIZE];
        float near = 0.1f;
        float far = 100.0f;
        osvrClientGetViewerEyeSurfaceProjectionMatrixd(
          displayConfig, viewer, eye, surface, near, far,
          OSVR_MATRIX_COLMAJOR | OSVR_MATRIX_COLVECTORS | OSVR_MATRIX_SIGNEDZ | OSVR_MATRIX_RHINPUT,
          projectionMatrix
        );

        // Do something with viewMatrix, projectionMatrix, left, bottom, width, and height

        callback(eye, userdata);
      }
    }
  }
}
