#include "texture.h"
#include "../util.h"
#include <stdlib.h>

Texture* lovrTextureCreate(void* data, int size) {
  Texture* texture = malloc(sizeof(Texture));
  if (!texture) return NULL;

  glGenTextures(1, &texture->id);

  if (data) {
    int channels;
    unsigned char* image = loadImage(data, size, &texture->width, &texture->height, &channels, 3);
    if (!image) {
      lovrTextureDestroy(texture);
      return NULL;
    }

    glBindTexture(GL_TEXTURE_2D, texture->id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, texture->width, texture->height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    free(image);
  }

  return texture;
}

void lovrTextureDestroy(Texture* texture) {
  glDeleteTextures(1, &texture->id);
  free(texture);
}

int lovrTextureGetHeight(Texture* texture) {
  return texture->height;
}

int lovrTextureGetWidth(Texture* texture) {
  return texture->width;
}
