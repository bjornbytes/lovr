#include "headset/headset.h"
#include "thread/thread.h"
#include "lib/vec/vec.h"
#include <stdbool.h>

#pragma once

#define MAX_EVENT_NAME_LENGTH 32

typedef enum {
  EVENT_QUIT,
  EVENT_FOCUS,
  EVENT_MOUNT,
  EVENT_THREAD_ERROR,
  EVENT_CONTROLLER_ADDED,
  EVENT_CONTROLLER_REMOVED,
  EVENT_CONTROLLER_PRESSED,
  EVENT_CONTROLLER_RELEASED,
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
  Ref* ref;
} VariantValue;

typedef struct {
  VariantType type;
  VariantValue value;
} Variant;

typedef vec_t(Variant) vec_variant_t;

typedef struct {
  bool restart;
  int exitCode;
} QuitEvent;

typedef struct {
  bool value;
} BoolEvent;

typedef struct {
  Thread* thread;
  const char* error;
} ThreadEvent;

typedef struct {
  Controller* controller;
  ControllerButton button;
} ControllerEvent;

typedef struct {
  char name[MAX_EVENT_NAME_LENGTH];
  Variant data[4];
  int count;
} CustomEvent;

typedef union {
  QuitEvent quit;
  BoolEvent boolean;
  ThreadEvent thread;
  ControllerEvent controller;
  CustomEvent custom;
} EventData;

typedef struct {
  EventType type;
  EventData data;
} Event;

typedef void (*EventPump)(void);

typedef vec_t(EventPump) vec_pump_t;
typedef vec_t(Event) vec_event_t;

typedef struct {
  bool initialized;
  vec_pump_t pumps;
  vec_event_t events;
} EventState;

void lovrEventInit();
void lovrEventDestroy();
void lovrEventAddPump(EventPump pump);
void lovrEventRemovePump(EventPump pump);
void lovrEventPump();
void lovrEventPush(Event event);
bool lovrEventPoll(Event* event);
void lovrEventClear();
