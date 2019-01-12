#include "data/blob.h"
#include "data/textureData.h"
#include "lib/map/map.h"
#include "lib/stb/stb_truetype.h"
#include "util.h"
#include <stdint.h>
#include <stdbool.h>

#pragma once

#define GLYPH_PADDING 1

typedef struct {
  Ref ref;
  stbtt_fontinfo font;
  Blob* blob;
  float size;
  float scale;
  int glyphCount;
  int height;
  int advance;
  int ascent;
  int descent;
} Rasterizer;

typedef struct {
  int x;
  int y;
  int w;
  int h;
  int tw;
  int th;
  int dx;
  int dy;
  int advance;
  TextureData* data;
} Glyph;

typedef map_t(Glyph) map_glyph_t;

Rasterizer* lovrRasterizerInit(Rasterizer* rasterizer, Blob* blob, float size);
#define lovrRasterizerCreate(...) lovrRasterizerInit(lovrAlloc(Rasterizer), __VA_ARGS__)
void lovrRasterizerDestroy(void* ref);
bool lovrRasterizerHasGlyph(Rasterizer* fontData, uint32_t character);
bool lovrRasterizerHasGlyphs(Rasterizer* fontData, const char* str);
void lovrRasterizerLoadGlyph(Rasterizer* fontData, uint32_t character, Glyph* glyph);
int lovrRasterizerGetKerning(Rasterizer* fontData, uint32_t left, uint32_t right);
