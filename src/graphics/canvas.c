#include "graphics/canvas.h"
#include "graphics/graphics.h"
#include "math/mat4.h"
#include <math.h>
#include <stdlib.h>

Canvas* lovrCanvasCreate(CanvasType type, int width, int height, int msaa) {
  TextureData* textureData = lovrTextureDataGetEmpty(width, height, FORMAT_RGBA);
  Texture* texture = lovrTextureCreate(TEXTURE_2D, &textureData, 1, true);
  if (!texture) return NULL;

  Canvas* canvas = lovrAlloc(sizeof(Canvas), lovrCanvasDestroy);
  canvas->texture = *texture;
  canvas->type = type;
  canvas->msaa = msaa;
  canvas->framebuffer = 0;
  canvas->resolveFramebuffer = 0;
  canvas->depthBuffer = 0;
  canvas->msaaTexture = 0;

  // Framebuffer
  glGenFramebuffers(1, &canvas->framebuffer);
  glBindFramebuffer(GL_FRAMEBUFFER, canvas->framebuffer);

  // Color attachment
  if (msaa > 0) {
    GLenum format = lovrGraphicsIsGammaCorrect() ? GL_SRGB8_ALPHA8 : GL_RGBA8;
    glGenRenderbuffers(1, &canvas->msaaTexture);
    glBindRenderbuffer(GL_RENDERBUFFER, canvas->msaaTexture);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, msaa, format, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, canvas->msaaTexture);
  } else {
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, canvas->texture.id, 0);
  }

  // Depth attachment
  if (type == CANVAS_3D) {
    glGenRenderbuffers(1, &canvas->depthBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, canvas->depthBuffer);
    if (msaa > 0) {
      glRenderbufferStorageMultisample(GL_RENDERBUFFER, msaa, GL_DEPTH_COMPONENT, width, height);
    } else {
      glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);
    }
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, canvas->depthBuffer);
  }

  // Resolve framebuffer
  if (msaa > 0) {
    glGenFramebuffers(1, &canvas->resolveFramebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, canvas->resolveFramebuffer);
    glBindTexture(GL_TEXTURE_2D, canvas->texture.id);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, canvas->texture.id, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, canvas->framebuffer);
  }

  lovrAssert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Error creating texture");
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
  if (canvas->depthBuffer) {
    glDeleteRenderbuffers(1, &canvas->depthBuffer);
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
    float fov = -2.f * atan(1.f / b);
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

int lovrCanvasGetMSAA(Canvas* canvas) {
  return canvas->msaa;
}
