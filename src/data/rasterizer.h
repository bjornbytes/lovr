#include "data/blob.h"
#include "lib/map/map.h"
#include "util.h"
#include <stdint.h>
#include <stdbool.h>

#pragma once

#define GLYPH_PADDING 1

typedef struct {
  Ref ref;
  void* ftHandle;
  Blob* blob;
  int size;
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
  uint8_t* data;
} Glyph;

typedef map_t(Glyph) map_glyph_t;

Rasterizer* lovrRasterizerCreate(Blob* blob, int size);
void lovrRasterizerDestroy(void* ref);
bool lovrRasterizerHasGlyph(Rasterizer* fontData, uint32_t character);
bool lovrRasterizerHasGlyphs(Rasterizer* fontData, const char* str);
void lovrRasterizerLoadGlyph(Rasterizer* fontData, uint32_t character, Glyph* glyph);
int lovrRasterizerGetKerning(Rasterizer* fontData, uint32_t left, uint32_t right);
