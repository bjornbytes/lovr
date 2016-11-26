#include <stdbool.h>
#ifndef _WIN32
#define __stdcall
#endif
#include <openvr_capi.h>

#ifndef OPENVR_TYPES
#define OPENVR_TYPES
typedef struct {
  int isLoaded;
  RenderModel_t* model;
  RenderModel_TextureMap_t* texture;
} OpenVRModel;
#endif
