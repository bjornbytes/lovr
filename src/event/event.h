#include "headset/headset.h"
#include "vendor/vec/vec.h"

#ifndef LOVR_EVENT_TYPES
#define LOVR_EVENT_TYPES

typedef enum {
  EVENT_QUIT,
  EVENT_CONTROLLER_ADDED,
  EVENT_CONTROLLER_REMOVED
} EventType;

typedef struct {
  int exitCode;
} QuitEvent;

typedef struct {
  Controller* controller;
} ControllerAddedEvent;

typedef struct {
  Controller* controller;
} ControllerRemovedEvent;

typedef union {
  QuitEvent quit;
  ControllerAddedEvent controlleradded;
  ControllerRemovedEvent controllerremoved;
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

#endif

void lovrEventInit();
void lovrEventDestroy();
void lovrEventAddPump(void (*pump)(void));
void lovrEventPump();
void lovrEventPush(Event event);
Event* lovrEventPoll();
void lovrEventClear();
