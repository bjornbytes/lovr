#include <stdint.h>
#include <stddef.h>

#pragma once

// The world's worst png encoder (uncompressed), for now just free the data when you're done

void* png_encode(uint8_t* pixels, uint32_t width, uint32_t height, int32_t stride, size_t* outputSize);
