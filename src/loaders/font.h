#include "graphics/font.h"
#include <stdint.h>

FontData* lovrFontDataCreate(void* data, int size);
void lovrFontDataLoadGlyph(FontData* fontData, uint32_t characer, Glyph* glyph);
