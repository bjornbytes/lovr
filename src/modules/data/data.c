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

  size_t bound = sdefl_bound(size) + (method == COMPRESSION_GZIP ? 18 : 0);
  uint8_t* output = lovrMalloc(bound);
  void* dst = method == COMPRESSION_GZIP ? output + 10 : output;
  int lvl = level == ~0u ? SDEFL_LVL_DEF : CLAMP((int) level, SDEFL_LVL_MIN, SDEFL_LVL_MAX);
  int result = (method == COMPRESSION_ZLIB ? zsdeflate : sdeflate)(&sdefl, dst, data, (int) size, lvl);

  if (result <= 0) {
    lovrFree(output);
    return NULL;
  }

  if (method == COMPRESSION_GZIP) {
    output[0] = 0x1f;
    output[1] = 0x8b;
    output[2] = 8;
    output[3] = 0;
    output[4] = 0;
    output[5] = 0;
    output[6] = 0;
    output[7] = 0;
    output[8] = 0;
    output[9] = 0xff;
    uint32_t crc = lovrDataCRC32(data, size);
    uint32_t size32 = (uint32_t) size;
    memcpy(output + 10 + result, &crc, sizeof(uint32_t));
    memcpy(output + 10 + result + 4, &size32, sizeof(uint32_t));
    *outputSize = (size_t) result + 18;
  } else {
    *outputSize = (size_t) result;
  }

  return output;
}

void* lovrDataDecompress(const void* data, size_t size, CompressionMethod method, bool autodetect, size_t* outputSize) {
  const uint8_t* bytes = data;
  bool gzip = bytes[0] == 0x1f && bytes[1] == 0x8b && bytes[2] == 8;

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
    size_t offset = 10;

    // Extra data
    if (flags & 0x4) {
      uint16_t extraLength;
      memcpy(&extraLength, bytes + offset, 2);
      offset += 2 + extraLength;
      lovrAssert(offset < size, "Invalid gzip data");
    }

    // File name
    if (flags & 0x8) {
      offset += strnlen(bytes + offset, size - offset) + 1;
    }

    // File comment
    if (flags & 0x10) {
      offset += strnlen(bytes + offset, size - offset) + 1;
    }

    // CRC16
    if (flags & 0x2) {
      offset += 2;
    }

    uint32_t uncompressedSize;
    memcpy(&uncompressedSize, data + size - 4, 4);
    bufferSize = (size_t) uncompressedSize;

    lovrAssert(offset < size, "Invalid gzip data");

    data = (char*) data + offset;
    size = size - offset;

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
    uint8_t* in = (uint8_t*) data + inputCursor;
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
