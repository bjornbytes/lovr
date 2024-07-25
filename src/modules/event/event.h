#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#pragma once

#define MAX_EVENT_NAME_LENGTH 32

struct Thread;

typedef enum {
  EVENT_QUIT,
  EVENT_RESTART,
  EVENT_VISIBLE,
  EVENT_FOCUS,
  EVENT_RECENTER,
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

typedef enum {
  TYPE_NIL,
  TYPE_BOOLEAN,
  TYPE_NUMBER,
  TYPE_STRING,
  TYPE_TABLE,
  TYPE_MINISTRING,
  TYPE_POINTER,
  TYPE_OBJECT,
  TYPE_VECTOR,
  TYPE_MATRIX
} VariantType;

typedef union {
  bool boolean;
  double number;
  void* pointer;
  struct {
    char* pointer;
    size_t length;
  } string;
  struct {
    uint8_t length;
    char data[23];
  } ministring;
  struct {
    void* pointer;
    const char* type;
    void (*destructor)(void*);
  } object;
  struct {
    int type;
    float data[4];
  } vector;
  struct {
    float* data;
  } matrix;
  struct {
    void* keys;
    void* vals;
    size_t length;
  } table;
} VariantValue;

typedef struct Variant {
  VariantType type;
  VariantValue value;
} Variant;

typedef struct {
  int exitCode;
} QuitEvent;

typedef struct {
  bool value;
} BoolEvent;

typedef struct {
  uint32_t width;
  uint32_t height;
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
  char name[MAX_EVENT_NAME_LENGTH];
  Variant data[4];
  uint32_t count;
} CustomEvent;

typedef union {
  QuitEvent quit;
  BoolEvent boolean;
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

void lovrVariantDestroy(Variant* variant);

bool lovrEventInit(void);
void lovrEventDestroy(void);
void lovrEventPush(Event event);
bool lovrEventPoll(Event* event);
void lovrEventClear(void);
