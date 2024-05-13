#include "os.h"
#define WIN32_LEAN_AND_MEAN
#define WINVER 0x0600
#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <knownfolders.h>
#include <shlobj.h>
#include <stdio.h>

static __declspec(thread) HANDLE timer;
static __declspec(thread) bool createdTimer;

static struct {
  HINSTANCE instance;
  HCURSOR cursor;
  HWND window;
  uint32_t width;
  uint32_t height;
  fn_quit* onQuit;
  fn_focus* onFocus;
  fn_resize* onResize;
  fn_key* onKey;
  fn_text* onText;
  fn_mouse_button* onMouseButton;
  fn_mouse_move* onMouseMove;
  fn_mouse_move* onWheelMove;
  WCHAR highSurrogate;
  bool keys[OS_KEY_COUNT];
  bool buttons[2];
  uint8_t captureMask;
  double mouseX;
  double mouseY;
  bool focused;
  uint64_t frequency;
} state;

#ifndef LOVR_OMIT_MAIN
int main(int argc, char** argv);
int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev, LPSTR args, int show) {
  int argc;
  char** argv;

  LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &argc);
  if (!wargv) {
    return EXIT_FAILURE;
  }

  argv = calloc(argc + 1, sizeof(char*));
  if (!argv) {
    return EXIT_FAILURE;
  }

  for (int i = 0; i < argc; i++) {
    int size = WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, NULL, 0, NULL, NULL);

    argv[i] = malloc(size);
    if (!argv[i]) {
      return EXIT_FAILURE;
    }

    if (!WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, argv[i], size, NULL, NULL)) {
      return EXIT_FAILURE;
    }
  }

  int status = main(argc, argv);

  for (int i = 0; i < argc; i++) {
    free(argv[i]);
  }
  free(argv);

  return status;
}
#endif

bool os_init(void) {
  state.instance = GetModuleHandle(NULL);
  LARGE_INTEGER f;
  QueryPerformanceFrequency(&f);
  state.frequency = f.QuadPart;
  return true;
}

void os_destroy(void) {
  if (state.window) DestroyWindow(state.window);
  os_thread_detach();
  memset(&state, 0, sizeof(state));
}

const char* os_get_name(void) {
  return "Windows";
}

uint32_t os_get_core_count(void) {
  SYSTEM_INFO info;
  GetSystemInfo(&info);
  return info.dwNumberOfProcessors;
}

void os_open_console(void) {
  if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
    if (GetLastError() != ERROR_ACCESS_DENIED) {
      if (!AllocConsole()) {
        return;
      }
    }
  }

  freopen("CONOUT$", "w", stdout);
  freopen("CONIN$", "r", stdin);
  freopen("CONOUT$", "w", stderr);
}

double os_get_time(void) {
  LARGE_INTEGER t;
  QueryPerformanceCounter(&t);
  return t.QuadPart / (double) state.frequency;
}

void os_sleep(double seconds) {
#ifdef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
  if (!createdTimer) {
    createdTimer = true;
    timer = CreateWaitableTimerExW(NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
  }

  if (timer && seconds > 0.) {
    LARGE_INTEGER due;
    due.QuadPart = -((LONGLONG) (seconds * 1e7));
    if (SetWaitableTimer(timer, &due, 0, NULL, NULL, false)) {
      WaitForSingleObject(timer, INFINITE);
      return;
    }
  }
#endif
  Sleep((unsigned int) (seconds * 1000));
}

void os_request_permission(os_permission permission) {
  //
}

const char* os_get_clipboard_text(void) {
  return NULL; // TODO
}

void os_set_clipboard_text(const char* text) {
  // TODO
}

void* os_vm_init(size_t size) {
  return VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_NOACCESS);
}

bool os_vm_free(void* p, size_t size) {
  return VirtualFree(p, 0, MEM_RELEASE);
}

bool os_vm_commit(void* p, size_t size) {
  return VirtualAlloc(p, size, MEM_COMMIT, PAGE_READWRITE);
}

bool os_vm_release(void* p, size_t size) {
  return VirtualFree(p, 0, MEM_DECOMMIT);
}

void os_thread_attach(void) {
  //
}

void os_thread_detach(void) {
  if (createdTimer) {
    CloseHandle(timer);
    createdTimer = false;
  }
}

static os_key convertKey(uint16_t scancode) {
  switch (scancode) {
    case 0x01E: return OS_KEY_A;
    case 0x030: return OS_KEY_B;
    case 0x02E: return OS_KEY_C;
    case 0x020: return OS_KEY_D;
    case 0x012: return OS_KEY_E;
    case 0x021: return OS_KEY_F;
    case 0x022: return OS_KEY_G;
    case 0x023: return OS_KEY_H;
    case 0x017: return OS_KEY_I;
    case 0x024: return OS_KEY_J;
    case 0x025: return OS_KEY_K;
    case 0x026: return OS_KEY_L;
    case 0x032: return OS_KEY_M;
    case 0x031: return OS_KEY_N;
    case 0x018: return OS_KEY_O;
    case 0x019: return OS_KEY_P;
    case 0x010: return OS_KEY_Q;
    case 0x013: return OS_KEY_R;
    case 0x01F: return OS_KEY_S;
    case 0x014: return OS_KEY_T;
    case 0x016: return OS_KEY_U;
    case 0x02F: return OS_KEY_V;
    case 0x011: return OS_KEY_W;
    case 0x02D: return OS_KEY_X;
    case 0x015: return OS_KEY_Y;
    case 0x02C: return OS_KEY_Z;
    case 0x002: return OS_KEY_1;
    case 0x003: return OS_KEY_2;
    case 0x004: return OS_KEY_3;
    case 0x005: return OS_KEY_4;
    case 0x006: return OS_KEY_5;
    case 0x007: return OS_KEY_6;
    case 0x008: return OS_KEY_7;
    case 0x009: return OS_KEY_8;
    case 0x00A: return OS_KEY_9;
    case 0x00B: return OS_KEY_0;
    case 0x039: return OS_KEY_SPACE;
    case 0x01C: return OS_KEY_ENTER;
    case 0x00F: return OS_KEY_TAB;
    case 0x001: return OS_KEY_ESCAPE;
    case 0x00E: return OS_KEY_BACKSPACE;
    case 0x148: return OS_KEY_UP;
    case 0x150: return OS_KEY_DOWN;
    case 0x14B: return OS_KEY_LEFT;
    case 0x14D: return OS_KEY_RIGHT;
    case 0x147: return OS_KEY_HOME;
    case 0x14F: return OS_KEY_END;
    case 0x149: return OS_KEY_PAGE_UP;
    case 0x151: return OS_KEY_PAGE_DOWN;
    case 0x152: return OS_KEY_INSERT;
    case 0x153: return OS_KEY_DELETE;
    case 0x03B: return OS_KEY_F1;
    case 0x03C: return OS_KEY_F2;
    case 0x03D: return OS_KEY_F3;
    case 0x03E: return OS_KEY_F4;
    case 0x03F: return OS_KEY_F5;
    case 0x040: return OS_KEY_F6;
    case 0x041: return OS_KEY_F7;
    case 0x042: return OS_KEY_F8;
    case 0x043: return OS_KEY_F9;
    case 0x044: return OS_KEY_F10;
    case 0x057: return OS_KEY_F11;
    case 0x058: return OS_KEY_F12;
    case 0x029: return OS_KEY_BACKTICK;
    case 0x00C: return OS_KEY_MINUS;
    case 0x00D: return OS_KEY_EQUALS;
    case 0x01A: return OS_KEY_LEFT_BRACKET;
    case 0x01B: return OS_KEY_RIGHT_BRACKET;
    case 0x02B: return OS_KEY_BACKSLASH;
    case 0x027: return OS_KEY_SEMICOLON;
    case 0x028: return OS_KEY_APOSTROPHE;
    case 0x033: return OS_KEY_COMMA;
    case 0x034: return OS_KEY_PERIOD;
    case 0x035: return OS_KEY_SLASH;
    case 0x052: return OS_KEY_KP_0;
    case 0x04F: return OS_KEY_KP_1;
    case 0x050: return OS_KEY_KP_2;
    case 0x051: return OS_KEY_KP_3;
    case 0x04B: return OS_KEY_KP_4;
    case 0x04C: return OS_KEY_KP_5;
    case 0x04D: return OS_KEY_KP_6;
    case 0x047: return OS_KEY_KP_7;
    case 0x048: return OS_KEY_KP_8;
    case 0x049: return OS_KEY_KP_9;
    case 0x053: return OS_KEY_KP_DECIMAL;
    case 0x135: return OS_KEY_KP_DIVIDE;
    case 0x037: return OS_KEY_KP_MULTIPLY;
    case 0x04A: return OS_KEY_KP_SUBTRACT;
    case 0x04E: return OS_KEY_KP_ADD;
    case 0x11C: return OS_KEY_KP_ENTER;
    case 0x059: return OS_KEY_KP_EQUALS;
    case 0x01D: return OS_KEY_LEFT_CONTROL;
    case 0x02A: return OS_KEY_LEFT_SHIFT;
    case 0x038: return OS_KEY_LEFT_ALT;
    case 0x15B: return OS_KEY_LEFT_OS;
    case 0x11D: return OS_KEY_RIGHT_CONTROL;
    case 0x036: return OS_KEY_RIGHT_SHIFT;
    case 0x138: return OS_KEY_RIGHT_ALT;
    case 0x15C: return OS_KEY_RIGHT_OS;
    case 0x03A: return OS_KEY_CAPS_LOCK;
    case 0x046: return OS_KEY_SCROLL_LOCK;
    case 0x145: return OS_KEY_NUM_LOCK;
    default: return OS_KEY_COUNT;
  }
}

static void mousePressed(uint8_t button) {
  if (state.captureMask == 0) SetCapture(state.window);
  state.captureMask |= (1 << button);

  state.buttons[button] = true;

  if (state.onMouseButton) {
    state.onMouseButton(button, true);
  }
}

static void mouseReleased(uint8_t button) {
  state.captureMask &= ~(1 << button);
  if (state.captureMask == 0) ReleaseCapture();

  state.buttons[button] = false;

  if (state.onMouseButton) {
    state.onMouseButton(button, false);
  }
}

static LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM param, LPARAM lparam) {
  switch (message) {
    case WM_CLOSE:
      if (state.onQuit) state.onQuit();
      break;
    case WM_SETFOCUS:
      if (state.onFocus) state.onFocus(true);
      state.focused = true;
      break;
    case WM_KILLFOCUS:
      if (state.onFocus) state.onFocus(false);
      state.focused = false;
      break;
    case WM_EXITSIZEMOVE: {
      RECT rect;
      GetClientRect(state.window, &rect);
      if (state.width != rect.right || state.height != rect.bottom) {
        state.width = rect.right;
        state.height = rect.bottom;
        if (state.onResize) {
          state.onResize(state.width, state.height);
        }
      }
      break;
    }
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP: {
      uint16_t scancode = HIWORD(lparam) & 0x1FF;
      os_key key = convertKey(scancode);

      if (key != OS_KEY_COUNT) {
        bool pressed = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
        os_button_action action = pressed ? BUTTON_PRESSED : BUTTON_RELEASED;
        bool repeat = !!(HIWORD(lparam) & KF_REPEAT);

        state.keys[key] = pressed;
        if (state.onKey) state.onKey(action, key, scancode, repeat);
      }
      break;
    }
    case WM_CHAR:
      if (param >= 0xD800 && param <= 0xDBFF) {
        state.highSurrogate = (WCHAR) param;
      } else {
        uint32_t codepoint = param;
        if (param >= 0xDC00 && param <= 0xDFFF && state.highSurrogate != 0) {
          codepoint = ((state.highSurrogate - 0xD800) << 10) + (param - 0xDC00) + 0x10000;
          state.highSurrogate = 0;
        }
        if (state.onText && codepoint >= 32) state.onText(codepoint);
      }
      break;
    case WM_SETCURSOR:
      if (LOWORD(lparam) == HTCLIENT) {
        SetCursor(state.cursor);
        return TRUE;
      }
      break;
    case WM_CAPTURECHANGED: state.captureMask = 0; break;
    case WM_LBUTTONDOWN: mousePressed(0); break;
    case WM_RBUTTONDOWN: mousePressed(1); break;
    case WM_MBUTTONDOWN: mousePressed(2); break;
    case WM_LBUTTONUP: mouseReleased(0); break;
    case WM_RBUTTONUP: mouseReleased(1); break;
    case WM_MBUTTONUP: mouseReleased(2); break;
    case WM_MOUSEMOVE: {
      double x = GET_X_LPARAM(lparam);
      double y = GET_Y_LPARAM(lparam);
      if (x != state.mouseX || y != state.mouseY) {
        if (state.onMouseMove) state.onMouseMove(x, y);
        state.mouseX = x;
        state.mouseY = y;
      }
      break;
    }
    case WM_MOUSEWHEEL:
      if (state.onWheelMove) state.onWheelMove(0., (double) (SHORT) HIWORD(param) / WHEEL_DELTA);
      break;
    case WM_MOUSEHWHEEL:
      if (state.onWheelMove) state.onWheelMove(-(double) (SHORT) HIWORD(param) / WHEEL_DELTA, 0.);
      break;
  }

  return DefWindowProcW(window, message, param, lparam);
}

void os_poll_events() {
  MSG message;
  while (PeekMessage(&message, NULL, 0, 0, PM_REMOVE)) {
    TranslateMessage(&message);
    DispatchMessage(&message);
  }
}

void os_on_quit(fn_quit* callback) {
  state.onQuit = callback;
}

void os_on_focus(fn_focus* callback) {
  state.onFocus = callback;
}

void os_on_resize(fn_resize* callback) {
  state.onResize = callback;
}

void os_on_key(fn_key* callback) {
  state.onKey = callback;
}

void os_on_text(fn_text* callback) {
  state.onText = callback;
}

void os_on_mouse_button(fn_mouse_button* callback) {
  state.onMouseButton = callback;
}

void os_on_mouse_move(fn_mouse_move* callback) {
  state.onMouseMove = callback;
}

void os_on_mousewheel_move(fn_mousewheel_move* callback) {
  state.onWheelMove = callback;
}

void os_on_permission(fn_permission* callback) {
  //
}

bool os_window_open(const os_window_config* config) {
  if (state.window) {
    return true;
  }

  WNDCLASSW wc = {
    .lpfnWndProc = windowProc,
    .hInstance = state.instance,
    .lpszClassName = L"LOVR"
  };

  if (!RegisterClassW(&wc)) {
    return false;
  }

  WCHAR wtitle[256];
  if (!MultiByteToWideChar(CP_UTF8, 0, config->title, -1, wtitle, sizeof(wtitle) / sizeof(wtitle[0]))) {
    return false;
  }

  DWORD style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;

  if (!config->resizable) {
    style &= ~WS_THICKFRAME;
  }

  RECT rect = { 0, 0, config->width, config->height };
  AdjustWindowRect(&rect, style, FALSE);

  state.window = CreateWindowW(wc.lpszClassName, wtitle, style,
      CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top,
      NULL, NULL, state.instance, NULL);


  state.width = config->width;
  state.height = config->height;
  state.cursor = LoadCursor(NULL, IDC_ARROW);

  return !!state.window;
}

bool os_window_is_open() {
  return state.window;
}

void os_window_get_size(uint32_t* width, uint32_t* height) {
  *width = state.width;
  *height = state.height;
}

float os_window_get_pixel_density(void) {
  return 1.f;
}

void os_window_message_box(const char* message) {
  MessageBox((HANDLE) os_get_win32_window(), message, NULL, 0);
}

void os_get_mouse_position(double* x, double* y) {
  POINT point;
  GetCursorPos(&point);
  ScreenToClient(state.window, &point);
  *x = point.x;
  *y = point.y;
}

void os_set_mouse_mode(os_mouse_mode mode) {
  if (mode == MOUSE_MODE_NORMAL) {
    //SetCursor(state.cursor);
    ClipCursor(NULL);
  } else {
    RECT clip;
    GetClientRect(state.window, &clip);
    ClientToScreen(state.window, (POINT*) &clip.left);
    ClientToScreen(state.window, (POINT*) &clip.right);
    //SetCursor(NULL);
    ClipCursor(&clip);
  }
}

bool os_is_mouse_down(os_mouse_button button) {
  return state.buttons[button];
}

bool os_is_key_down(os_key key) {
  return false;
}

size_t os_get_home_directory(char* buffer, size_t size) {
  PWSTR wpath = NULL;
  if (SHGetKnownFolderPath(&FOLDERID_Profile, 0, NULL, &wpath) == S_OK) {
    size_t bytes = WideCharToMultiByte(CP_UTF8, 0, wpath, -1, buffer, (int) size, NULL, NULL) - 1;
    CoTaskMemFree(wpath);
    return bytes;
  }
  return 0;
}

size_t os_get_data_directory(char* buffer, size_t size) {
  PWSTR wpath = NULL;
  if (SHGetKnownFolderPath(&FOLDERID_RoamingAppData, 0, NULL, &wpath) == S_OK) {
    size_t bytes = WideCharToMultiByte(CP_UTF8, 0, wpath, -1, buffer, (int) size, NULL, NULL) - 1;
    CoTaskMemFree(wpath);
    return bytes;
  }
  return 0;
}

size_t os_get_working_directory(char* buffer, size_t size) {
  WCHAR wpath[1024];
  int length = GetCurrentDirectoryW((int) size, wpath);
  if (length) {
    return WideCharToMultiByte(CP_UTF8, 0, wpath, length + 1, buffer, (int) size, NULL, NULL) - 1;
  }
  return 0;
}

size_t os_get_executable_path(char* buffer, size_t size) {
  WCHAR wpath[1024];
  DWORD length = GetModuleFileNameW(NULL, wpath, 1024);
  if (length < 1024) {
    return WideCharToMultiByte(CP_UTF8, 0, wpath, length + 1, buffer, (int) size, NULL, NULL) - 1;
  }
  return 0;
}

size_t os_get_bundle_path(char* buffer, size_t size, const char** root) {
  *root = NULL;
  return os_get_executable_path(buffer, size);
}

uintptr_t os_get_win32_instance(void) {
  return (uintptr_t) state.instance;
}

uintptr_t os_get_win32_window(void) {
  return (uintptr_t) state.window;
}
