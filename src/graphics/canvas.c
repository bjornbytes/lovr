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

Canvas* lovrCanvasCreate(int width, int height, TextureFormat format, CanvasType type, int msaa, bool depth, bool stencil) {
  TextureData* textureData = lovrTextureDataGetEmpty(width, height, format);
  Texture* texture = lovrTextureCreate(TEXTURE_2D, &textureData, 1, true);
  if (!texture) return NULL;

  Canvas* canvas = lovrAlloc(sizeof(Canvas), lovrCanvasDestroy);
  canvas->texture = *texture;
  canvas->type = type;
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
  lovrGraphicsClear(true, true);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  return canvas;
}

void lovrCanvasDestroy(const Ref* ref) {
  Canvas* canvas = (Canvas*) containerof(ref, Texture);
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

void lovrCanvasBind(Canvas* canvas) {
  int width = canvas->texture.width;
  int height = canvas->texture.height;
  lovrGraphicsBindFramebuffer(canvas->framebuffer);
  lovrGraphicsSetViewport(0, 0, width, height);

  if (canvas->type == CANVAS_2D) {
    float projection[16];
    mat4_orthographic(projection, 0, width, 0, height, -1, 1);
    lovrGraphicsSetProjection(projection);
  } else {
    mat4 projection = lovrGraphicsGetProjection();
    float b = projection[5];
    float c = projection[10];
    float d = projection[14];
    float aspect = (float) width / height;
    float k = (c - 1.f) / (c + 1.f);
    float near = (d * (1.f - k)) / (2.f * k);
    float far = k * near;
    float fov = 2.f * atan(1.f / b);
    float newProjection[16];
    mat4_perspective(newProjection, near, far, fov, aspect);
    lovrGraphicsSetProjection(newProjection);
  }
}

void lovrCanvasResolveMSAA(Canvas* canvas) {
  if (canvas->msaa == 0) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return;
  }

  int width = canvas->texture.width;
  int height = canvas->texture.height;
  glBindFramebuffer(GL_READ_FRAMEBUFFER, canvas->framebuffer);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, canvas->resolveFramebuffer);
  glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_LINEAR);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

TextureFormat lovrCanvasGetFormat(Canvas* canvas) {
  return canvas->texture.slices[0]->format;
}

int lovrCanvasGetMSAA(Canvas* canvas) {
  return canvas->msaa;
}
