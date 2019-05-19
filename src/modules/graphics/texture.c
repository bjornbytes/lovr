#include "graphics/texture.h"

int lovrTextureGetWidth(Texture* texture, int mipmap) {
  return MAX(texture->width >> mipmap, 1);
}

int lovrTextureGetHeight(Texture* texture, int mipmap) {
  return MAX(texture->height >> mipmap, 1);
}

int lovrTextureGetDepth(Texture* texture, int mipmap) {
  return texture->type == TEXTURE_VOLUME ? MAX(texture->depth >> mipmap, 1) : texture->depth;
}

int lovrTextureGetMipmapCount(Texture* texture) {
  return texture->mipmapCount;
}

TextureType lovrTextureGetType(Texture* texture) {
  return texture->type;
}

TextureFormat lovrTextureGetFormat(Texture* texture) {
  return texture->format;
}

TextureFilter lovrTextureGetFilter(Texture* texture) {
  return texture->filter;
}

TextureWrap lovrTextureGetWrap(Texture* texture) {
  return texture->wrap;
}
