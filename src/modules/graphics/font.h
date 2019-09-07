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
Font* lovrFontCreate(struct Rasterizer* rasterizer);
void lovrFontDestroy(void* ref);
struct Rasterizer* lovrFontGetRasterizer(Font* font);
struct Texture* lovrFontGetTexture(Font* font);
void lovrFontRender(Font* font, const char* str, size_t length, float wrap, HorizontalAlign halign, float* vertices, uint16_t* indices, uint16_t baseVertex);
void lovrFontMeasure(Font* font, const char* string, size_t length, float wrap, float* width, float* height, uint32_t* lineCount, uint32_t* glyphCount);
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
