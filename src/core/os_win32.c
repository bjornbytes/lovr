#include "os.h"
#define WIN32_LEAN_AND_MEAN
#define WINVER 0x0600
#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <shellapi.h>
#include <knownfolders.h>
#include <shlobj.h>
#include <stdio.h>

static struct {
  HINSTANCE instance;
  HWND window;
  fn_quit* onQuit;
  fn_focus* onFocus;
  fn_key* onKey;
  bool keys[KEY_COUNT];
  bool buttons[2];
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
  state.instance = GetModuleHandle(NULL);;
  LARGE_INTEGER f;
  QueryPerformanceFrequency(&f);
  state.frequency = f.QuadPart;
  return true;
}

void os_destroy(void) {
  if (state.window) DestroyWindow(state.window);
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
  Sleep((unsigned int) (seconds * 1000));
}

void os_request_permission(os_permission permission) {
  //
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
  //
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
    case WM_KEYDOWN:
    case WM_KEYUP: {
      os_key key = KEY_COUNT;
      switch (param) {
        case 'A': key = KEY_A; break;
        case 'B': key = KEY_B; break;
        case 'C': key = KEY_C; break;
        case 'D': key = KEY_D; break;
        case 'E': key = KEY_E; break;
        case 'F': key = KEY_F; break;
        case 'G': key = KEY_G; break;
        case 'H': key = KEY_H; break;
        case 'I': key = KEY_I; break;
        case 'J': key = KEY_J; break;
        case 'K': key = KEY_K; break;
        case 'L': key = KEY_L; break;
        case 'M': key = KEY_M; break;
        case 'N': key = KEY_N; break;
        case 'O': key = KEY_O; break;
        case 'P': key = KEY_P; break;
        case 'Q': key = KEY_Q; break;
        case 'R': key = KEY_R; break;
        case 'S': key = KEY_S; break;
        case 'T': key = KEY_T; break;
        case 'U': key = KEY_U; break;
        case 'V': key = KEY_V; break;
        case 'W': key = KEY_W; break;
        case 'X': key = KEY_X; break;
        case 'Y': key = KEY_Y; break;
        case 'Z': key = KEY_Z; break;
        case '0': key = KEY_0; break;
        case '1': key = KEY_1; break;
        case '2': key = KEY_2; break;
        case '3': key = KEY_3; break;
        case '4': key = KEY_4; break;
        case '5': key = KEY_5; break;
        case '6': key = KEY_6; break;
        case '7': key = KEY_7; break;
        case '8': key = KEY_8; break;
        case '9': key = KEY_9; break;
        default: break;
      }
      if (key != KEY_COUNT) {
        bool pressed = message == WM_KEYDOWN;

        if (state.onKey) {
          bool repeat = pressed && state.keys[key];
          state.onKey(pressed ? BUTTON_PRESSED : BUTTON_RELEASED, key, 0, repeat);
        }

        state.keys[key] = pressed;
      }
      break;
    }
    case WM_LBUTTONDOWN:
      state.buttons[MOUSE_LEFT] = true;
      break;
    case WM_RBUTTONDOWN:
      state.buttons[MOUSE_RIGHT] = true;
      break;
    case WM_LBUTTONUP:
      state.buttons[MOUSE_LEFT] = false;
      break;
    case WM_RBUTTONUP:
      state.buttons[MOUSE_RIGHT] = false;
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
  //
}

void os_on_key(fn_key* callback) {
  state.onKey = callback;
}

void os_on_text(fn_text* callback) {
  //
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

  state.window = CreateWindowW(wc.lpszClassName, wtitle, style,
      CW_USEDEFAULT, CW_USEDEFAULT, config->width, config->height,
      NULL, NULL, state.instance, NULL);

  return !!state.window;
}

bool os_window_is_open() {
  return state.window;
}

void os_window_get_size(uint32_t* width, uint32_t* height) {
  if (state.window) {
    RECT rect;
    GetClientRect(state.window, &rect);
    *width = rect.right;
    *height = rect.bottom;
  } else {
    *width = 0;
    *height = 0;
  }
}

// FIXME
void os_window_get_fbsize(uint32_t* width, uint32_t* height) {
  if (state.window) {
    RECT rect;
    GetClientRect(state.window, &rect);
    *width = rect.right;
    *height = rect.bottom;
  } else {
    *width = 0;
    *height = 0;
  }
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
    SetCursor(LoadCursorW(NULL, IDC_ARROW));
    ClipCursor(NULL);
  } else {
    RECT clip;
    GetClientRect(state.window, &clip);
    ClientToScreen(state.window, (POINT*) &clip.left);
    ClientToScreen(state.window, (POINT*) &clip.right);
    SetCursor(NULL);
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

uintptr_t os_get_win32_instance() {
  return (uintptr_t) state.instance;
}

uintptr_t os_get_win32_window() {
  return (uintptr_t) state.window;
}
