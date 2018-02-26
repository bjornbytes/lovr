#include "graphics/canvas.h"
#include "graphics/graphics.h"
#include "math/mat4.h"
#include <math.h>
#include <stdlib.h>

bool lovrCanvasSupportsFormat(TextureFormat format) {
  switch (format) {
    case FORMAT_RGB:
    case FORMAT_RGBA:
    case FORMAT_RGBA16F:
    case FORMAT_RGBA32F:
    case FORMAT_RG11B10F:
      return true;
    case FORMAT_DXT1:
    case FORMAT_DXT3:
    case FORMAT_DXT5:
      return false;
  }
}

Canvas* lovrCanvasCreate(int width, int height, TextureFormat format, int msaa, bool depth, bool stencil) {
  TextureData* textureData = lovrTextureDataGetEmpty(width, height, format);
  Texture* texture = lovrTextureCreate(TEXTURE_2D, &textureData, 1, true, false);
  if (!texture) return NULL;

  Canvas* canvas = lovrAlloc(sizeof(Canvas), lovrCanvasDestroy);
  canvas->texture = *texture;
  canvas->msaa = msaa;
  canvas->framebuffer = 0;
  canvas->resolveFramebuffer = 0;
  canvas->depthStencilBuffer = 0;
  canvas->msaaTexture = 0;

  // Framebuffer
  glGenFramebuffers(1, &canvas->framebuffer);
  glBindFramebuffer(GL_FRAMEBUFFER, canvas->framebuffer);

  // Color attachment
  if (msaa > 0) {
    GLenum internalFormat = lovrTextureFormatGetGLInternalFormat(format, lovrGraphicsIsGammaCorrect());
    glGenRenderbuffers(1, &canvas->msaaTexture);
    glBindRenderbuffer(GL_RENDERBUFFER, canvas->msaaTexture);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, msaa, internalFormat, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, canvas->msaaTexture);
  } else {
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, canvas->texture.id, 0);
  }

  // Depth/Stencil
  if (depth || stencil) {
    GLenum depthStencilFormat = stencil ? GL_DEPTH24_STENCIL8 : GL_DEPTH_COMPONENT24;
    glGenRenderbuffers(1, &canvas->depthStencilBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, canvas->depthStencilBuffer);
    if (msaa > 0) {
      glRenderbufferStorageMultisample(GL_RENDERBUFFER, msaa, depthStencilFormat, width, height);
    } else {
      glRenderbufferStorage(GL_RENDERBUFFER, depthStencilFormat, width, height);
    }

    if (depth) {
      glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, canvas->depthStencilBuffer);
    }

    if (stencil) {
      glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, canvas->depthStencilBuffer);
    }
  }

  // Resolve framebuffer
  if (msaa > 0) {
    glGenFramebuffers(1, &canvas->resolveFramebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, canvas->resolveFramebuffer);
    glBindTexture(GL_TEXTURE_2D, canvas->texture.id);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, canvas->texture.id, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, canvas->framebuffer);
  }

  lovrAssert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Error creating Canvas");
  lovrGraphicsClear(true, true, true, (Color) { 0, 0, 0, 0 }, 1., 0);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  return canvas;
}

void lovrCanvasDestroy(void* ref) {
  Canvas* canvas = ref;
  glDeleteFramebuffers(1, &canvas->framebuffer);
  if (canvas->resolveFramebuffer) {
    glDeleteFramebuffers(1, &canvas->resolveFramebuffer);
  }
  if (canvas->depthStencilBuffer) {
    glDeleteRenderbuffers(1, &canvas->depthStencilBuffer);
  }
  if (canvas->msaaTexture) {
    glDeleteTextures(1, &canvas->msaaTexture);
  }
  lovrTextureDestroy(ref);
}

TextureFormat lovrCanvasGetFormat(Canvas* canvas) {
  return canvas->texture.slices[0]->format;
}

int lovrCanvasGetMSAA(Canvas* canvas) {
  return canvas->msaa;
}
