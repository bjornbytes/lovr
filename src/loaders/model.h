#include "graphics/model.h"
#include <stdbool.h>
#ifndef _WIN32
#define __stdcall
#endif
#include <openvr_capi.h>

ModelData* lovrModelDataFromFile(void* data, int size);
ModelData* lovrModelDataFromOpenVRModel(RenderModel_t* renderModel);
