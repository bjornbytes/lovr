#include <vendor/vec/vec.h>

#ifndef LOVR_EVENT_TYPES
#define LOVR_EVENT_TYPES

typedef enum {
  EVENT_QUIT
} EventType;

typedef struct {
  int exitCode;
} QuitEvent;

typedef union {
  QuitEvent quit;
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
