#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#pragma once

typedef enum {
  COMPRESSION_DEFLATE,
  COMPRESSION_ZLIB,
  COMPRESSION_GZIP
} CompressionMethod;

void* lovrDataCompress(const void* data, size_t size, CompressionMethod method, uint32_t level, size_t* outputSize);
void* lovrDataDecompress(const void* data, size_t size, CompressionMethod method, bool autodetect, size_t* outputSize);
uint32_t lovrDataCRC32(const void* data, size_t size);
