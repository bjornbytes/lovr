#include "lib/stb/stb_truetype.h"
#include "core/util.h"
#include <stdint.h>
#include <stdbool.h>

#pragma once

#define GLYPH_PADDING 1

struct Blob;
struct TextureData;

typedef struct Rasterizer {
  ref_t ref;
  stbtt_fontinfo font;
  struct Blob* blob;
  float size;
  float scale;
  int glyphCount;
  int height;
  int advance;
  int ascent;
  int descent;
} Rasterizer;

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

Rasterizer* lovrRasterizerCreate(struct Blob* blob, float size);
void lovrRasterizerDestroy(void* ref);
bool lovrRasterizerHasGlyph(Rasterizer* fontData, uint32_t character);
bool lovrRasterizerHasGlyphs(Rasterizer* fontData, const char* str);
void lovrRasterizerLoadGlyph(Rasterizer* fontData, uint32_t character, Glyph* glyph);
int32_t lovrRasterizerGetKerning(Rasterizer* fontData, uint32_t left, uint32_t right);
