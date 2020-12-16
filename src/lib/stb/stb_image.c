#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_ONLY_HDR
#define STBI_ASSERT(x)
#define STBI_FAILURE_USERMSG
#include "stb_image.h"

#ifndef LOVR_STBI_VFLIP_PATCH
#error "Somebody updated stb_image.h without replacing the thread-local patch"
#endif
