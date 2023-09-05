#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#pragma once

typedef void fn_on_header(void* userdata, const char* name, size_t nameLength, const char* value, size_t valueLength);

typedef struct {
  const char* url;
  const char* method;
  const char** headers;
  uint32_t headerCount;
  const char* data;
  size_t size;
} Request;

typedef struct {
  const char* error;
  uint32_t status;
  char* data;
  size_t size;
  fn_on_header* onHeader;
  void* userdata;
} Response;

bool lovrHttpInit(void);
void lovrHttpDestroy(void);
bool lovrHttpRequest(Request* request, Response* response);
