#include "skybox.h"
#include "graphics.h"
#include "../util.h"
#include <stdlib.h>

// For now
#define STB_IMAGE_IMPLEMENTATION
#include "../vendor/stb_image.h"

Skybox* lovrSkyboxCreate(void** data, int* size) {
  Skybox* skybox = malloc(sizeof(Skybox));

  glGenTextures(1, &skybox->texture);
  glBindTexture(GL_TEXTURE_CUBE_MAP, skybox->texture);

  for (int i = 0; i < 6; i++) {
    int width, height, channels;
    unsigned char* image = stbi_load_from_memory(data[i], size[i], &width, &height, &channels, 3);

    if (image == NULL) {
      error("Could not load skybox image %d", i);
    }

    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
    free(image);
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
