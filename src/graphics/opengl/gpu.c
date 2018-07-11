#include "graphics/gpu.h"
#include "graphics/opengl/opengl.h"
#include "lib/glfw.h"
#include <string.h>

#define MAX_TEXTURES 16

static struct {
  Texture* defaultTexture;
  uint32_t framebuffer;
  uint32_t indexBuffer;
  uint32_t program;
  Texture* textures[MAX_TEXTURES];
  uint32_t vertexArray;
  uint32_t vertexBuffer;
  uint32_t viewport[4];
  GpuStats stats;
} state;

void gpuInit(bool srgb, gpuProc (*getProcAddress)(const char*)) {
#ifndef EMSCRIPTEN
  gladLoadGLLoader((GLADloadproc) getProcAddress);
  glEnable(GL_LINE_SMOOTH);
  glEnable(GL_PROGRAM_POINT_SIZE);
  if (srgb) {
    glEnable(GL_FRAMEBUFFER_SRGB);
  } else {
    glDisable(GL_FRAMEBUFFER_SRGB);
  }
#endif
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
}

void gpuDestroy() {
  lovrRelease(state.defaultTexture);
  for (int i = 0; i < MAX_TEXTURES; i++) {
    lovrRelease(state.textures[i]);
  }
}

void gpuPresent() {
  memset(&state.stats, 0, sizeof(state.stats));
}

void gpuBindFramebuffer(uint32_t framebuffer) {
  if (state.framebuffer != framebuffer) {
    state.framebuffer = framebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  }
}

void gpuBindIndexBuffer(uint32_t indexBuffer) {
  if (state.indexBuffer != indexBuffer) {
    state.indexBuffer = indexBuffer;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
  }
}

void gpuBindTexture(Texture* texture, int slot) {
  lovrAssert(slot >= 0 && slot < MAX_TEXTURES, "Invalid texture slot %d", slot);

  if (!texture) {
    if (!state.defaultTexture) {
      TextureData* textureData = lovrTextureDataGetBlank(1, 1, 0xff, FORMAT_RGBA);
      state.defaultTexture = lovrTextureCreate(TEXTURE_2D, &textureData, 1, true, false);
      lovrRelease(textureData);
    }

    texture = state.defaultTexture;
  }

  if (texture != state.textures[slot]) {
    lovrRetain(texture);
    lovrRelease(state.textures[slot]);
    state.textures[slot] = texture;
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(texture->glType, lovrTextureGetId(texture));
  }
}

void gpuBindVertexArray(uint32_t vertexArray) {
  if (state.vertexArray != vertexArray) {
    state.vertexArray = vertexArray;
    glBindVertexArray(vertexArray);
  }
}

void gpuBindVertexBuffer(uint32_t vertexBuffer) {
  if (state.vertexBuffer != vertexBuffer) {
    state.vertexBuffer = vertexBuffer;
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
  }
}

Texture* gpuGetTexture(int slot) {
  lovrAssert(slot >= 0 && slot < MAX_TEXTURES, "Invalid texture slot %d", slot);
  return state.textures[slot];
}

void gpuSetViewport(uint32_t viewport[4]) {
  if (memcmp(state.viewport, viewport, 4 * sizeof(uint32_t))) {
    memcpy(state.viewport, viewport, 4 * sizeof(uint32_t));
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
  }
}

void gpuUseProgram(uint32_t program) {
  if (state.program != program) {
    state.program = program;
    glUseProgram(program);
    state.stats.shaderSwitches++;
  }
}
