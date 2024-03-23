#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#pragma once

typedef struct os_window_config {
  uint32_t width;
  uint32_t height;
  bool fullscreen;
  bool resizable;
  const char* title;
  struct {
    void* data;
    uint32_t width;
    uint32_t height;
  } icon;
} os_window_config;

typedef enum {
  MOUSE_LEFT,
  MOUSE_RIGHT
} os_mouse_button;

typedef enum {
  MOUSE_MODE_NORMAL,
  MOUSE_MODE_GRABBED
} os_mouse_mode;

typedef enum {
  OS_KEY_A,
  OS_KEY_B,
  OS_KEY_C,
  OS_KEY_D,
  OS_KEY_E,
  OS_KEY_F,
  OS_KEY_G,
  OS_KEY_H,
  OS_KEY_I,
  OS_KEY_J,
  OS_KEY_K,
  OS_KEY_L,
  OS_KEY_M,
  OS_KEY_N,
  OS_KEY_O,
  OS_KEY_P,
  OS_KEY_Q,
  OS_KEY_R,
  OS_KEY_S,
  OS_KEY_T,
  OS_KEY_U,
  OS_KEY_V,
  OS_KEY_W,
  OS_KEY_X,
  OS_KEY_Y,
  OS_KEY_Z,
  OS_KEY_0,
  OS_KEY_1,
  OS_KEY_2,
  OS_KEY_3,
  OS_KEY_4,
  OS_KEY_5,
  OS_KEY_6,
  OS_KEY_7,
  OS_KEY_8,
  OS_KEY_9,
  OS_KEY_SPACE,
  OS_KEY_ENTER,
  OS_KEY_TAB,
  OS_KEY_ESCAPE,
  OS_KEY_BACKSPACE,
  OS_KEY_UP,
  OS_KEY_DOWN,
  OS_KEY_LEFT,
  OS_KEY_RIGHT,
  OS_KEY_HOME,
  OS_KEY_END,
  OS_KEY_PAGE_UP,
  OS_KEY_PAGE_DOWN,
  OS_KEY_INSERT,
  OS_KEY_DELETE,
  OS_KEY_F1,
  OS_KEY_F2,
  OS_KEY_F3,
  OS_KEY_F4,
  OS_KEY_F5,
  OS_KEY_F6,
  OS_KEY_F7,
  OS_KEY_F8,
  OS_KEY_F9,
  OS_KEY_F10,
  OS_KEY_F11,
  OS_KEY_F12,
  OS_KEY_BACKTICK,
  OS_KEY_MINUS,
  OS_KEY_EQUALS,
  OS_KEY_LEFT_BRACKET,
  OS_KEY_RIGHT_BRACKET,
  OS_KEY_BACKSLASH,
  OS_KEY_SEMICOLON,
  OS_KEY_APOSTROPHE,
  OS_KEY_COMMA,
  OS_KEY_PERIOD,
  OS_KEY_SLASH,
  OS_KEY_KP_0,
  OS_KEY_KP_1,
  OS_KEY_KP_2,
  OS_KEY_KP_3,
  OS_KEY_KP_4,
  OS_KEY_KP_5,
  OS_KEY_KP_6,
  OS_KEY_KP_7,
  OS_KEY_KP_8,
  OS_KEY_KP_9,
  OS_KEY_KP_DECIMAL,
  OS_KEY_KP_DIVIDE,
  OS_KEY_KP_MULTIPLY,
  OS_KEY_KP_SUBTRACT,
  OS_KEY_KP_ADD,
  OS_KEY_KP_ENTER,
  OS_KEY_KP_EQUALS,
  OS_KEY_LEFT_CONTROL,
  OS_KEY_LEFT_SHIFT,
  OS_KEY_LEFT_ALT,
  OS_KEY_LEFT_OS,
  OS_KEY_RIGHT_CONTROL,
  OS_KEY_RIGHT_SHIFT,
  OS_KEY_RIGHT_ALT,
  OS_KEY_RIGHT_OS,
  OS_KEY_CAPS_LOCK,
  OS_KEY_SCROLL_LOCK,
  OS_KEY_NUM_LOCK,
  OS_KEY_COUNT
} os_key;

typedef enum {
  BUTTON_PRESSED,
  BUTTON_RELEASED
} os_button_action;

typedef enum {
  OS_PERMISSION_AUDIO_CAPTURE
} os_permission;

typedef void fn_quit(void);
typedef void fn_focus(bool focused);
typedef void fn_resize(uint32_t width, uint32_t height);
typedef void fn_key(os_button_action action, os_key key, uint32_t scancode, bool repeat);
typedef void fn_text(uint32_t codepoint);
typedef void fn_mouse_button(int button, bool pressed);
typedef void fn_mouse_move(double x, double y);
typedef void fn_mousewheel_move(double deltaX, double deltaY);
typedef void fn_permission(os_permission permission, bool granted);

bool os_init(void);
void os_destroy(void);
const char* os_get_name(void);
uint32_t os_get_core_count(void);
void os_open_console(void);
double os_get_time(void);
void os_sleep(double seconds);
void os_request_permission(os_permission permission);
const char* os_get_clipboard_text(void);
void os_set_clipboard_text(const char* text);

void* os_vm_init(size_t size);
bool os_vm_free(void* p, size_t size);
bool os_vm_commit(void* p, size_t size);
bool os_vm_release(void* p, size_t size);

void os_thread_attach(void);
void os_thread_detach(void);

void os_poll_events(void);
void os_on_quit(fn_quit* callback);
void os_on_focus(fn_focus* callback);
void os_on_resize(fn_resize* callback);
void os_on_key(fn_key* callback);
void os_on_text(fn_text* callback);
void os_on_mouse_button(fn_mouse_button* callback);
void os_on_mouse_move(fn_mouse_move* callback);
void os_on_mousewheel_move(fn_mousewheel_move* callback);
void os_on_permission(fn_permission* callback);

bool os_window_open(const os_window_config* config);
bool os_window_is_open(void);
void os_window_get_size(uint32_t* width, uint32_t* height);
float os_window_get_pixel_density(void);
void os_window_message_box(const char* message);

size_t os_get_home_directory(char* buffer, size_t size);
size_t os_get_data_directory(char* buffer, size_t size);
size_t os_get_working_directory(char* buffer, size_t size);
size_t os_get_executable_path(char* buffer, size_t size);
size_t os_get_bundle_path(char* buffer, size_t size, const char** root);

void os_get_mouse_position(double* x, double* y);
void os_set_mouse_mode(os_mouse_mode mode);
bool os_is_mouse_down(os_mouse_button button);
bool os_is_key_down(os_key key);

uintptr_t os_get_win32_window(void);
uintptr_t os_get_win32_instance(void);

uintptr_t os_get_ca_metal_layer(void);

uintptr_t os_get_xcb_connection(void);
uintptr_t os_get_xcb_window(void);
