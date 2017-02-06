#include "graphics/font.h"
#include <stdint.h>

FontData* lovrFontDataCreate(void* data, int size, int height);
void lovrFontDataDestroy(FontData* fontData);
void lovrFontDataLoadGlyph(FontData* fontData, uint32_t characer, Glyph* glyph);
