#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_ONLY_HDR
#define STBI_ASSERT(x)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-compare"
#include "stb_image.h"
#pragma clang diagnostic pop
