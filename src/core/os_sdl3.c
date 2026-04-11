#include "os.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#else
#include <unistd.h>
#endif

static size_t copy_path(char* buffer, size_t size, const char* path) {
  if (!path) return 0;
  size_t len = strlen(path);
  if (len >= size) return 0;
  memcpy(buffer, path, len);
  buffer[len] = '\0';
  return len;
}

SDL_Window* lovr_sdl_window;

static struct {
  SDL_Window* window;
  fn_quit* onQuit;
  fn_visible* onVisible;
  fn_focus* onFocus;
  fn_resize* onResize;
  fn_key* onKey;
  fn_text* onText;
  fn_mouse_button* onMouseButton;
  fn_mouse_move* onMouseMove;
  fn_mousewheel_move* onWheelMove;
  fn_permission* onPermission;
  uint32_t width;
  uint32_t height;
  uint32_t windowedX;
  uint32_t windowedY;
  uint32_t windowedW;
  uint32_t windowedH;
  bool fullscreen;
  double mouseX;
  double mouseY;
} state;

static char* clipboard_text = NULL;

static os_key convert_scancode(SDL_Scancode scancode) {
  switch (scancode) {
    case SDL_SCANCODE_A: return OS_KEY_A;
    case SDL_SCANCODE_B: return OS_KEY_B;
    case SDL_SCANCODE_C: return OS_KEY_C;
    case SDL_SCANCODE_D: return OS_KEY_D;
    case SDL_SCANCODE_E: return OS_KEY_E;
    case SDL_SCANCODE_F: return OS_KEY_F;
    case SDL_SCANCODE_G: return OS_KEY_G;
    case SDL_SCANCODE_H: return OS_KEY_H;
    case SDL_SCANCODE_I: return OS_KEY_I;
    case SDL_SCANCODE_J: return OS_KEY_J;
    case SDL_SCANCODE_K: return OS_KEY_K;
    case SDL_SCANCODE_L: return OS_KEY_L;
    case SDL_SCANCODE_M: return OS_KEY_M;
    case SDL_SCANCODE_N: return OS_KEY_N;
    case SDL_SCANCODE_O: return OS_KEY_O;
    case SDL_SCANCODE_P: return OS_KEY_P;
    case SDL_SCANCODE_Q: return OS_KEY_Q;
    case SDL_SCANCODE_R: return OS_KEY_R;
    case SDL_SCANCODE_S: return OS_KEY_S;
    case SDL_SCANCODE_T: return OS_KEY_T;
    case SDL_SCANCODE_U: return OS_KEY_U;
    case SDL_SCANCODE_V: return OS_KEY_V;
    case SDL_SCANCODE_W: return OS_KEY_W;
    case SDL_SCANCODE_X: return OS_KEY_X;
    case SDL_SCANCODE_Y: return OS_KEY_Y;
    case SDL_SCANCODE_Z: return OS_KEY_Z;
    case SDL_SCANCODE_1: return OS_KEY_1;
    case SDL_SCANCODE_2: return OS_KEY_2;
    case SDL_SCANCODE_3: return OS_KEY_3;
    case SDL_SCANCODE_4: return OS_KEY_4;
    case SDL_SCANCODE_5: return OS_KEY_5;
    case SDL_SCANCODE_6: return OS_KEY_6;
    case SDL_SCANCODE_7: return OS_KEY_7;
    case SDL_SCANCODE_8: return OS_KEY_8;
    case SDL_SCANCODE_9: return OS_KEY_9;
    case SDL_SCANCODE_0: return OS_KEY_0;
    case SDL_SCANCODE_RETURN: return OS_KEY_ENTER;
    case SDL_SCANCODE_ESCAPE: return OS_KEY_ESCAPE;
    case SDL_SCANCODE_BACKSPACE: return OS_KEY_BACKSPACE;
    case SDL_SCANCODE_TAB: return OS_KEY_TAB;
    case SDL_SCANCODE_SPACE: return OS_KEY_SPACE;
    case SDL_SCANCODE_MINUS: return OS_KEY_MINUS;
    case SDL_SCANCODE_EQUALS: return OS_KEY_EQUALS;
    case SDL_SCANCODE_LEFTBRACKET: return OS_KEY_LEFT_BRACKET;
    case SDL_SCANCODE_RIGHTBRACKET: return OS_KEY_RIGHT_BRACKET;
    case SDL_SCANCODE_BACKSLASH: return OS_KEY_BACKSLASH;
    case SDL_SCANCODE_SEMICOLON: return OS_KEY_SEMICOLON;
    case SDL_SCANCODE_APOSTROPHE: return OS_KEY_APOSTROPHE;
    case SDL_SCANCODE_GRAVE: return OS_KEY_BACKTICK;
    case SDL_SCANCODE_COMMA: return OS_KEY_COMMA;
    case SDL_SCANCODE_PERIOD: return OS_KEY_PERIOD;
    case SDL_SCANCODE_SLASH: return OS_KEY_SLASH;
    case SDL_SCANCODE_F1: return OS_KEY_F1;
    case SDL_SCANCODE_F2: return OS_KEY_F2;
    case SDL_SCANCODE_F3: return OS_KEY_F3;
    case SDL_SCANCODE_F4: return OS_KEY_F4;
    case SDL_SCANCODE_F5: return OS_KEY_F5;
    case SDL_SCANCODE_F6: return OS_KEY_F6;
    case SDL_SCANCODE_F7: return OS_KEY_F7;
    case SDL_SCANCODE_F8: return OS_KEY_F8;
    case SDL_SCANCODE_F9: return OS_KEY_F9;
    case SDL_SCANCODE_F10: return OS_KEY_F10;
    case SDL_SCANCODE_F11: return OS_KEY_F11;
    case SDL_SCANCODE_F12: return OS_KEY_F12;
    case SDL_SCANCODE_INSERT: return OS_KEY_INSERT;
    case SDL_SCANCODE_DELETE: return OS_KEY_DELETE;
    case SDL_SCANCODE_HOME: return OS_KEY_HOME;
    case SDL_SCANCODE_END: return OS_KEY_END;
    case SDL_SCANCODE_PAGEUP: return OS_KEY_PAGE_UP;
    case SDL_SCANCODE_PAGEDOWN: return OS_KEY_PAGE_DOWN;
    case SDL_SCANCODE_UP: return OS_KEY_UP;
    case SDL_SCANCODE_DOWN: return OS_KEY_DOWN;
    case SDL_SCANCODE_LEFT: return OS_KEY_LEFT;
    case SDL_SCANCODE_RIGHT: return OS_KEY_RIGHT;
    case SDL_SCANCODE_CAPSLOCK: return OS_KEY_CAPS_LOCK;
    case SDL_SCANCODE_SCROLLLOCK: return OS_KEY_SCROLL_LOCK;
    case SDL_SCANCODE_NUMLOCKCLEAR: return OS_KEY_NUM_LOCK;
    case SDL_SCANCODE_KP_DIVIDE: return OS_KEY_KP_DIVIDE;
    case SDL_SCANCODE_KP_MULTIPLY: return OS_KEY_KP_MULTIPLY;
    case SDL_SCANCODE_KP_MINUS: return OS_KEY_KP_SUBTRACT;
    case SDL_SCANCODE_KP_PLUS: return OS_KEY_KP_ADD;
    case SDL_SCANCODE_KP_ENTER: return OS_KEY_KP_ENTER;
    case SDL_SCANCODE_KP_EQUALS: return OS_KEY_KP_EQUALS;
    case SDL_SCANCODE_KP_1: return OS_KEY_KP_1;
    case SDL_SCANCODE_KP_2: return OS_KEY_KP_2;
    case SDL_SCANCODE_KP_3: return OS_KEY_KP_3;
    case SDL_SCANCODE_KP_4: return OS_KEY_KP_4;
    case SDL_SCANCODE_KP_5: return OS_KEY_KP_5;
    case SDL_SCANCODE_KP_6: return OS_KEY_KP_6;
    case SDL_SCANCODE_KP_7: return OS_KEY_KP_7;
    case SDL_SCANCODE_KP_8: return OS_KEY_KP_8;
    case SDL_SCANCODE_KP_9: return OS_KEY_KP_9;
    case SDL_SCANCODE_KP_0: return OS_KEY_KP_0;
    case SDL_SCANCODE_KP_PERIOD: return OS_KEY_KP_DECIMAL;
    case SDL_SCANCODE_LCTRL: return OS_KEY_LEFT_CONTROL;
    case SDL_SCANCODE_LSHIFT: return OS_KEY_LEFT_SHIFT;
    case SDL_SCANCODE_LALT: return OS_KEY_LEFT_ALT;
    case SDL_SCANCODE_LGUI: return OS_KEY_LEFT_OS;
    case SDL_SCANCODE_RCTRL: return OS_KEY_RIGHT_CONTROL;
    case SDL_SCANCODE_RSHIFT: return OS_KEY_RIGHT_SHIFT;
    case SDL_SCANCODE_RALT: return OS_KEY_RIGHT_ALT;
    case SDL_SCANCODE_RGUI: return OS_KEY_RIGHT_OS;
    default: return OS_KEY_COUNT;
  }
}

static uint32_t utf8_decode_codepoint(const char* s) {
  const unsigned char* p = (const unsigned char*)s;
  if (p[0] < 0x80) return p[0];
  if ((p[0] & 0xE0) == 0xC0) return ((p[0] & 0x1F) << 6) | (p[1] & 0x3F);
  if ((p[0] & 0xF0) == 0xE0) return ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
  if ((p[0] & 0xF8) == 0xF0) return ((p[0] & 0x07) << 18) | ((p[1] & 0x3F) << 12) | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F);
  return 0;
}

bool os_init(void) {
  return SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_AUDIO);
}

void os_destroy(void) {
  if (state.window) {
    SDL_DestroyWindow(state.window);
  }
  if (clipboard_text) {
    SDL_free(clipboard_text);
    clipboard_text = NULL;
  }
  SDL_Quit();
  memset(&state, 0, sizeof(state));
}

const char* os_get_name(void) {
  return "SDL3";
}

uint32_t os_get_core_count(void) {
  return (uint32_t) SDL_GetNumLogicalCPUCores();
}

void os_open_console(void) {
}

double os_get_time(void) {
  return SDL_GetTicksNS() / 1e9;
}

void os_sleep(double seconds) {
  SDL_DelayNS((Uint64)(seconds * 1e9));
}

void os_request_permission(os_permission permission) {
}

void* os_vm_init(size_t size) {
  return SDL_aligned_alloc(SDL_GetSIMDAlignment(), size);
}

bool os_vm_free(void* p, size_t size) {
  SDL_aligned_free(p);
  return true;
}

bool os_vm_commit(void* p, size_t size) {
  return true;
}

bool os_vm_release(void* p, size_t size) {
  return true;
}

bool os_window_open(const os_window_config* config) {
  if (state.window) return true;

  uint32_t flags = SDL_WINDOW_VULKAN;
  if (config->resizable) flags |= SDL_WINDOW_RESIZABLE;

  uint32_t width = config->width;
  uint32_t height = config->height;

  if (config->fullscreen) {
    flags |= SDL_WINDOW_FULLSCREEN;
    const SDL_DisplayMode* mode = SDL_GetDesktopDisplayMode(SDL_GetPrimaryDisplay());
    if (mode) {
      width = mode->w;
      height = mode->h;
    }
  }

  state.window = SDL_CreateWindow(config->title, width, height, flags);
  if (!state.window) return false;

  lovr_sdl_window = state.window;
  state.width = width;
  state.height = height;
  state.fullscreen = config->fullscreen;

  if (config->icon.data) {
    SDL_Surface* icon = SDL_CreateSurfaceFrom(
      config->icon.width, config->icon.height,
      SDL_PIXELFORMAT_RGBA32, config->icon.data,
      config->icon.width * 4
    );
    if (icon) {
      SDL_SetWindowIcon(state.window, icon);
      SDL_DestroySurface(icon);
    }
  }

  SDL_StartTextInput(state.window);

  return true;
}

bool os_window_is_open(void) {
  return state.window != NULL;
}

bool os_window_is_visible(void) {
  if (!state.window) return false;
  return !(SDL_GetWindowFlags(state.window) & SDL_WINDOW_HIDDEN);
}

bool os_window_is_focused(void) {
  if (!state.window) return false;
  return (SDL_GetWindowFlags(state.window) & SDL_WINDOW_INPUT_FOCUS) != 0;
}

bool os_window_is_fullscreen(void) {
  return state.fullscreen;
}

void os_window_set_fullscreen(bool fullscreen) {
  if (!state.window) return;
  state.fullscreen = fullscreen;
  if (fullscreen) {
    SDL_GetWindowPosition(state.window, (int*)&state.windowedX, (int*)&state.windowedY);
    SDL_GetWindowSize(state.window, (int*)&state.windowedW, (int*)&state.windowedH);
    SDL_SetWindowFullscreen(state.window, true);
  } else {
    SDL_SetWindowFullscreen(state.window, false);
    SDL_SetWindowSize(state.window, state.windowedW, state.windowedH);
    SDL_SetWindowPosition(state.window, state.windowedX, state.windowedY);
  }
}

void os_window_get_size(uint32_t* width, uint32_t* height) {
  int w, h;
  SDL_GetWindowSize(state.window, &w, &h);
  *width = (uint32_t) w;
  *height = (uint32_t) h;
}

float os_window_get_pixel_density(void) {
  if (!state.window) return 1.f;
  return SDL_GetWindowDisplayScale(state.window);
}

void os_window_message_box(const char* message) {
  SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "LÖVR", message, state.window);
}

void os_poll_events(void) {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    switch (event.type) {
      case SDL_EVENT_QUIT:
        if (state.onQuit) state.onQuit();
        break;

      case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        if (state.window && event.window.windowID == SDL_GetWindowID(state.window)) {
          if (state.onQuit) state.onQuit();
        }
        break;

      case SDL_EVENT_WINDOW_MINIMIZED:
        if (state.onVisible) state.onVisible(false);
        break;
      case SDL_EVENT_WINDOW_RESTORED:
        if (state.onVisible) state.onVisible(true);
        break;

      case SDL_EVENT_WINDOW_FOCUS_GAINED:
        if (state.onFocus) state.onFocus(true);
        break;
      case SDL_EVENT_WINDOW_FOCUS_LOST:
        if (state.onFocus) state.onFocus(false);
        break;

      case SDL_EVENT_WINDOW_RESIZED:
        if (state.onResize) {
          state.onResize((uint32_t)event.window.data1, (uint32_t)event.window.data2);
        }
        break;

      case SDL_EVENT_KEY_DOWN:
      case SDL_EVENT_KEY_UP: {
        if (!state.onKey) break;
        os_key key = convert_scancode(event.key.scancode);
        if (key == OS_KEY_COUNT) break;
        os_button_action action = event.key.down ? BUTTON_PRESSED : BUTTON_RELEASED;
        bool repeat = event.key.repeat;
        state.onKey(action, key, (uint32_t)event.key.scancode, repeat);
        break;
      }

      case SDL_EVENT_TEXT_INPUT: {
        if (!state.onText) break;
        uint32_t codepoint = utf8_decode_codepoint(event.text.text);
        if (codepoint) state.onText(codepoint);
        break;
      }

      case SDL_EVENT_MOUSE_BUTTON_DOWN:
      case SDL_EVENT_MOUSE_BUTTON_UP: {
        if (!state.onMouseButton) break;
        bool pressed = (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
        int button = (int)event.button.button - 1;
        state.onMouseButton(button, pressed);
        break;
      }

      case SDL_EVENT_MOUSE_MOTION: {
        if (!state.onMouseMove) break;
        double x = (double)event.motion.x;
        double y = (double)event.motion.y;
        state.mouseX = x;
        state.mouseY = y;
        state.onMouseMove(x, y);
        break;
      }

      case SDL_EVENT_MOUSE_WHEEL: {
        if (!state.onWheelMove) break;
        state.onWheelMove((double)event.wheel.x, (double)event.wheel.y);
        break;
      }

      default: break;
    }
  }
}

void os_on_quit(fn_quit* callback) { state.onQuit = callback; }
void os_on_visible(fn_visible* callback) { state.onVisible = callback; }
void os_on_focus(fn_focus* callback) { state.onFocus = callback; }
void os_on_resize(fn_resize* callback) { state.onResize = callback; }
void os_on_key(fn_key* callback) { state.onKey = callback; }
void os_on_text(fn_text* callback) { state.onText = callback; }
void os_on_mouse_button(fn_mouse_button* callback) { state.onMouseButton = callback; }
void os_on_mouse_move(fn_mouse_move* callback) { state.onMouseMove = callback; }
void os_on_mousewheel_move(fn_mousewheel_move* callback) { state.onWheelMove = callback; }
void os_on_permission(fn_permission* callback) { state.onPermission = callback; }

void os_get_mouse_position(double* x, double* y) {
  if (state.window) {
    float fx, fy;
    SDL_GetMouseState(&fx, &fy);
    *x = (double)fx;
    *y = (double)fy;
  } else {
    *x = *y = 0.;
  }
}

os_mouse_mode os_get_mouse_mode(void) {
  if (state.window && SDL_GetWindowRelativeMouseMode(state.window)) {
    return OS_MOUSE_RELATIVE;
  }
  return OS_MOUSE_NORMAL;
}

void os_set_mouse_mode(os_mouse_mode mode) {
  if (state.window) {
    SDL_SetWindowRelativeMouseMode(state.window, mode == OS_MOUSE_RELATIVE);
  }
}

const char* os_get_clipboard_text(void) {
  if (clipboard_text) {
    SDL_free(clipboard_text);
    clipboard_text = NULL;
  }
  clipboard_text = SDL_GetClipboardText();
  return clipboard_text ? clipboard_text : "";
}

void os_set_clipboard_text(const char* text) {
  SDL_SetClipboardText(text);
}

size_t os_get_home_directory(char* buffer, size_t size) {
  return copy_path(buffer, size, SDL_GetUserFolder(SDL_FOLDER_HOME));
}

size_t os_get_data_directory(char* buffer, size_t size) {
  char* path = SDL_GetPrefPath(NULL, "");
  if (!path) return 0;
  size_t len = strlen(path);
  if (len > 0 && (path[len - 1] == '/' || path[len - 1] == '\\')) {
    path[len - 1] = '\0';
    len--;
  }
  size_t result = copy_path(buffer, size, path);
  SDL_free(path);
  return result;
}

size_t os_get_working_directory(char* buffer, size_t size) {
  char* cwd = SDL_GetCurrentDirectory();
  if (!cwd) return 0;
  size_t len = strlen(cwd);
  if (len > 1 && (cwd[len - 1] == '/' || cwd[len - 1] == '\\')) {
    cwd[len - 1] = '\0';
    len--;
  }
  size_t result = copy_path(buffer, size, cwd);
  SDL_free(cwd);
  return result;
}

// SDL3 has no cross-platform "get executable path" API — SDL_GetBasePath
// returns the directory, not the file. Use platform APIs directly here.
// This is acceptable because release bundles use fused archives and don't
// rely on the executable path for bootstrapping.
size_t os_get_executable_path(char* buffer, size_t size) {
#ifdef _WIN32
  DWORD len = GetModuleFileNameA(NULL, buffer, (DWORD) size);
  return (len > 0 && len < size) ? (size_t) len : 0;
#elif defined(__APPLE__)
  uint32_t bufsize = (uint32_t) size;
  if (_NSGetExecutablePath(buffer, &bufsize) != 0) return 0;
  return strlen(buffer);
#else
  ssize_t len = readlink("/proc/self/exe", buffer, size - 1);
  if (len < 0) len = readlink("/proc/curproc/file", buffer, size - 1);
  if (len < 0) len = readlink("/proc/curproc/exe", buffer, size - 1);
  if (len < 0) len = readlink("/proc/self/path/a.out", buffer, size - 1);
  if (len >= 0) { buffer[len] = '\0'; return (size_t) len; }
  return 0;
#endif
}

size_t os_get_bundle_path(char* buffer, size_t size, const char** root) {
  *root = NULL;
  return os_get_executable_path(buffer, size);
}

void os_thread_attach(void) {}
void os_thread_detach(void) {}
void os_thread_set_name(const char* name) {}

uintptr_t os_get_win32_window(void) { return 0; }
uintptr_t os_get_win32_instance(void) { return 0; }
uintptr_t os_get_ca_metal_layer(void) { return 0; }
uintptr_t os_get_xcb_connection(void) { return 0; }
uintptr_t os_get_xcb_window(void) { return 0; }
