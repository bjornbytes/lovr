#include "graphics/texture.h"
#include "openvr.h"
#include <stdint.h>

TextureData* lovrTextureDataGetEmpty(int width, int height, uint8_t value);
TextureData* lovrTextureDataFromFile(void* data, int size);
TextureData* lovrTextureDataFromOpenVRModel(OpenVRModel* vrModel);
