#include "headset/headset.h"
#include "graphics/texture.h"
#include "glfw.h"
#include <stdbool.h>
#ifndef _WIN32
#define __stdcall
#endif
#include <openvr_capi.h>

#pragma once

typedef struct {
  int isInitialized;
  int isRendering;

  struct VR_IVRSystem_FnTable* system;
  struct VR_IVRCompositor_FnTable* compositor;
  struct VR_IVRChaperone_FnTable* chaperone;
  struct VR_IVRRenderModels_FnTable* renderModels;

  unsigned int headsetIndex;

  TrackedDevicePose_t renderPoses[16];
  RenderModel_t* deviceModels[16];
  RenderModel_TextureMap_t* deviceTextures[16];

  vec_controller_t controllers;

  float clipNear;
  float clipFar;

  uint32_t renderWidth;
  uint32_t renderHeight;

  Texture* texture;
} HeadsetState;

void lovrHeadsetRefreshControllers();
Controller* lovrHeadsetAddController(unsigned int id);
