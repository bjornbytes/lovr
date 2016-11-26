#include "graphics/texture.h"
#include <stdbool.h>
#ifndef _WIN32
#define __stdcall
#endif
#include <openvr_capi.h>

TextureData* lovrTextureDataFromFile(void* data, int size);
TextureData* lovrTextureDataFromOpenVRModel(RenderModel_t* renderModel);
