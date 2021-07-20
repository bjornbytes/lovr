#include "os.h"
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <pwd.h>
#include <sys/mman.h>
#include <linux/input.h>
#include <xcb/xcb.h>

#define NS_PER_SEC 1000000000ULL

static os_key convertKey(uint8_t keycode);

static struct {
  xcb_connection_t* connection;
  xcb_intern_atom_reply_t* deleteWindow;
  fn_quit* onQuit;
  fn_focus* onFocus;
  fn_key* onKey;
  fn_text* onText;
  bool keyDown[OS_KEY_COUNT];
  bool mouseDown[2];
  double mouseX;
  double mouseY;
} state;

bool os_init(void) {
  return true;
}

void os_destroy(void) {
  free(state.deleteWindow);
  xcb_disconnect(state.connection);
  memset(&state, 0, sizeof(state));
}

const char* os_get_name(void) {
  return "Linux";
}

uint32_t os_get_core_count(void) {
  return sysconf(_SC_NPROCESSORS_ONLN);
}

void os_open_console(void) {
  //
}

double os_get_time(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (double) t.tv_sec + (t.tv_nsec / (double) NS_PER_SEC);
}

void os_sleep(double seconds) {
  seconds += .5e-9;
  struct timespec t;
  t.tv_sec = seconds;
  t.tv_nsec = (seconds - t.tv_sec) * NS_PER_SEC;
  while (nanosleep(&t, &t));
}

void os_request_permission(os_permission permission) {
  //
}

void* os_vm_init(size_t size) {
  return mmap(NULL, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
}

bool os_vm_free(void* p, size_t size) {
  return !munmap(p, size);
}

bool os_vm_commit(void* p, size_t size) {
  return !mprotect(p, size, PROT_READ | PROT_WRITE);
}

bool os_vm_release(void* p, size_t size) {
  return !madvise(p, size, MADV_DONTNEED);
}

void os_thread_attach(void) {
  //
}

void os_thread_detach(void) {
  //
}

void os_poll_events() {
  if (!state.connection) return;

  union {
    xcb_generic_event_t* any;
    xcb_client_message_event_t* message;
    xcb_focus_in_event_t* focus;
    xcb_key_press_event_t* key;
    xcb_button_press_event_t* mouse;
    xcb_motion_notify_event_t* motion;
  } event;

  while ((event.any = xcb_poll_for_event(state.connection)) != NULL) {
    uint8_t type = event.any->response_type & 0xf;

    switch (type) {
      case XCB_CLIENT_MESSAGE:
        if (event.message->data.data32[0] == state.deleteWindow->atom && state.onQuit) {
          state.onQuit();
        }
        break;

      case XCB_KEY_PRESS:
      case XCB_KEY_RELEASE: {
        uint8_t keycode = event.key->detail;
        os_key key = convertKey(keycode);

        if (key < OS_KEY_COUNT) {
          state.keyDown[key] = type == XCB_KEY_PRESS;
          if (state.onKey) state.onKey(type == XCB_KEY_RELEASE, key, keycode, false); // TODO repeat
        }

        if (type == XCB_KEY_PRESS && state.onText) {
          // TODO xkb
        }
        break;
      }

      case XCB_BUTTON_PRESS:
      case XCB_BUTTON_RELEASE:
        switch (event.mouse->detail) {
          case 1: state.mouseDown[MOUSE_LEFT] = type == XCB_BUTTON_PRESS; break;
          case 3: state.mouseDown[MOUSE_RIGHT] = type == XCB_BUTTON_PRESS; break;
          default: break;
        }
        break;

      case XCB_MOTION_NOTIFY:
        state.mouseX = event.motion->event_x;
        state.mouseY = event.motion->event_y;
        break;

      case XCB_FOCUS_IN:
      case XCB_FOCUS_OUT:
        if (event.focus->mode == XCB_NOTIFY_MODE_GRAB || event.focus->mode == XCB_NOTIFY_MODE_UNGRAB) break;
        if (state.onFocus) state.onFocus(type == XCB_FOCUS_IN);
        break;

      default: break; // TODO xkb
    }

    free(event.any);
  }
}

void os_on_quit(fn_quit* callback) {
  state.onQuit = callback;
}

void os_on_focus(fn_focus* callback) {
  state.onFocus = callback;
}

void os_on_resize(fn_resize* callback) {
  // TODO
}

void os_on_key(fn_key* callback) {
  state.onKey = callback;
}

void os_on_text(fn_text* callback) {
  state.onText = callback;
}

void os_on_permission(fn_permission* callback) {
  //
}

bool os_window_open(const os_window_config* config) {
  state.connection = xcb_connect(NULL, NULL);

  if (xcb_connection_has_error(state.connection)) {
    xcb_disconnect(state.connection);
    return false;
  }

  xcb_screen_t* screen = xcb_setup_roots_iterator(xcb_get_setup(state.connection)).data;

  uint32_t values[] = {
    screen->black_pixel,
    XCB_EVENT_MASK_KEY_PRESS |
    XCB_EVENT_MASK_KEY_RELEASE |
    XCB_EVENT_MASK_BUTTON_PRESS |
    XCB_EVENT_MASK_BUTTON_RELEASE |
    XCB_EVENT_MASK_POINTER_MOTION |
    XCB_EVENT_MASK_FOCUS_CHANGE
  };

  xcb_window_t window = xcb_generate_id(state.connection);

  // TODO fullscreen
  // TODO resizable
  // TODO title
  // TODO icon
  xcb_create_window(
    state.connection,
    XCB_COPY_FROM_PARENT,
    window,
    screen->root,
    0,
    0,
    config->width,
    config->height,
    0,
    XCB_WINDOW_CLASS_INPUT_OUTPUT,
    screen->root_visual,
    XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK,
    values
  );

  xcb_map_window(state.connection, window);

  xcb_intern_atom_cookie_t protocolCookie = xcb_intern_atom(state.connection, 1, 12, "WM_PROTOCOLS");
  xcb_intern_atom_reply_t* protocolReply = xcb_intern_atom_reply(state.connection, protocolCookie, 0);
  xcb_intern_atom_cookie_t deleteCookie = xcb_intern_atom(state.connection, 0, 16, "WM_DELETE_WINDOW");
  state.deleteWindow = xcb_intern_atom_reply(state.connection, deleteCookie, 0);
  xcb_change_property(state.connection, XCB_PROP_MODE_REPLACE, window, protocolReply->atom, 4, 32, 1, &state.deleteWindow->atom);

  xcb_flush(state.connection);
  free(protocolReply);

  // TODO EGL

  return true;
}

bool os_window_is_open(void) {
  return state.connection;
}

void os_window_get_size(uint32_t* width, uint32_t* height) {
  *width = *height = 0; // TODO
}

void os_window_get_fbsize(uint32_t* width, uint32_t* height) {
  *width = *height = 0; // TODO
}

void os_window_message_box(const char* message) {
  //
}

void os_window_set_vsync(int interval) {
  // TODO EGL
}

void os_window_swap(void) {
  // TODO EGL
}

fn_gl_proc* os_get_gl_proc_address(const char* function) {
  return NULL; // TODO EGL
}

size_t os_get_home_directory(char* buffer, size_t size) {
  const char* path = getenv("HOME");

  if (!path) {
    struct passwd* entry = getpwuid(getuid());
    if (!entry) {
      return 0;
    }
    path = entry->pw_dir;
  }

  size_t length = strlen(path);
  if (length >= size) { return 0; }
  memcpy(buffer, path, length);
  buffer[length] = '\0';
  return length;
}

size_t os_get_data_directory(char* buffer, size_t size) {
  const char* xdg = getenv("XDG_DATA_HOME");

  if (xdg) {
    size_t length = strlen(xdg);
    if (length < size) {
      memcpy(buffer, xdg, length);
      buffer[length] = '\0';
      return length;
    }
  } else {
    size_t cursor = os_get_home_directory(buffer, size);
    if (cursor > 0) {
      buffer += cursor;
      size -= cursor;
      const char* suffix = "/.local/share";
      size_t length = strlen(suffix);
      if (length < size) {
        memcpy(buffer, suffix, length);
        buffer[length] = '\0';
        return cursor + length;
      }
    }
  }

  return 0;
}

size_t os_get_working_directory(char* buffer, size_t size) {
  return getcwd(buffer, size) ? strlen(buffer) : 0;
}

size_t os_get_executable_path(char* buffer, size_t size) {
  ssize_t length = readlink("/proc/self/exe", buffer, size - 1);
  if (length >= 0) {
    buffer[length] = '\0';
    return length;
  } else {
    return 0;
  }
}

size_t os_get_bundle_path(char* buffer, size_t size, const char** root) {
  *root = NULL;
  return os_get_executable_path(buffer, size);
}

void os_get_mouse_position(double* x, double* y) {
  *x = state.mouseX;
  *y = state.mouseY;
}

void os_set_mouse_mode(os_mouse_mode mode) {
  // TODO
}

bool os_is_mouse_down(os_mouse_button button) {
  return state.mouseDown[button];
}

bool os_is_key_down(os_key key) {
  return state.keyDown[key];
}

static os_key convertKey(uint8_t keycode) {
  switch (keycode - 8) {
    case KEY_ESC: return OS_KEY_ESCAPE;
    case KEY_1: return OS_KEY_1;
    case KEY_2: return OS_KEY_2;
    case KEY_3: return OS_KEY_3;
    case KEY_4: return OS_KEY_4;
    case KEY_5: return OS_KEY_5;
    case KEY_6: return OS_KEY_6;
    case KEY_7: return OS_KEY_7;
    case KEY_8: return OS_KEY_8;
    case KEY_9: return OS_KEY_9;
    case KEY_0: return OS_KEY_0;
    case KEY_MINUS: return OS_KEY_MINUS;
    case KEY_EQUAL: return OS_KEY_EQUALS;
    case KEY_BACKSPACE: return OS_KEY_BACKSPACE;
    case KEY_TAB: return OS_KEY_TAB;
    case KEY_Q: return OS_KEY_Q;
    case KEY_W: return OS_KEY_W;
    case KEY_E: return OS_KEY_E;
    case KEY_R: return OS_KEY_R;
    case KEY_T: return OS_KEY_T;
    case KEY_Y: return OS_KEY_Y;
    case KEY_U: return OS_KEY_U;
    case KEY_I: return OS_KEY_I;
    case KEY_O: return OS_KEY_O;
    case KEY_P: return OS_KEY_P;
    case KEY_LEFTBRACE: return OS_KEY_LEFT_BRACKET;
    case KEY_RIGHTBRACE: return OS_KEY_RIGHT_BRACKET;
    case KEY_ENTER: return OS_KEY_ENTER;
    case KEY_LEFTCTRL: return OS_KEY_LEFT_CONTROL;
    case KEY_A: return OS_KEY_A;
    case KEY_S: return OS_KEY_S;
    case KEY_D: return OS_KEY_D;
    case KEY_F: return OS_KEY_F;
    case KEY_G: return OS_KEY_G;
    case KEY_H: return OS_KEY_H;
    case KEY_J: return OS_KEY_J;
    case KEY_K: return OS_KEY_K;
    case KEY_L: return OS_KEY_L;
    case KEY_SEMICOLON: return OS_KEY_SEMICOLON;
    case KEY_APOSTROPHE: return OS_KEY_APOSTROPHE;
    case KEY_GRAVE: return OS_KEY_BACKTICK;
    case KEY_LEFTSHIFT: return OS_KEY_LEFT_SHIFT;
    case KEY_BACKSLASH: return OS_KEY_BACKSLASH;
    case KEY_Z: return OS_KEY_Z;
    case KEY_X: return OS_KEY_X;
    case KEY_C: return OS_KEY_C;
    case KEY_V: return OS_KEY_V;
    case KEY_B: return OS_KEY_B;
    case KEY_N: return OS_KEY_N;
    case KEY_M: return OS_KEY_M;
    case KEY_COMMA: return OS_KEY_COMMA;
    case KEY_DOT: return OS_KEY_PERIOD;
    case KEY_SLASH: return OS_KEY_SLASH;
    case KEY_RIGHTSHIFT: return OS_KEY_RIGHT_SHIFT;
    case KEY_LEFTALT: return OS_KEY_LEFT_ALT;
    case KEY_SPACE: return OS_KEY_SPACE;
    case KEY_CAPSLOCK: return OS_KEY_CAPS_LOCK;
    case KEY_F1: return OS_KEY_F1;
    case KEY_F2: return OS_KEY_F2;
    case KEY_F3: return OS_KEY_F3;
    case KEY_F4: return OS_KEY_F4;
    case KEY_F5: return OS_KEY_F5;
    case KEY_F6: return OS_KEY_F6;
    case KEY_F7: return OS_KEY_F7;
    case KEY_F8: return OS_KEY_F8;
    case KEY_F9: return OS_KEY_F9;
    case KEY_F10: return OS_KEY_F10;
    case KEY_NUMLOCK: return OS_KEY_NUM_LOCK;
    case KEY_SCROLLLOCK: return OS_KEY_SCROLL_LOCK;
    case KEY_F11: return OS_KEY_F11;
    case KEY_F12: return OS_KEY_F12;
    case KEY_RIGHTCTRL: return OS_KEY_RIGHT_CONTROL;
    case KEY_RIGHTALT: return OS_KEY_RIGHT_ALT;
    case KEY_HOME: return OS_KEY_HOME;
    case KEY_UP: return OS_KEY_UP;
    case KEY_PAGEUP: return OS_KEY_PAGE_UP;
    case KEY_LEFT: return OS_KEY_LEFT;
    case KEY_RIGHT: return OS_KEY_RIGHT;
    case KEY_END: return OS_KEY_END;
    case KEY_DOWN: return OS_KEY_DOWN;
    case KEY_PAGEDOWN: return OS_KEY_PAGE_DOWN;
    case KEY_INSERT: return OS_KEY_INSERT;
    case KEY_DELETE: return OS_KEY_DELETE;
    case KEY_LEFTMETA: return OS_KEY_LEFT_OS;
    case KEY_RIGHTMETA: return OS_KEY_RIGHT_OS;
    default: return OS_KEY_COUNT;
  }
}
