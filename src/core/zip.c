#include "zip.h"
#include <string.h>

static uint16_t readu16(const uint8_t* p) { uint16_t x; memcpy(&x, p, sizeof(x)); return x; }
static uint32_t readu32(const uint8_t* p) { uint32_t x; memcpy(&x, p, sizeof(x)); return x; }

bool zip_open(zip_state* zip) {
  const uint8_t* p = zip->data + zip->size - 22;

  if (zip->size < 22 || readu32(p) != 0x06054b50) {
    return false;
  }

  zip->count = readu16(p + 10);
  zip->cursor = readu32(p + 16);
  zip->base = 0;

  if (zip->cursor + 4 > zip->size) {
    return false;
  }

  // See if the central directory starts where the endOfCentralDirectory said it would.
  // If it doesn't, then it might be a self-extracting archive with broken offsets (common).
  // In this case, assume the central directory is directly adjacent to the endOfCentralDirectory,
  // located at (offsetOfEndOfCentralDirectory (aka size - 22) - sizeOfCentralDirectory).
  // If we find a central directory there, then compute a "base" offset that equals the difference
  // between where it is and where it was supposed to be, and apply this offset to everything else.
  if (readu32(zip->data + zip->cursor) != 0x02014b50) {
    size_t offsetOfEndOfCentralDirectory = zip->size - 22;
    size_t sizeOfCentralDirectory = readu32(p + 12);
    size_t centralDirectoryOffset = offsetOfEndOfCentralDirectory - sizeOfCentralDirectory;

    if (sizeOfCentralDirectory > offsetOfEndOfCentralDirectory || centralDirectoryOffset + 4 > zip->size) {
      return false;
    }

    if (readu32(zip->data + centralDirectoryOffset) != 0x02014b50) {
      return false;
    }

    zip->base = centralDirectoryOffset - zip->cursor;
    zip->cursor = centralDirectoryOffset;
  }

  return true;
}

bool zip_next(zip_state* zip, zip_file* file) {
  const uint8_t* p = zip->data + zip->cursor;

  if (zip->cursor + 46 > zip->size || readu32(p) != 0x02014b50) {
    return false;
  }

  file->mtime = readu16(p + 12);
  file->mdate = readu16(p + 14);
  file->csize = readu32(p + 20);
  file->size = readu32(p + 24);
  file->length = readu16(p + 28);
  file->offset = readu32(p + 42) + zip->base;
  file->name = (const char*) (p + 46);
  zip->cursor += 46 + file->length + readu16(p + 30) + readu16(p + 32);
  return zip->cursor < zip->size;
}

void* zip_load(zip_state* zip, size_t offset, bool* compressed) {
  if (zip->size < 30 || offset > zip->size - 30) {
    return NULL;
  }

  uint8_t* p = zip->data + offset;
  if (readu32(p) != 0x04034b50) {
    return NULL;
  }

  uint16_t compression;
  *compressed = compression = readu16(p + 8);
  if (compression != 0 && compression != 8) {
    return false;
  }

  uint32_t skip = readu16(p + 26) + readu16(p + 28);
  return p + 30 + skip;
}
