#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#pragma once

struct Rasterizer;
struct Texture;

typedef enum {
  ALIGN_LEFT,
  ALIGN_CENTER,
  ALIGN_RIGHT
} HorizontalAlign;

typedef enum {
  ALIGN_TOP,
  ALIGN_MIDDLE,
  ALIGN_BOTTOM
} VerticalAlign;

typedef struct Font Font;
Font* lovrFontCreate(struct Rasterizer* rasterizer, uint32_t padding, double spread);
void lovrFontDestroy(void* ref);
struct Rasterizer* lovrFontGetRasterizer(Font* font);
struct Texture* lovrFontGetTexture(Font* font);
void lovrFontRender(Font* font, const char* str, size_t length, float wrap, HorizontalAlign halign, float* vertices, uint16_t* indices, uint16_t baseVertex);
void lovrFontMeasure(Font* font, const char* string, size_t length, float wrap, float* width, float* height, uint32_t* lineCount, uint32_t* glyphCount);
uint32_t lovrFontGetPadding(Font* font);
double lovrFontGetSpread(Font* font);
float lovrFontGetHeight(Font* font);
float lovrFontGetAscent(Font* font);
float lovrFontGetDescent(Font* font);
float lovrFontGetBaseline(Font* font);
float lovrFontGetLineHeight(Font* font);
void lovrFontSetLineHeight(Font* font, float lineHeight);
bool lovrFontIsFlipEnabled(Font* font);
void lovrFontSetFlipEnabled(Font* font, bool flip);
int32_t lovrFontGetKerning(Font* font, unsigned int a, unsigned int b);
float lovrFontGetPixelDensity(Font* font);
void lovrFontSetPixelDensity(Font* font, float pixelDensity);
bool lovrFontHasGlyphCached(Font* font, uint32_t character);
bool lovrFontHasGlyphsCached(Font* font, const char* str);
void lovrFontSetTextureDataExternal(Font* font, struct Texture* texture);
void lovrFontAddTextureGlyph(Font* font, uint32_t codepoint, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t tw, uint32_t th, int32_t dx, int32_t dy, int32_t advance);
