#include "util.h"
#include "graphics/texture.h"
#include "vendor/map/map.h"
#include <stdint.h>

#pragma once

typedef struct {
  int x;
  int y;
  int w;
  int h;
  int advance;
  uint8_t* data;
} GlyphData;

typedef struct {
  void* rasterizer;
} FontData;

// GlyphData along with the glyph's position in the texture atlas
typedef struct {
  GlyphData* glyphData;
  float s;
  float t;
} Glyph;

typedef struct {
  Ref ref;
  FontData* fontData;
  Texture* texture;
  int tx;
  int ty;
  int tw;
  int th;
  int rowHeight;
  int padding;
  map_void_t glyphs;
} Font;

Font* lovrFontCreate(FontData* fontData);
void lovrFontDestroy(const Ref* ref);
void lovrFontDataDestroy(FontData* fontData);
void lovrFontPrint(Font* font, const char* str);
Glyph* lovrFontGetGlyph(Font* font, char character);
