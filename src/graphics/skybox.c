#include "graphics/skybox.h"
#include "lib/stb/stb_image.h"
#include <stdlib.h>

Skybox* lovrSkyboxCreate(void** data, size_t* size, SkyboxType type) {
  Skybox* skybox = lovrAlloc(sizeof(Skybox), lovrSkyboxDestroy);
  if (!skybox) return NULL;

  skybox->type = type;

  GLenum binding;
  int count;

  if (type == SKYBOX_CUBE) {
    binding = GL_TEXTURE_CUBE_MAP;
    count = 6;
  } else {
    binding = GL_TEXTURE_2D;
    count = 1;
  }

  glGenTextures(1, &skybox->texture);
  glBindTexture(binding, skybox->texture);

  for (int i = 0; i < count; i++) {
    int width, height, channels;
    stbi_set_flip_vertically_on_load(0);
    unsigned char* image = stbi_load_from_memory(data[i], size[i], &width, &height, &channels, 3);

    if (!image) {
      error("Could not load skybox image %d", i);
    }

    if (type == SKYBOX_CUBE) {
      glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
    } else {
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
    }

    free(image);
  }

  glTexParameteri(binding, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(binding, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(binding, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(binding, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  if (type == SKYBOX_CUBE) {
    glTexParameteri(binding, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
  }

  return skybox;
}

void lovrSkyboxDestroy(const Ref* ref) {
  Skybox* skybox = containerof(ref, Skybox);
  glDeleteTextures(1, &skybox->texture);
  free(skybox);
}
