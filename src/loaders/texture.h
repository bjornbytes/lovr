#include "graphics/texture.h"
#ifndef EMSCRIPTEN
#include "openvr.h"
#endif

TextureData* lovrTextureDataGetEmpty();
TextureData* lovrTextureDataFromFile(void* data, int size);
#ifndef EMSCRIPTEN
TextureData* lovrTextureDataFromOpenVRModel(OpenVRModel* vrModel);
#endif
