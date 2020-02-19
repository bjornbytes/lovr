#include "png.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

static uint32_t crc_lookup[256];
static bool crc_ready = false;
static void crc_init(void) {
  if (!crc_ready) {
    crc_ready = true;
    for (uint32_t i = 0; i < 256; i++) {
      uint32_t x = i;
      for (uint32_t b = 0; b < 8; b++) {
        if (x & 1) {
          x = 0xedb88320L ^ (x >> 1);
        } else {
          x >>= 1;
        }
        crc_lookup[i] = x;
      }
    }
  }
}

static uint32_t crc32(uint8_t* data, size_t length) {
  uint32_t c = 0xffffffff;
  for (size_t i = 0; i < length; i++) c = crc_lookup[(c ^ data[i]) & 0xff] ^ (c >> 8);
  return c ^ 0xffffffff;
}

void* png_encode(uint8_t* pixels, uint32_t w, uint32_t h, int32_t stride, size_t* outputSize) {
  uint8_t signature[] = { 137, 80, 78, 71, 13, 10, 26, 10 };
  uint8_t header[13] = {
    w >> 24, w >> 16, w >> 8, w >> 0,
    h >> 24, h >> 16, h >> 8, h >> 0,
    8, 6, 0, 0, 0
  };

  // The png is encoded using one unfiltered IDAT chunk, each row is an uncompressed huffman block
  // The total IDAT chunk data size is
  // - 2 bytes for the zlib header
  // - 6 bytes per block (5 bytes for the huffman block header + 1 byte scanline filter prefix)
  // - n bytes for the image data itself (width * height * 4)
  // - 4 bytes for the adler32 checksum
  size_t rowSize = w * 4;
  size_t imageSize = rowSize * h;
  size_t blockSize = rowSize + 1;
  size_t idatSize = 2 + (h * (5 + 1)) + imageSize + 4;

  *outputSize = sizeof(signature); // Signature
  *outputSize += 4 + strlen("IHDR") + sizeof(header) + 4;
  *outputSize += 4 + strlen("IDAT") + idatSize + 4;
  *outputSize += 4 + strlen("IEND") + 4;
  uint8_t* data = malloc(*outputSize);
  if (data == NULL) {
    return NULL;
  }

  crc_init();
  uint32_t crc;

  // Signature
  memcpy(data, signature, sizeof(signature));
  data += sizeof(signature);

  // IHDR
  memcpy(data, (uint8_t[4]) { 0, 0, 0, sizeof(header) }, 4);
  memcpy(data + 4, "IHDR", 4);
  memcpy(data + 8, header, sizeof(header));
  crc = crc32(data + 4, 4 + sizeof(header));
  memcpy(data + 8 + sizeof(header), (uint8_t[4]) { crc >> 24, crc >> 16, crc >> 8, crc >> 0 }, 4);
  data += 8 + sizeof(header) + 4;

  // IDAT
  memcpy(data, (uint8_t[4]) { idatSize >> 24, idatSize >> 16, idatSize >> 8, idatSize >> 0 }, 4);
  memcpy(data + 4, "IDAT", 4);

  {
    uint8_t* p = data + 8;
    size_t length = imageSize;

    // adler32 counters
    uint64_t s1 = 1, s2 = 0;

    // zlib header
    *p++ = (7 << 4) + (8 << 0);
    *p++ = 1;

    while (length >= rowSize) {

      // 1 indicates the final block
      *p++ = (length == rowSize);

      // Write length and negated length
      memcpy(p + 0, &(uint16_t) {blockSize}, 2);
      memcpy(p + 2, &(uint16_t) {~blockSize}, 2);
      p += 4;

      // Write the filter method (0) and the row data
      *p++ = 0x00;
      memcpy(p, pixels, rowSize);

      // Update adler32
      s1 += 0;
      s2 += s1;
      for (size_t i = 0; i < rowSize; i++) {
        s1 = (s1 + pixels[i]);
        s2 = (s2 + s1);
      }
      s1 %= 65521;
      s2 %= 65521;

      // Update cursors
      p += rowSize;
      pixels += stride;
      length -= rowSize;
    }

    // Write adler32 checksum
    memcpy(p, (uint8_t[4]) { s2 >> 8, s2 >> 0, s1 >> 8, s1 >> 0 }, 4);
  }

  crc = crc32(data + 8, idatSize);
  memcpy(data + 8 + idatSize, (uint8_t[4]) { crc >> 24, crc >> 16, crc >> 8, crc }, 4);
  data += 8 + idatSize + 4;

  // IEND
  memcpy(data, (uint8_t[4]) { 0 }, 4);
  memcpy(data + 4, "IEND", 4);
  crc = crc32(data + 4, 4);
  memcpy(data + 8, (uint8_t[4]) { crc >> 24, crc >> 16, crc >> 8, crc >> 0 }, 4);
  data += 8 + 4;

  return data - *outputSize;
}
