#include "data.h"
#include "lib/miniz/miniz_tinfl.h"
#include "lib/sdefl/sdefl.h"
#include "util.h"
#include <threads.h>

static thread_local struct sdefl sdefl;

void* lovrDataCompress(const void* data, size_t size, CompressionMethod method, uint32_t level, size_t* outputSize) {
  if (size == 0) {
    *outputSize = 0;
    return lovrMalloc(0);
  }

  const uint8_t gzipHeader[10] = { 0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff };
  size_t bound = sdefl_bound(size) + (method == COMPRESSION_GZIP ? 18 : 0);
  uint8_t* output = lovrMalloc(bound);
  void* dst = output + (method == COMPRESSION_GZIP ? sizeof(gzipHeader) : 0);
  int lvl = level == ~0u ? SDEFL_LVL_DEF : CLAMP((int) level, SDEFL_LVL_MIN, SDEFL_LVL_MAX);
  int compressedSize = (method == COMPRESSION_ZLIB ? zsdeflate : sdeflate)(&sdefl, dst, data, (int) size, lvl);

  if (compressedSize <= 0) {
    lovrFree(output);
    return NULL;
  }

  if (method == COMPRESSION_GZIP) {
    size_t cursor = 0;
    uint32_t crc = lovrDataCRC32(data, size);
    memcpy(output + cursor, gzipHeader, sizeof(gzipHeader)), cursor += sizeof(gzipHeader) + compressedSize;
    memcpy(output + cursor, &crc, sizeof(uint32_t)), cursor += 4;
    memcpy(output + cursor, &(uint32_t) { size }, sizeof(uint32_t));
    *outputSize = (size_t) compressedSize + 18;
  } else {
    *outputSize = (size_t) compressedSize;
  }

  return output;
}

void* lovrDataDecompress(const void* data, size_t size, CompressionMethod method, bool autodetect, size_t* outputSize) {
  const uint8_t* bytes = data;

  bool gzip = size > 18 && bytes[0] == 0x1f && bytes[1] == 0x8b && bytes[2] == 8;

  if (autodetect) {
    if (gzip) {
      method = COMPRESSION_GZIP;
    } else if (bytes[0] == 0x78 && (bytes[1] & 0xf) == 1) {
      method = COMPRESSION_ZLIB;
    } else {
      method = COMPRESSION_DEFLATE;
    }
  }

  size_t bufferSize;

  if (method == COMPRESSION_GZIP) {
    lovrCheck(gzip, "Invalid gzip data");
    uint8_t flags = bytes[3];

    // Read uncompressed size from footer and use it as the initial buffer size
    uint32_t uncompressedSize;
    memcpy(&uncompressedSize, bytes + size - 4, 4);
    bufferSize = (size_t) uncompressedSize;
    size -= 8;

    // Skip the beginning of the header
    bytes += 10;
    size -= 10;

    // Extra data
    if (flags & 0x4) {
      lovrAssert(size > 2, "Invalid gzip data");
      uint16_t extraLength;
      memcpy(&extraLength, bytes, 2);
      lovrAssert(extraLength + 2 < size, "Invalid gzip data");
      bytes += 2 + extraLength;
      size -= 2 + extraLength;
    }

    // File name
    if (flags & 0x8) {
      lovrAssert(size > 0, "Invalid gzip data");
      const uint8_t* end = memchr(bytes, '\0', size);
      lovrAssert(end, "Invalid gzip data");
      size_t length = end - bytes;
      bytes += length + 1;
      size -= length + 1;
    }

    // File comment
    if (flags & 0x10) {
      lovrAssert(size > 0, "Invalid gzip data");
      const uint8_t* end = memchr(bytes, '\0', size);
      lovrAssert(end, "Invalid gzip data");
      size_t length = end - bytes;
      bytes += length + 1;
      size -= length + 1;
    }

    // CRC16
    if (flags & 0x2) {
      lovrAssert(size > 2, "Invalid gzip data");
      bytes += 2;
      size -= 2;
    }

    method = COMPRESSION_DEFLATE;
  } else {
    bufferSize = size * 2;
  }

  tinfl_decompressor decompressor;
  tinfl_init(&decompressor);

  size_t inputCursor = 0;
  size_t outputCursor = 0;
  uint32_t flags = TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF;
  if (method == COMPRESSION_ZLIB) flags |= TINFL_FLAG_PARSE_ZLIB_HEADER;
  void* output = lovrMalloc(bufferSize);

  while (inputCursor < size) {
    const uint8_t* in = bytes + inputCursor;
    uint8_t* out = (uint8_t*) output + outputCursor;
    size_t inSize = size - inputCursor;
    size_t outSize = bufferSize - outputCursor;

    int status = tinfl_decompress(&decompressor, in, &inSize, output, out, &outSize, flags);
    inputCursor += inSize;
    outputCursor += outSize;

    if (status == TINFL_STATUS_DONE) {
      break;
    } else if (status == TINFL_STATUS_HAS_MORE_OUTPUT) {
      bufferSize *= 2;
      output = lovrRealloc(output, bufferSize);
    } else {
      lovrFree(output);
      lovrSetError("Decompression failed");
      return NULL;
    }
  }

  *outputSize = outputCursor;
  return output;
}

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

uint32_t lovrDataCRC32(const void* data, size_t size) {
  crc_init();
  uint32_t c = 0xffffffff;
  const uint8_t* u8 = data;
  for (size_t i = 0; i < size; i++) c = crc_lookup[(c ^ u8[i]) & 0xff] ^ (c >> 8);
  return c ^ 0xffffffff;
}
