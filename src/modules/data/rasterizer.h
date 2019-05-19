#include "types.h"
#include "lib/stb/stb_truetype.h"
#include <stdint.h>
#include <stdbool.h>

#pragma once

#define GLYPH_PADDING 1

struct Blob;
struct TextureData;

typedef struct Rasterizer {
  Ref ref;
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
  int x;
  int y;
  int w;
  int h;
  int tw;
  int th;
  int dx;
  int dy;
  int advance;
  struct TextureData* data;
} Glyph;

Rasterizer* lovrRasterizerInit(Rasterizer* rasterizer, struct Blob* blob, float size);
#define lovrRasterizerCreate(...) lovrRasterizerInit(lovrAlloc(Rasterizer), __VA_ARGS__)
void lovrRasterizerDestroy(void* ref);
bool lovrRasterizerHasGlyph(Rasterizer* fontData, uint32_t character);
bool lovrRasterizerHasGlyphs(Rasterizer* fontData, const char* str);
void lovrRasterizerLoadGlyph(Rasterizer* fontData, uint32_t character, Glyph* glyph);
int lovrRasterizerGetKerning(Rasterizer* fontData, uint32_t left, uint32_t right);
