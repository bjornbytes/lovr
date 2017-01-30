#include "graphics/font.h"

FontData* lovrFontDataCreate(void* data, int size);
void lovrFontDataGetGlyph(FontData* fontData, char c, Glyph* glyph);
