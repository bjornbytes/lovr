#include "data/rasterizer.h"
#include "data/blob.h"
#include "data/image.h"
#include "util.h"
#include "VarelaRound.ttf.h"
#include "lib/stb/stb_truetype.h"
#include <msdfgen-c.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct {
  uint32_t codepoint;
  uint16_t x;
  uint16_t y;
  uint16_t w;
  uint16_t h;
  int16_t ox;
  int16_t oy;
  int16_t advance;
} Glyph;

struct Rasterizer {
  uint32_t ref;
  RasterizerType type;
  float size;
  float scale;
  float ascent;
  float descent;
  float leading;
  float spaceAdvance;
  map_t kerning;
  Blob* blob;
  Image* atlas;
  stbtt_fontinfo font;
  arr_t(Glyph) glyphs;
  map_t glyphLookup;
};

static Glyph* lovrRasterizerGetGlyph(Rasterizer* rasterizer, uint32_t codepoint) {
  uint64_t index = map_get(&rasterizer->glyphLookup, hash64(&codepoint, 4));
  return index == MAP_NIL ? NULL : &rasterizer->glyphs.data[index];
}

static bool lovrRasterizerCreateTTF(Rasterizer** result, Blob* blob, float size) {
  const unsigned char* data = blob ? blob->data : etc_VarelaRound_ttf;

  int offset = stbtt_GetFontOffsetForIndex(data, 0);
  if (offset == -1) {
    return true;
  }

  stbtt_fontinfo font;
  if (!stbtt_InitFont(&font, data, offset)) {
    return true;
  }

  Rasterizer* rasterizer = lovrCalloc(sizeof(Rasterizer));
  rasterizer->ref = 1;
  rasterizer->type = RASTERIZER_TTF;

  lovrRetain(blob);
  rasterizer->blob = blob;
  rasterizer->font = font;
  rasterizer->size = size;
  rasterizer->scale = stbtt_ScaleForMappingEmToPixels(&rasterizer->font, size);

  // Even though line gap is a thing, it's usually zero so we pretend it isn't real
  int ascent, descent, lineGap;
  stbtt_GetFontVMetrics(&rasterizer->font, &ascent, &descent, &lineGap);
  rasterizer->ascent = ascent * rasterizer->scale;
  rasterizer->descent = descent * rasterizer->scale;
  rasterizer->leading = (ascent - descent + lineGap) * rasterizer->scale;

  int advance, bearing;
  stbtt_GetCodepointHMetrics(&rasterizer->font, ' ', &advance, &bearing);
  rasterizer->spaceAdvance = advance * rasterizer->scale;

  map_init(&rasterizer->kerning, 0);

  *result = rasterizer;
  return true;
}

static int64_t parseNumber(const char* string, size_t length, map_t* map, const char* key) {
  uint64_t value = map_get(map, hash64(key, strlen(key)));
  if (value == MAP_NIL) return 0;
  const char* s = (const char*) (uintptr_t) value;
  size_t n = length - (s - string);
  bool negative = *s == '-';
  s += negative;
  n -= negative;
  int64_t number = 0;
  while (n > 0 && *s != ' ' && *s >= '0' && *s <= '9') {
    number *= 10;
    number += *s++ - '0';
    n--;
  }
  return negative ? -number : number;
}

static const char* parseString(const char* string, size_t maxLength, map_t* map, const char* key, size_t* length) {
  uint64_t value = map_get(map, hash64(key, strlen(key)));
  if (value == MAP_NIL) return NULL;
  const char* s = (const char*) (uintptr_t) value;
  size_t n = maxLength - (s - string);
  if (*s == '"') {
    s++, n--;
    char* quote = memchr(s, '"', n);
    if (!quote) return NULL;
    *length = quote - s;
    return s;
  } else {
    char* space = memchr(s, ' ', n);
    *length = space ? space - s : n;
    return s;
  }
}

static int16_t readi16(const uint8_t* p) { int16_t x; memcpy(&x, p, sizeof(x)); return x; }
static uint16_t readu16(const uint8_t* p) { uint16_t x; memcpy(&x, p, sizeof(x)); return x; }
static uint32_t readu32(const uint8_t* p) { uint32_t x; memcpy(&x, p, sizeof(x)); return x; }

static bool lovrRasterizerCreateBMF(Rasterizer** result, Blob* blob, RasterizerIO* io) {
  if (!blob || blob->size < 4) return true;

  uint8_t magic[] = { 'B', 'M', 'F' };
  bool text = !memcmp(blob->data, "info", 4);
  bool binary = !memcmp(blob->data, magic, sizeof(magic));

  if (!text && !binary) {
    return true;
  }

  char fullpath[1024];
  size_t nameLength = strlen(blob->name);
  lovrCheck(nameLength < sizeof(fullpath), "BMFont Blob filename is too long");
  memcpy(fullpath, blob->name, nameLength + 1);
  char* slash = strrchr(fullpath, '/');
  char* filename = slash ? slash + 1 : fullpath;
  size_t maxLength = sizeof(fullpath) - 1 - (filename - fullpath);

  Rasterizer* rasterizer = lovrCalloc(sizeof(Rasterizer));
  rasterizer->ref = 1;
  rasterizer->type = RASTERIZER_BMF;
  rasterizer->scale = 1.f;
  map_init(&rasterizer->kerning, 0);
  arr_init(&rasterizer->glyphs);
  arr_reserve(&rasterizer->glyphs, 36);
  map_init(&rasterizer->glyphLookup, 36);

  if (text) {
    map_t map;
    map_init(&map, 8);

    char* string = blob->data;
    size_t length = blob->size;

    while (length > 0) {
      char* newline = memchr(string, '\n', length);
      size_t lineLength = newline ? newline - string : length;

      // Clear the map
      memset(map.hashes, 0xff, map.size * sizeof(uint64_t));
      memset(map.values, 0xff, map.size * sizeof(uint64_t));
      map.used = 0;

      // Parse the tag
      char* tag = string;
      char* space = memchr(string, ' ', lineLength);
      size_t tagLength = space ? space - tag : lineLength;
      size_t cursor = space ? tagLength + 1 : lineLength;

      // Add fields to the map
      while (cursor < lineLength) {
        char* equals = memchr(string + cursor, '=', lineLength - cursor);

        // Split fields on =, the part before is the key and the part after is the value, add to map
        if (equals) {
          char* key = string + cursor;
          uint64_t hash = hash64(key, equals - key);
          map_set(&map, hash, (uintptr_t) (equals + 1));
        }

        // Fields are separated by spaces
        space = memchr(string + cursor, ' ', lineLength - cursor);
        cursor = space ? space - string + 1 : lineLength;
      }

      if (!memcmp(tag, "info", tagLength)) {
        rasterizer->size = parseNumber(string, lineLength, &map, "size");
      } else if (!memcmp(tag, "common", tagLength)) {
        rasterizer->leading = parseNumber(string, lineLength, &map, "lineHeight");
        rasterizer->ascent = parseNumber(string, lineLength, &map, "base");
        rasterizer->descent = rasterizer->leading - rasterizer->ascent; // Best effort

        if (parseNumber(string, lineLength, &map, "pages") != 1) {
          lovrSetError("Currently, BMFont files with multiple images are not supported");
          map_free(&map);
          goto fail;
        }

        if (parseNumber(string, lineLength, &map, "packed") != 0) {
          lovrSetError("Currently, packed BMFont files are not supported");
          map_free(&map);
          goto fail;
        }
      } else if (!memcmp(tag, "page", tagLength)) {
        size_t fileLength;
        const char* file = parseString(string, lineLength, &map, "file", &fileLength);
        lovrCheck(file, "BMFont is missing image path");
        lovrCheck(fileLength <= maxLength, "BMFont filename is too long");
        memcpy(filename, file, fileLength);
        filename[fileLength] = '\0';
      } else if (!memcmp(tag, "char", tagLength)) {
        Glyph glyph;
        glyph.codepoint = parseNumber(string, lineLength, &map, "id");
        glyph.x = parseNumber(string, lineLength, &map, "x");
        glyph.y = parseNumber(string, lineLength, &map, "y");
        glyph.w = parseNumber(string, lineLength, &map, "width");
        glyph.h = parseNumber(string, lineLength, &map, "height");
        glyph.ox = parseNumber(string, lineLength, &map, "xoffset");
        glyph.oy = parseNumber(string, lineLength, &map, "yoffset");
        glyph.advance = parseNumber(string, lineLength, &map, "xadvance");
        if (glyph.codepoint == ' ') rasterizer->spaceAdvance = glyph.advance;
        arr_push(&rasterizer->glyphs, glyph);
        map_set(&rasterizer->glyphLookup, hash64(&glyph.codepoint, 4), rasterizer->glyphs.length - 1);
      } else if (!memcmp(tag, "kerning", tagLength)) {
        uint32_t first = parseNumber(string, lineLength, &map, "first");
        uint32_t second = parseNumber(string, lineLength, &map, "second");
        int64_t kerning = parseNumber(string, lineLength, &map, "amount");
        uint32_t hash = hash64((uint32_t[]) { first, second }, 2 * sizeof(uint32_t));
        map_set(&rasterizer->kerning, hash, (uint64_t) kerning);
      }

      // Go to the next line
      if (newline) {
        string += lineLength + 1;
        length -= lineLength + 1;
      } else {
        break;
      }
    }
  } else if (binary) {
    uint8_t* data = blob->data;
    size_t size = blob->size;

    lovrCheck(data[3] == 3, "Currently, only BMFont version 3 is supported");
    data += 4;
    size -= 4;

    while (size > 0) {
      uint8_t blockType = data[0];
      uint32_t blockSize = readu32(data + 1);
      data += 5;
      size -= 5;

      switch (blockType) {
        case 1: // info
          rasterizer->size = (float) readu16(data + 0);
          break;
        case 2: // common
          lovrCheck(readu16(data + 8) == 1, "Currently, BMFont files with multiple images are not supported");
          lovrCheck(data[10] == 0, "Currently, packed BMFont files are not supported");
          rasterizer->leading = (float) readu16(data + 0);
          rasterizer->ascent = (float) readu16(data + 2);
          rasterizer->descent = rasterizer->leading - rasterizer->ascent;
          break;
        case 3: { // pages
          size_t fileLength = strnlen((const char*) data, size);
          lovrCheck(fileLength <= maxLength, "BMFont filename is too long");
          memcpy(filename, data, fileLength);
          filename[fileLength] = '\0';
          break;
        }
        case 4: { // chars
          uint32_t count = blockSize / 20;
          arr_reserve(&rasterizer->glyphs, count);
          for (uint32_t i = 0; i < count; i++) {
            uint32_t codepoint = readu32(data + 20 * i + 0);
            Glyph glyph;
            glyph.x = readu16(data + 20 * i + 4);
            glyph.y = readu16(data + 20 * i + 6);
            glyph.w = readu16(data + 20 * i + 8);
            glyph.h = readu16(data + 20 * i + 10);
            glyph.ox = readi16(data + 20 * i + 12);
            glyph.oy = readi16(data + 20 * i + 14);
            glyph.advance = readi16(data + 20 * i + 16);
            arr_push(&rasterizer->glyphs, glyph);
            map_set(&rasterizer->glyphLookup, hash64(&codepoint, 4), rasterizer->glyphs.length - 1);
          }
          break;
        }
        case 5: { // kerning
          uint32_t count = blockSize / 10;
          for (uint32_t i = 0; i < count; i++) {
            uint32_t first = readu32(data + 10 * i + 0);
            uint32_t second = readu32(data + 10 * i + 4);
            int16_t kerning = readi16(data + 10 * i + 8);
            map_set(&rasterizer->kerning, hash64((uint32_t[]) { first, second }, 8), kerning);
          }
          break;
        }
      }

      data += blockSize;
      size -= MIN(blockSize, size);
    }
  }

  size_t atlasSize;
  void* atlasData = io(fullpath, &atlasSize);
  lovrAssertGoto(fail, atlasData, "Failed to read BMFont image from %s", fullpath);

  Blob* atlasBlob = lovrBlobCreate(atlasData, atlasSize, "BMFont atlas");
  rasterizer->atlas = lovrImageCreateFromFile(atlasBlob);
  lovrRelease(atlasBlob, lovrBlobDestroy);
  lovrAssertGoto(fail, rasterizer->atlas, "Failed to load BMfont atlas image: %s", lovrGetError());

  *result = rasterizer;
  return true;
fail:
  map_free(&rasterizer->kerning);
  arr_free(&rasterizer->glyphs);
  map_free(&rasterizer->glyphLookup);
  return false;
}

Rasterizer* lovrRasterizerCreate(Blob* blob, float size, RasterizerIO* io) {
  Rasterizer* rasterizer = NULL;
  if (!rasterizer && !lovrRasterizerCreateTTF(&rasterizer, blob, size)) return NULL;
  if (!rasterizer && !lovrRasterizerCreateBMF(&rasterizer, blob, io)) return NULL;
  if (!rasterizer) lovrSetError("Problem loading font: not recognized as TTF or BMFont");
  return rasterizer;
}

void lovrRasterizerDestroy(void* ref) {
  Rasterizer* rasterizer = ref;
  lovrRelease(rasterizer->blob, lovrBlobDestroy);
  lovrRelease(rasterizer->atlas, lovrImageDestroy);
  map_free(&rasterizer->kerning);
  if (rasterizer->type == RASTERIZER_BMF) {
    arr_free(&rasterizer->glyphs);
    map_free(&rasterizer->glyphLookup);
  }
  lovrFree(rasterizer);
}

RasterizerType lovrRasterizerGetType(Rasterizer* rasterizer) {
  return rasterizer->type;
}

float lovrRasterizerGetFontSize(Rasterizer* rasterizer) {
  return rasterizer->size;
}

size_t lovrRasterizerGetGlyphCount(Rasterizer* rasterizer) {
  if (rasterizer->type == RASTERIZER_TTF) {
    return rasterizer->font.numGlyphs;
  } else {
    return rasterizer->glyphs.length;
  }
}

bool lovrRasterizerHasGlyph(Rasterizer* rasterizer, uint32_t codepoint) {
  if (rasterizer->type == RASTERIZER_TTF) {
    return stbtt_FindGlyphIndex(&rasterizer->font, codepoint) != 0;
  } else {
    return map_get(&rasterizer->glyphLookup, hash64(&codepoint, 4)) != MAP_NIL;
  }
}

bool lovrRasterizerHasGlyphs(Rasterizer* rasterizer, const char* str, size_t length) {
  size_t bytes;
  uint32_t codepoint;
  const char* end = str + length;
  while ((bytes = utf8_decode(str, end, &codepoint)) > 0) {
    if (!lovrRasterizerHasGlyph(rasterizer, codepoint)) {
      return false;
    }
    str += bytes;
  }
  return true;
}

bool lovrRasterizerIsGlyphEmpty(Rasterizer* rasterizer, uint32_t codepoint) {
  if (rasterizer->type == RASTERIZER_TTF) {
    return stbtt_IsGlyphEmpty(&rasterizer->font, stbtt_FindGlyphIndex(&rasterizer->font, codepoint));
  } else {
    Glyph* glyph = lovrRasterizerGetGlyph(rasterizer, codepoint);
    return !glyph || glyph->w == 0 || glyph->h == 0;
  }
}

float lovrRasterizerGetAscent(Rasterizer* rasterizer) {
  return rasterizer->ascent;
}

float lovrRasterizerGetDescent(Rasterizer* rasterizer) {
  return rasterizer->descent;
}

float lovrRasterizerGetLeading(Rasterizer* rasterizer) {
  return rasterizer->leading;
}

float lovrRasterizerGetAdvance(Rasterizer* rasterizer, uint32_t codepoint) {
  if (codepoint == ' ') {
    return rasterizer->spaceAdvance;
  }

  if (rasterizer->type == RASTERIZER_TTF) {
    int advance, bearing;
    stbtt_GetCodepointHMetrics(&rasterizer->font, codepoint, &advance, &bearing);
    return advance * rasterizer->scale;
  } else {
    Glyph* glyph = lovrRasterizerGetGlyph(rasterizer, codepoint);
    return glyph ? (float) glyph->advance : 0.f;
  }
}

float lovrRasterizerGetBearing(Rasterizer* rasterizer, uint32_t codepoint) {
  if (rasterizer->type == RASTERIZER_TTF) {
    int advance, bearing;
    stbtt_GetCodepointHMetrics(&rasterizer->font, codepoint, &advance, &bearing);
    return bearing * rasterizer->scale;
  } else {
    Glyph* glyph = lovrRasterizerGetGlyph(rasterizer, codepoint);
    return glyph ? (float) glyph->ox : 0.f;
  }
}

float lovrRasterizerGetKerning(Rasterizer* rasterizer, uint32_t first, uint32_t second) {
  uint32_t codepoints[] = { first, second };
  uint64_t hash = hash64(codepoints, sizeof(codepoints));
  uint64_t kerning = map_get(&rasterizer->kerning, hash);

  if (kerning == MAP_NIL) {
    if (rasterizer->type == RASTERIZER_TTF) {
      kerning = stbtt_GetCodepointKernAdvance(&rasterizer->font, first, second);
      map_set(&rasterizer->kerning, hash, kerning);
    } else {
      return 0.f;
    }
  }

  return (int32_t) kerning * rasterizer->scale;
}

void lovrRasterizerGetBoundingBox(Rasterizer* rasterizer, float box[4]) {
  if (rasterizer->type == RASTERIZER_TTF) {
    int x0, y0, x1, y1;
    stbtt_GetFontBoundingBox(&rasterizer->font, &x0, &y0, &x1, &y1);
    box[0] = x0 * rasterizer->scale;
    box[1] = y0 * rasterizer->scale;
    box[2] = x1 * rasterizer->scale;
    box[3] = y1 * rasterizer->scale;
  } else {
    box[0] = box[1] = HUGE_VALF;
    box[2] = box[3] = -HUGE_VALF;
    for (size_t i = 0; i < rasterizer->glyphs.length; i++) {
      box[0] = MIN(box[0], rasterizer->glyphs.data[i].ox);
      box[1] = MIN(box[1], rasterizer->glyphs.data[i].oy);
      box[2] = MAX(box[2], rasterizer->glyphs.data[i].ox + rasterizer->glyphs.data[i].w);
      box[3] = MAX(box[3], rasterizer->glyphs.data[i].oy + rasterizer->glyphs.data[i].h);
    }
  }
}

void lovrRasterizerGetGlyphBoundingBox(Rasterizer* rasterizer, uint32_t codepoint, float box[4]) {
  if (rasterizer->type == RASTERIZER_TTF) {
    int x0, y0, x1, y1;
    stbtt_GetCodepointBox(&rasterizer->font, codepoint, &x0, &y0, &x1, &y1);
    box[0] = x0 * rasterizer->scale;
    box[1] = y0 * rasterizer->scale;
    box[2] = x1 * rasterizer->scale;
    box[3] = y1 * rasterizer->scale;
  } else {
    Glyph* glyph = lovrRasterizerGetGlyph(rasterizer, codepoint);
    if (glyph) {
      box[0] = glyph->ox;
      box[1] = rasterizer->ascent - (glyph->oy + glyph->h);
      box[2] = glyph->ox + glyph->w;
      box[3] = rasterizer->ascent - glyph->oy;
    } else {
      box[0] = 0.f;
      box[1] = 0.f;
      box[2] = 0.f;
      box[3] = 0.f;
    }
  }
}

bool lovrRasterizerGetCurves(Rasterizer* rasterizer, uint32_t codepoint, void (*fn)(void* context, uint32_t degree, float* points), void* context) {
  if (rasterizer->type == RASTERIZER_BMF) {
    return false;
  }

  uint32_t id = stbtt_FindGlyphIndex(&rasterizer->font, codepoint);

  if (stbtt_IsGlyphEmpty(&rasterizer->font, id)) {
    return false;
  }

  stbtt_vertex* vertices;
  uint32_t count = stbtt_GetGlyphShape(&rasterizer->font, id, &vertices);

  float x = 0.f;
  float y = 0.f;
  for (uint32_t i = 0; i < count; i++) {
    stbtt_vertex vertex = vertices[i];
    float x2 = vertex.x * rasterizer->scale;
    float y2 = vertex.y * rasterizer->scale;

    if (vertex.type == STBTT_vline) {
      float points[4] = { x, y, x2, y2 };
      fn(context, 1, points);
    } else if (vertex.type == STBTT_vcurve) {
      float cx = vertex.cx * rasterizer->scale;
      float cy = vertex.cy * rasterizer->scale;
      float points[6] = { x, y, cx, cy, x2, y2 };
      fn(context, 2, points);
    } else if (vertex.type == STBTT_vcubic) {
      float cx1 = vertex.cx * rasterizer->scale;
      float cy1 = vertex.cy * rasterizer->scale;
      float cx2 = vertex.cx1 * rasterizer->scale;
      float cy2 = vertex.cy1 * rasterizer->scale;
      float points[8] = { x, y, cx1, cy1, cx2, cy2, x2, y2 };
      fn(context, 3, points);
    }

    x = x2;
    y = y2;
  }

  stbtt_FreeShape(&rasterizer->font, vertices);
  return true;
}

bool lovrRasterizerGetPixels(Rasterizer* rasterizer, uint32_t codepoint, float* pixels, uint32_t width, uint32_t height, double spread) {
  if (rasterizer->type == RASTERIZER_BMF) {
    Glyph* glyph = lovrRasterizerGetGlyph(rasterizer, codepoint);
    if (!glyph) return false;

    lovrCheck(width == glyph->w, "Invalid glyph width (%d expected, got %d)", glyph->w, width);
    lovrCheck(height == glyph->h, "Invalid glyph height (%d expected, got %d)", glyph->h, height);

    for (uint32_t y = 0; y < glyph->h; y++) {
      for (uint32_t x = 0; x < glyph->w; x++) {
        float pixel[4];
        lovrImageGetPixel(rasterizer->atlas, glyph->x + x, glyph->y + y, pixel);
        pixels[4 * (y * width + x) + 0] = pixel[0];
        pixels[4 * (y * width + x) + 1] = pixel[1];
        pixels[4 * (y * width + x) + 2] = pixel[2];
        pixels[4 * (y * width + x) + 3] = pixel[3];
      }
    }

    return true;
  }

  int id = stbtt_FindGlyphIndex(&rasterizer->font, codepoint);

  if (!id || stbtt_IsGlyphEmpty(&rasterizer->font, id)) {
    return true;
  }

  stbtt_vertex* vertices;
  int count = stbtt_GetGlyphShape(&rasterizer->font, id, &vertices);

  msShape* shape = msShapeCreate();
  msContour* contour = NULL;

  float x = 0.f;
  float y = 0.f;
  float scale = rasterizer->scale;
  for (int i = 0; i < count; i++) {
    stbtt_vertex vertex = vertices[i];
    float x2 = vertex.x * scale;
    float y2 = vertex.y * scale;

    if (vertex.type == STBTT_vmove) {
      contour = msShapeAddContour(shape);
    } else if (vertex.type == STBTT_vline) {
      msContourAddLinearEdge(contour, x, y, x2, y2);
    } else if (vertex.type == STBTT_vcurve) {
      float cx = vertex.cx * scale;
      float cy = vertex.cy * scale;
      msContourAddQuadraticEdge(contour, x, y, cx, cy, x2, y2);
    } else if (vertex.type == STBTT_vcubic) {
      float cx1 = vertex.cx * scale;
      float cy1 = vertex.cy * scale;
      float cx2 = vertex.cx1 * scale;
      float cy2 = vertex.cy1 * scale;
      msContourAddCubicEdge(contour, x, y, cx1, cy1, cx2, cy2, x2, y2);
    }

    x = x2;
    y = y2;
  }

  stbtt_FreeShape(&rasterizer->font, vertices);

  int x0, y0, x1, y1;
  stbtt_GetGlyphBox(&rasterizer->font, id, &x0, &y0, &x1, &y1);

  uint32_t padding = ceil(spread / 2.);
  float offsetX = -x0 * scale + padding;
  float offsetY = -y1 * scale - padding;

  msShapeNormalize(shape);
  msShapeOrientContours(shape);
  msEdgeColoringSimple(shape, 3., 0);
  msGenerateMTSDF(pixels, width, height, shape, spread, 1.f, -1.f, offsetX, offsetY);
  msShapeDestroy(shape);

  return true;
}

Image* lovrRasterizerGetAtlas(Rasterizer* rasterizer) {
  return rasterizer->atlas;
}

uint32_t lovrRasterizerGetAtlasGlyph(Rasterizer* rasterizer, size_t index, uint16_t* x, uint16_t* y) {
  if (rasterizer->type == RASTERIZER_TTF || index >= rasterizer->glyphs.length) {
    return 0;
  }

  Glyph* glyph = &rasterizer->glyphs.data[index];

  *x = glyph->x;
  *y = glyph->y;

  return glyph->codepoint;
}
