#include "graphics/texture.h"
#include "openvr.h"
#include <stdint.h>

TextureData* lovrTextureDataGetBlank(int width, int height, uint8_t value, TextureFormat format);
TextureData* lovrTextureDataGetEmpty(int width, int height, TextureFormat format);
TextureData* lovrTextureDataFromFile(void* data, int size);
TextureData* lovrTextureDataFromOpenVRModel(OpenVRModel* vrModel);
void lovrTextureDataResize(TextureData* textureData, int width, int height, uint8_t value);
