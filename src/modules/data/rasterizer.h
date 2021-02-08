#include <stdbool.h>
#include <stdint.h>

#pragma once

#define GLYPH_PADDING 1

struct Blob;
struct TextureData;

typedef struct {
  uint32_t x;
  uint32_t y;
  uint32_t w;
  uint32_t h;
  uint32_t tw;
  uint32_t th;
  int32_t dx;
  int32_t dy;
  int32_t advance;
  struct TextureData* data;
} Glyph;

typedef struct Rasterizer Rasterizer;
Rasterizer* lovrRasterizerCreate(struct Blob* blob, float size);
void lovrRasterizerDestroy(void* ref);
float lovrRasterizerGetSize(Rasterizer* rasterizer);
int lovrRasterizerGetGlyphCount(Rasterizer* rasterizer);
int lovrRasterizerGetHeight(Rasterizer* rasterizer);
int lovrRasterizerGetAdvance(Rasterizer* rasterizer);
int lovrRasterizerGetAscent(Rasterizer* rasterizer);
int lovrRasterizerGetDescent(Rasterizer* rasterizer);
bool lovrRasterizerHasGlyph(Rasterizer* fontData, uint32_t character);
bool lovrRasterizerHasGlyphs(Rasterizer* fontData, const char* str);
void lovrRasterizerLoadGlyph(Rasterizer* fontData, uint32_t character, Glyph* glyph);
int32_t lovrRasterizerGetKerning(Rasterizer* fontData, uint32_t left, uint32_t right);
