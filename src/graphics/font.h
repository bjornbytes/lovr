#include "data/rasterizer.h"
#include "util.h"
#include "graphics/texture.h"
#include "math/math.h"
#include "lib/map/map.h"
#include "lib/vec/vec.h"
#include <stdint.h>

#pragma once

typedef enum {
  ALIGN_LEFT,
  ALIGN_RIGHT,
  ALIGN_CENTER
} HorizontalAlign;

typedef enum {
  ALIGN_TOP,
  ALIGN_BOTTOM,
  ALIGN_MIDDLE
} VerticalAlign;

typedef struct {
  int x;
  int y;
  int width;
  int height;
  int rowHeight;
  int padding;
  map_glyph_t glyphs;
} FontAtlas;

typedef struct {
  Ref ref;
  Rasterizer* rasterizer;
  Texture* texture;
  FontAtlas atlas;
  map_int_t kerning;
  float lineHeight;
  float pixelDensity;
} Font;

Font* lovrFontCreate(Rasterizer* rasterizer);
void lovrFontDestroy(void* ref);
void lovrFontRender(Font* font, const char* str, float wrap, HorizontalAlign halign, VerticalAlign valign, vec_float_t* vertices, float* offsety);
float lovrFontGetWidth(Font* font, const char* string, float wrap);
float lovrFontGetHeight(Font* font);
float lovrFontGetAscent(Font* font);
float lovrFontGetDescent(Font* font);
float lovrFontGetBaseline(Font* font);
float lovrFontGetLineHeight(Font* font);
void lovrFontSetLineHeight(Font* font, float lineHeight);
int lovrFontGetKerning(Font* font, unsigned int a, unsigned int b);
float lovrFontGetPixelDensity(Font* font);
void lovrFontSetPixelDensity(Font* font, float pixelDensity);
Glyph* lovrFontGetGlyph(Font* font, uint32_t codepoint);
void lovrFontAddGlyph(Font* font, Glyph* glyph);
void lovrFontExpandTexture(Font* font);
void lovrFontCreateTexture(Font* font);
