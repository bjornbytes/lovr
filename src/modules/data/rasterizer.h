#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#pragma once

struct Blob;
struct Image;

typedef struct Rasterizer Rasterizer;
Rasterizer* lovrRasterizerCreate(struct Blob* blob, float size);
void lovrRasterizerDestroy(void* ref);
float lovrRasterizerGetFontSize(Rasterizer* rasterizer);
uint32_t lovrRasterizerGetGlyphCount(Rasterizer* rasterizer);
bool lovrRasterizerHasGlyph(Rasterizer* rasterizer, uint32_t codepoint);
bool lovrRasterizerHasGlyphs(Rasterizer* rasterizer, const char* str, size_t length);
float lovrRasterizerGetAscent(Rasterizer* rasterizer);
float lovrRasterizerGetDescent(Rasterizer* rasterizer);
float lovrRasterizerGetLineGap(Rasterizer* rasterizer);
float lovrRasterizerGetLeading(Rasterizer* rasterizer);
float lovrRasterizerGetKerning(Rasterizer* rasterizer, uint32_t prev, uint32_t next);
float lovrRasterizerGetGlyphAdvance(Rasterizer* rasterizer, uint32_t codepoint);
float lovrRasterizerGetGlyphBearing(Rasterizer* rasterizer, uint32_t codepoint);
void lovrRasterizerGetGlyphBoundingBox(Rasterizer* rasterizer, uint32_t codepoint, float box[4]);
bool lovrRasterizerIsGlyphEmpty(Rasterizer* rasterizer, uint32_t codepoint);
void lovrRasterizerGetGlyphCurves(Rasterizer* rasterizer, uint32_t codepoint, void (*fn)(void* context, uint32_t degree, float* points), void* context);
