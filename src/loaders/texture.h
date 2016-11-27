#include "graphics/texture.h"
#include "openvr.h"

TextureData* lovrTextureDataGetEmpty();
TextureData* lovrTextureDataFromFile(void* data, int size);
TextureData* lovrTextureDataFromOpenVRModel(OpenVRModel* vrModel);
