#include "headset/headset.h"
#include "lib/vec/vec.h"
#include <stdbool.h>

#pragma once

typedef enum {
  EVENT_QUIT,
  EVENT_FOCUS,
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
  bool isFocused;
} FocusEvent;

typedef struct {
  Controller* controller;
} ControllerAddedEvent;

typedef struct {
  Controller* controller;
} ControllerRemovedEvent;

typedef struct {
  Controller* controller;
  ControllerButton button;
} ControllerPressedEvent;

typedef struct {
  Controller* controller;
  ControllerButton button;
} ControllerReleasedEvent;

typedef union {
  QuitEvent quit;
  FocusEvent focus;
  ControllerAddedEvent controlleradded;
  ControllerRemovedEvent controllerremoved;
  ControllerPressedEvent controllerpressed;
  ControllerReleasedEvent controllerreleased;
} EventData;

typedef struct {
  EventType type;
  EventData data;
} Event;

typedef void (*EventPump)(void);

typedef vec_t(EventPump) vec_pump_t;
typedef vec_t(Event) vec_event_t;

typedef struct {
  vec_pump_t pumps;
  vec_event_t events;
} EventState;

void lovrEventInit();
void lovrEventDestroy();
void lovrEventAddPump(void (*pump)(void));
void lovrEventPump();
void lovrEventPush(Event event);
bool lovrEventPoll(Event* event);
void lovrEventClear();
