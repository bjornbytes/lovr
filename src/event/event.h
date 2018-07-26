#include "headset/headset.h"
#ifndef EMSCRIPTEN
#include "thread/thread.h"
#endif
#include "lib/vec/vec.h"
#include <stdbool.h>

#pragma once

typedef enum {
  EVENT_QUIT,
  EVENT_FOCUS,
  EVENT_MOUNT,
  EVENT_THREAD_ERROR,
  EVENT_CONTROLLER_ADDED,
  EVENT_CONTROLLER_REMOVED,
  EVENT_CONTROLLER_PRESSED,
  EVENT_CONTROLLER_RELEASED
} EventType;

typedef struct {
  bool restart;
  int exitCode;
} QuitEvent;

typedef struct {
  bool value;
} BoolEvent;

#ifndef EMSCRIPTEN
typedef struct {
  Thread* thread;
  const char* error;
} ThreadEvent;
#endif

typedef struct {
  Controller* controller;
  ControllerButton button;
} ControllerEvent;

typedef union {
  QuitEvent quit;
  BoolEvent boolean;
#ifndef EMSCRIPTEN
  ThreadEvent thread;
#endif
  ControllerEvent controller;
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
