#include <stdbool.h>
#include <stdint.h>

#pragma once

#define MAX_EVENT_NAME_LENGTH 32

struct Thread;

typedef enum {
  EVENT_QUIT,
  EVENT_FOCUS,
  EVENT_THREAD_ERROR,
  EVENT_CUSTOM
} EventType;

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
  struct {
    void* pointer;
    const char* type;
    void (*destructor)(void*);
  } object;
} VariantValue;

typedef struct Variant {
  VariantType type;
  VariantValue value;
} Variant;

typedef struct {
  bool restart;
  int exitCode;
} QuitEvent;

typedef struct {
  bool value;
} BoolEvent;

typedef struct {
  struct Thread* thread;
  char* error;
} ThreadEvent;

typedef struct {
  char name[MAX_EVENT_NAME_LENGTH];
  Variant data[4];
  uint32_t count;
} CustomEvent;

typedef union {
  QuitEvent quit;
  BoolEvent boolean;
  ThreadEvent thread;
  CustomEvent custom;
} EventData;

typedef struct {
  EventType type;
  EventData data;
} Event;

typedef void (*EventPump)(void);

void lovrVariantDestroy(Variant* variant);

bool lovrEventInit(void);
void lovrEventDestroy(void);
void lovrEventAddPump(EventPump pump);
void lovrEventRemovePump(EventPump pump);
void lovrEventPump(void);
void lovrEventPush(Event event);
bool lovrEventPoll(Event* event);
void lovrEventClear(void);
