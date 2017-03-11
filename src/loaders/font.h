#include "lib/map/map.h"
#include <stdint.h>

#pragma once

typedef struct {
  void* rasterizer;
  int size;
  int height;
} FontData;

typedef struct {
  int x;
  int y;
  int w;
  int h;
  int dx;
  int dy;
  int advance;
  uint8_t* data;
} Glyph;

typedef map_t(Glyph) map_glyph_t;

FontData* lovrFontDataCreate(void* data, int size, int height);
void lovrFontDataDestroy(FontData* fontData);
void lovrFontDataLoadGlyph(FontData* fontData, uint32_t character, Glyph* glyph);
int lovrFontDataGetKerning(FontData* fontData, uint32_t left, uint32_t right);
