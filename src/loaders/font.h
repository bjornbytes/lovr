#include "graphics/font.h"
#include <stdint.h>

FontData* lovrFontDataCreate(void* data, int size);
GlyphData* lovrFontDataCreateGlyph(FontData* fontData, uint32_t characer);
