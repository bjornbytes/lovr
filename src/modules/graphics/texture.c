#include "graphics/texture.h"

uint32_t lovrTextureGetWidth(Texture* texture, uint32_t mipmap) {
  return MAX(texture->width >> mipmap, 1);
}

uint32_t lovrTextureGetHeight(Texture* texture, uint32_t mipmap) {
  return MAX(texture->height >> mipmap, 1);
}

uint32_t lovrTextureGetDepth(Texture* texture, uint32_t mipmap) {
  return texture->type == TEXTURE_VOLUME ? MAX(texture->depth >> mipmap, 1) : texture->depth;
}

uint32_t lovrTextureGetMipmapCount(Texture* texture) {
  return texture->mipmapCount;
}

TextureType lovrTextureGetType(Texture* texture) {
  return texture->type;
}

TextureFormat lovrTextureGetFormat(Texture* texture) {
  return texture->format;
}

CompareMode lovrTextureGetCompareMode(Texture* texture) {
  return texture->compareMode;
}

TextureFilter lovrTextureGetFilter(Texture* texture) {
  return texture->filter;
}

TextureWrap lovrTextureGetWrap(Texture* texture) {
  return texture->wrap;
}
