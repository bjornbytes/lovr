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

void lovrSkyboxDraw(Skybox* skybox, float angle, float ax, float ay, float az) {
  float cos2 = cos(angle / 2);
  float sin2 = sin(angle / 2);
  float rw = cos2;
  float rx = sin2 * ax;
  float ry = sin2 * ay;
  float rz = sin2 * az;

  lovrGraphicsPrepare();
  lovrGraphicsPush();
  lovrGraphicsOrigin();
  lovrGraphicsRotate(rw, rx, ry, rz);

  float cube[] = {
    // Front
    1.f, -1.f, -1.f, 0, 0, 0,
    1.f, 1.f, -1.f,  0, 0, 0,
    -1.f, -1.f, -1.f,0, 0, 0,
    -1.f, 1.f, -1.f, 0, 0, 0,

    // Left
    -1.f, 1.f, -1.f, 0, 0, 0,
    -1.f, 1.f, 1.f,  0, 0, 0,
    -1.f, -1.f, -1.f,0, 0, 0,
    -1.f, -1.f, 1.f, 0, 0, 0,

    // Back
    -1.f, -1.f, 1.f, 0, 0, 0,
    1.f, -1.f, 1.f,  0, 0, 0,
    -1.f, 1.f, 1.f,  0, 0, 0,
    1.f, 1.f, 1.f,   0, 0, 0,

    // Right
    1.f, 1.f, 1.f,   0, 0, 0,
    1.f, -1.f, 1.f,  0, 0, 0,
    1.f, 1.f, -1.f,  0, 0, 0,
    1.f, -1.f, -1.f, 0, 0, 0,

    // Bottom
    1.f, -1.f, -1.f, 0, 0, 0,
    1.f, -1.f, 1.f,  0, 0, 0,
    -1.f, -1.f, -1.f,0, 0, 0,
    -1.f, -1.f, 1.f, 0, 0, 0,

    // Adjust
    -1.f, -1.f, 1.f, 0, 0, 0,
    -1.f, 1.f, -1.f, 0, 0, 0,

    // Top
    -1.f, 1.f, -1.f, 0, 0, 0,
    -1.f, 1.f, 1.f,  0, 0, 0,
    1.f, 1.f, -1.f,  0, 0, 0,
    1.f, 1.f, 1.f,   0, 0, 0
  };

  glDepthMask(GL_FALSE);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_CUBE_MAP, skybox->texture);

  lovrGraphicsSetShapeData(cube, 156, NULL, 0);
  lovrGraphicsDrawFilledShape();

  glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
  glDepthMask(GL_TRUE);

  lovrGraphicsPop();
}
