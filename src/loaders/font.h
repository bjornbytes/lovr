#include "graphics/font.h"
#include <stdint.h>

FontData* lovrFontDataCreate(void* data, int size, int height);
void lovrFontDataDestroy(FontData* fontData);
void lovrFontDataLoadGlyph(FontData* fontData, uint32_t character, Glyph* glyph);
int lovrFontDataGetKerning(FontData* fontData, uint32_t left, uint32_t right);
