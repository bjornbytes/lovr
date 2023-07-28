#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#pragma once

typedef void fn_on_header(void* userdata, const char* name, size_t nameLength, const char* value, size_t valueLength);
typedef void fn_on_data(void* userdata, void* data, size_t length);

typedef struct {
  const char* url;
  const char* method;
  const char** headers;
  uint32_t headerCount;
  const char* data;
  size_t size;
  uint32_t timeout;
} Request;

typedef struct {
  uint32_t status;
  char* data;
  size_t size;
  fn_on_header* onHeader;
  void* userdata;
} Response;

bool lovrHttpInit(void);
void lovrHttpDestroy(void);
bool lovrHttpRequest(Request* request, Response* response);
