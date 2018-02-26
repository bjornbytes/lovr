#include "util.h"
#include "lib/tinycthread/tinycthread.h"
#include "lib/vec/vec.h"
#include <stdbool.h>
#include <stdint.h>

#pragma once

typedef enum {
  TYPE_NIL,
  TYPE_BOOLEAN,
  TYPE_NUMBER,
  TYPE_STRING,
  TYPE_OBJECT
} VariantType;

typedef union {
  bool boolean;
  double number;
  char* string;
  Ref* ref;
} VariantValue;

typedef struct {
  const char* meta;
  VariantType type;
  VariantValue value;
} Variant;

typedef vec_t(Variant) vec_variant_t;

typedef struct {
  Ref ref;
  mtx_t lock;
  cnd_t cond;
  vec_variant_t messages;
  uint64_t sent;
  uint64_t received;
} Channel;

Channel* lovrChannelCreate();
void lovrChannelDestroy(void* ref);
bool lovrChannelPush(Channel* channel, Variant variant, double timeout, uint64_t* id);
bool lovrChannelPop(Channel* channel, Variant* variant, double timeout);
bool lovrChannelPeek(Channel* channel, Variant* variant);
void lovrChannelClear(Channel* channel);
uint64_t lovrChannelGetCount(Channel* channel);
bool lovrChannelHasRead(Channel* channel, uint64_t id);
