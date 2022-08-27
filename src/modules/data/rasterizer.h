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
bool lovrRasterizerIsGlyphEmpty(Rasterizer* rasterizer, uint32_t codepoint);
float lovrRasterizerGetAscent(Rasterizer* rasterizer);
float lovrRasterizerGetDescent(Rasterizer* rasterizer);
float lovrRasterizerGetLeading(Rasterizer* rasterizer);
float lovrRasterizerGetAdvance(Rasterizer* rasterizer, uint32_t codepoint);
float lovrRasterizerGetBearing(Rasterizer* rasterizer, uint32_t codepoint);
float lovrRasterizerGetKerning(Rasterizer* rasterizer, uint32_t first, uint32_t second);
void lovrRasterizerGetBoundingBox(Rasterizer* rasterizer, float box[4]);
void lovrRasterizerGetGlyphBoundingBox(Rasterizer* rasterizer, uint32_t codepoint, float box[4]);
bool lovrRasterizerGetCurves(Rasterizer* rasterizer, uint32_t codepoint, void (*fn)(void* context, uint32_t degree, float* points), void* context);
bool lovrRasterizerGetPixels(Rasterizer* rasterizer, uint32_t codepoint, float* pixels, uint32_t width, uint32_t height, double spread);
