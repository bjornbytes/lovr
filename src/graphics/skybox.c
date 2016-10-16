#include "skybox.h"
#include <stdlib.h>

Skybox* lovrSkyboxCreate(char** filenames) {
  Skybox* skybox = malloc(sizeof(Skybox));

  glGenTextures(1, &skybox->texture);
  glBindTexture(GL_TEXTURE_CUBE_MAP, skybox->texture);

  for (int i = 0; i < 6; i++) {
    int width, height, channels;
    unsigned char* data = stbi_load(filenames[i], &width, &height, &channels, 3);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
  }

  return skybox;
}

void lovrSkyboxDestroy(Skybox* skybox) {
  glDeleteTextures(1, skybox->texture);
  free(skybox);
}

void lovrSkyboxDraw(Skybox* skybox) {
  //
}
