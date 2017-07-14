#include "filesystem/blob.h"
#include "lib/map/map.h"
#include <stdint.h>

#pragma once

#define GLYPH_PADDING 1

typedef struct {
  void* rasterizer;
  Blob* blob;
  int size;
  int height;
  int ascent;
  int descent;
} FontData;

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

FontData* lovrFontDataCreate(Blob* blob, int size);
void lovrFontDataDestroy(FontData* fontData);
void lovrFontDataLoadGlyph(FontData* fontData, uint32_t character, Glyph* glyph);
int lovrFontDataGetKerning(FontData* fontData, uint32_t left, uint32_t right);
