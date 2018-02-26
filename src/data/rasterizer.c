#include "data/rasterizer.h"
#include "resources/Cabin.ttf.h"
#include "util.h"
#include "msdfgen-c.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_OUTLINE_H

static FT_Library ft = NULL;

typedef struct {
  float x;
  float y;
  msShape* shape;
  msContour* contour;
} ftContext;

static int ftMoveTo(const FT_Vector* to, void* userdata) {
  ftContext* context = userdata;
  context->contour = msShapeAddContour(context->shape);
  context->x = to->x / 64.;
  context->y = to->y / 64.;
  return 0;
}

static int ftLineTo(const FT_Vector* to, void* userdata) {
  ftContext* context = userdata;
  float x = to->x / 64.;
  float y = to->y / 64.;
  msContourAddLinearEdge(context->contour, context->x, context->y, x, y);
  context->x = x;
  context->y = y;
  return 0;
}

static int ftConicTo(const FT_Vector* control, const FT_Vector* to, void* userdata) {
  ftContext* context = userdata;
  float cx = control->x / 64.;
  float cy = control->y / 64.;
  float x = to->x / 64.;
  float y = to->y / 64.;
  msContourAddQuadraticEdge(context->contour, context->x, context->y, cx, cy, x, y);
  context->x = x;
  context->y = y;
  return 0;
}

static int ftCubicTo(const FT_Vector* control1, const FT_Vector* control2, const FT_Vector* to, void* userdata) {
  ftContext* context = userdata;
  float c1x = control1->x / 64.;
  float c1y = control1->y / 64.;
  float c2x = control2->x / 64.;
  float c2y = control2->y / 64.;
  float x = to->x / 64.;
  float y = to->y / 64.;
  msContourAddCubicEdge(context->contour, context->x, context->y, c1x, c1y, c2x, c2y, x, y);
  context->x = x;
  context->y = y;
  return 0;
}

Rasterizer* lovrRasterizerCreate(Blob* blob, int size) {
  if (!ft && FT_Init_FreeType(&ft)) {
    lovrThrow("Error initializing FreeType");
  }

  FT_Face face = NULL;
  FT_Error err = FT_Err_Ok;
  if (blob) {
    err = err || FT_New_Memory_Face(ft, blob->data, blob->size, 0, &face);
    lovrRetain(blob);
  } else {
    err = err || FT_New_Memory_Face(ft, Cabin_ttf, Cabin_ttf_len, 0, &face);
  }

  err = err || FT_Set_Pixel_Sizes(face, 0, size);
  lovrAssert(!err, "Problem loading font");

  Rasterizer* rasterizer = lovrAlloc(sizeof(Rasterizer), lovrRasterizerDestroy);
  rasterizer->ftHandle = face;
  rasterizer->blob = blob;
  rasterizer->size = size;
  rasterizer->glyphCount = face->num_glyphs;

  FT_Size_Metrics metrics = face->size->metrics;
  rasterizer->height = metrics.height >> 6;
  rasterizer->advance = metrics.max_advance >> 6;
  rasterizer->ascent = metrics.ascender >> 6;
  rasterizer->descent = metrics.descender >> 6;

  return rasterizer;
}

void lovrRasterizerDestroy(void* ref) {
  Rasterizer* rasterizer = ref;
  FT_Done_Face(rasterizer->ftHandle);
  lovrRelease(rasterizer->blob);
  free(rasterizer);
}

bool lovrRasterizerHasGlyph(Rasterizer* rasterizer, uint32_t character) {
  FT_Face face = rasterizer->ftHandle;
  return FT_Get_Char_Index(face, character) != 0;
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
  FT_Face face = rasterizer->ftHandle;
  FT_Error err = FT_Err_Ok;
  FT_Glyph_Metrics* metrics;
  FT_Outline_Funcs callbacks = {
    .move_to = ftMoveTo,
    .line_to = ftLineTo,
    .conic_to = ftConicTo,
    .cubic_to = ftCubicTo
  };

  msShape* shape = msShapeCreate();
  ftContext context = { .x = 0, .y = 0, .shape = shape, .contour = NULL };

  err = err || FT_Load_Glyph(face, FT_Get_Char_Index(face, character), FT_LOAD_DEFAULT);
  err = err || FT_Outline_Decompose(&face->glyph->outline, &callbacks, &context);
  lovrAssert(!err, "Error loading glyph");

  metrics = &face->glyph->metrics;

  // Initialize glyph
  glyph->x = 0;
  glyph->y = 0;
  glyph->w = metrics->width >> 6;
  glyph->h = metrics->height >> 6;
  glyph->tw = glyph->w + 2 * GLYPH_PADDING;
  glyph->th = glyph->h + 2 * GLYPH_PADDING;
  glyph->dx = metrics->horiBearingX >> 6;
  glyph->dy = metrics->horiBearingY >> 6;
  glyph->advance = metrics->horiAdvance >> 6;
  glyph->data = malloc(glyph->tw * glyph->th * 3 * sizeof(uint8_t));

  // Render SDF
  float tx = GLYPH_PADDING + -glyph->dx;
  float ty = GLYPH_PADDING + glyph->h - glyph->dy;
  msShapeNormalize(shape);
  msEdgeColoringSimple(shape, 3.0, 0);
  msGenerateMSDF(glyph->data, glyph->tw, glyph->th, shape, 4., 1, 1, tx, ty);
  msShapeDestroy(shape);
}

int lovrRasterizerGetKerning(Rasterizer* rasterizer, uint32_t left, uint32_t right) {
  FT_Face face = rasterizer->ftHandle;
  FT_Vector kerning;
  left = FT_Get_Char_Index(face, left);
  right = FT_Get_Char_Index(face, right);
  FT_Get_Kerning(face, left, right, FT_KERNING_DEFAULT, &kerning);
  return kerning.x >> 6;
}
