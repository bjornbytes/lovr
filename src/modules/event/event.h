#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#pragma once

struct Thread;
struct Window;
union Variant;

typedef enum {
  DISPLAY_HEADSET,
  DISPLAY_WINDOW
} DisplayType;

typedef enum {
  EVENT_QUIT,
  EVENT_RESTART,
  EVENT_VISIBLE,
  EVENT_FOCUS,
  EVENT_MOUNT,
  EVENT_RECENTER,
  EVENT_MODELSCHANGED,
  EVENT_RESIZE,
  EVENT_KEYPRESSED,
  EVENT_KEYRELEASED,
  EVENT_TEXTINPUT,
  EVENT_MOUSEPRESSED,
  EVENT_MOUSERELEASED,
  EVENT_MOUSEMOVED,
  EVENT_MOUSEWHEELMOVED,
#ifndef LOVR_DISABLE_THREAD
  EVENT_THREAD_ERROR,
#endif
  EVENT_FILECHANGED,
  EVENT_PERMISSION,
  EVENT_CUSTOM
} EventType;

typedef struct {
  int exitCode;
} QuitEvent;

typedef struct {
  bool visible;
  DisplayType display;
  struct Window* window;
} VisibleEvent;

typedef struct {
  bool focused;
  DisplayType display;
  struct Window* window;
} FocusEvent;

typedef struct {
  bool mounted;
} MountEvent;

typedef struct {
  struct Window* window;
  float width;
  float height;
  float depth;
} ResizeEvent;

typedef struct {
  uint32_t code;
  uint32_t scancode;
  bool repeat;
} KeyEvent;

typedef struct {
  char utf8[4];
  uint32_t codepoint;
} TextEvent;

typedef struct {
  double x;
  double y;
  double dx;
  double dy;
  int button;
} MouseEvent;

typedef struct {
  double x;
  double y;
} MouseWheelEvent;

typedef struct {
  struct Thread* thread;
  char* error;
} ThreadEvent;

typedef struct {
  char* path;
  char* oldpath;
  int action;
} FileEvent;

typedef struct {
  uint32_t permission;
  bool granted;
} PermissionEvent;

typedef struct {
  char name[32];
  union Variant* data;
  uint32_t count;
} CustomEvent;

typedef union {
  QuitEvent quit;
  VisibleEvent visible;
  FocusEvent focus;
  MountEvent mount;
  ResizeEvent resize;
  KeyEvent key;
  TextEvent text;
  MouseEvent mouse;
  MouseWheelEvent wheel;
  ThreadEvent thread;
  FileEvent file;
  PermissionEvent permission;
  CustomEvent custom;
} EventData;

typedef struct {
  EventType type;
  EventData data;
} Event;

bool lovrEventInit(void);
void lovrEventDestroy(void);
void lovrEventPush(Event event);
bool lovrEventPoll(Event* event);
void lovrEventClear(void);
