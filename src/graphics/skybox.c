#include "skybox.h"
#include "graphics.h"
#include "../util.h"
#include <stdlib.h>

// For now
#define STB_IMAGE_IMPLEMENTATION
#include "../vendor/stb_image.h"

Skybox* lovrSkyboxCreate(char** filenames) {
  Skybox* skybox = malloc(sizeof(Skybox));

  glGenTextures(1, &skybox->texture);
  glBindTexture(GL_TEXTURE_CUBE_MAP, skybox->texture);

  for (int i = 0; i < 6; i++) {
    int width, height, channels;
    unsigned char* data = stbi_load(filenames[i], &width, &height, &channels, 3);

    if (data == NULL) {
      error("Could not load image %s", filenames[i]);
    }

    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    free(data);
  }

  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  return skybox;
}

void lovrSkyboxDestroy(Skybox* skybox) {
  glDeleteTextures(1, &skybox->texture);
  free(skybox);
}
