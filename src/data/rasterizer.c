#include "data/rasterizer.h"
#include "resources/Cabin.ttf.h"
#include "util.h"
#include "lib/stb/stb_truetype.h"
#include "msdfgen-c.h"
#include <math.h>

Rasterizer* lovrRasterizerCreate(Blob* blob, int size) {
  Rasterizer* rasterizer = lovrAlloc(Rasterizer, lovrRasterizerDestroy);
  if (!rasterizer) return NULL;

  stbtt_fontinfo* font = &rasterizer->font;
  unsigned char* data = blob ? blob->data : Cabin_ttf;
  if (!stbtt_InitFont(font, data, stbtt_GetFontOffsetForIndex(data, 0))) {
    lovrThrow("Problem loading font");
  }

  lovrRetain(blob);
  rasterizer->blob = blob;
  rasterizer->size = size;
  rasterizer->scale = stbtt_ScaleForMappingEmToPixels(font, size);
  rasterizer->glyphCount = font->numGlyphs;

  int ascent, descent, linegap;
  stbtt_GetFontVMetrics(font, &ascent, &descent, &linegap);
  rasterizer->ascent = roundf(ascent * rasterizer->scale);
  rasterizer->descent = roundf(descent * rasterizer->scale);
  rasterizer->height = roundf((ascent - descent + linegap) * rasterizer->scale);

  int x0, y0, x1, y1;
  stbtt_GetFontBoundingBox(font, &x0, &y0, &x1, &y1);
  rasterizer->advance = roundf(x1 * rasterizer->scale);

  return rasterizer;
}

void lovrRasterizerDestroy(void* ref) {
  Rasterizer* rasterizer = ref;
  lovrRelease(rasterizer->blob);
  free(rasterizer);
}

bool lovrRasterizerHasGlyph(Rasterizer* rasterizer, uint32_t character) {
  return stbtt_FindGlyphIndex(&rasterizer->font, character) != 0;
}

bool lovrRasterizerHasGlyphs(Rasterizer* rasterizer, const char* str) {
  int len = strlen(str);
  const char* end = str + len;
  unsigned int codepoint;
  size_t bytes;

  bool hasGlyphs = true;
  while ((bytes = utf8_decode(str, end, &codepoint)) > 0) {
    hasGlyphs &= lovrRasterizerHasGlyph(rasterizer, codepoint);
    str += bytes;
  }
  return hasGlyphs;
}

void lovrRasterizerLoadGlyph(Rasterizer* rasterizer, uint32_t character, Glyph* glyph) {
  int glyphIndex = stbtt_FindGlyphIndex(&rasterizer->font, character);
  lovrAssert(glyphIndex, "Error loading glyph");

  // Trace glyph outline
  stbtt_vertex* vertices;
  int vertexCount = stbtt_GetGlyphShape(&rasterizer->font, glyphIndex, &vertices);
  msShape* shape = msShapeCreate();
  msContour* contour = NULL;
  float x = 0;
  float y = 0;

  for (int i = 0; i < vertexCount; i++) {
    stbtt_vertex vertex = vertices[i];
    float x2 = vertex.x * rasterizer->scale;
    float y2 = vertex.y * rasterizer->scale;

    switch (vertex.type) {
      case STBTT_vmove:
        contour = msShapeAddContour(shape);
        break;

      case STBTT_vline:
        msContourAddLinearEdge(contour, x, y, x2, y2);
        break;

      case STBTT_vcurve: {
        float cx = vertex.cx * rasterizer->scale;
        float cy = vertex.cy * rasterizer->scale;
        msContourAddQuadraticEdge(contour, x, y, cx, cy, x2, y2);
        break;
      }

      case STBTT_vcubic: {
        float cx1 = vertex.cx * rasterizer->scale;
        float cy1 = vertex.cy * rasterizer->scale;
        float cx2 = vertex.cx1 * rasterizer->scale;
        float cy2 = vertex.cy1 * rasterizer->scale;
        msContourAddCubicEdge(contour, x, y, cx1, cy1, cx2, cy2, x2, y2);
        break;
      }
    }

    x = x2;
    y = y2;
  }

  int advance, bearing;
  stbtt_GetGlyphHMetrics(&rasterizer->font, glyphIndex, &advance, &bearing);

  int x0, y0, x1, y1;
  stbtt_GetGlyphBox(&rasterizer->font, glyphIndex, &x0, &y0, &x1, &y1);

  bool empty = stbtt_IsGlyphEmpty(&rasterizer->font, glyphIndex);

  // Initialize glyph data
  glyph->x = 0;
  glyph->y = 0;
  glyph->w = empty ? 0 : ceilf((x1 - x0) * rasterizer->scale);
  glyph->h = empty ? 0 : ceilf((y1 - y0) * rasterizer->scale);
  glyph->tw = glyph->w + 2 * GLYPH_PADDING;
  glyph->th = glyph->h + 2 * GLYPH_PADDING;
  glyph->dx = empty ? 0 : roundf(bearing * rasterizer->scale);
  glyph->dy = empty ? 0 : roundf(y1 * rasterizer->scale);
  glyph->advance = roundf(advance * rasterizer->scale);
  glyph->data = lovrTextureDataCreate(glyph->tw, glyph->th, 0, FORMAT_RGB);

  // Render SDF
  float tx = GLYPH_PADDING + -glyph->dx;
  float ty = GLYPH_PADDING + glyph->h - glyph->dy;
  msShapeNormalize(shape);
  msEdgeColoringSimple(shape, 3.0, 0);
  msGenerateMSDF(glyph->data->blob.data, glyph->tw, glyph->th, shape, 4., 1, 1, tx, ty);
  msShapeDestroy(shape);
}

int lovrRasterizerGetKerning(Rasterizer* rasterizer, uint32_t left, uint32_t right) {
  return stbtt_GetCodepointKernAdvance(&rasterizer->font, left, right) * rasterizer->scale;
}
