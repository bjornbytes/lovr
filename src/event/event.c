#include "event.h"
#include "../lovr.h"
#include "../glfw.h"

void lovrEventPoll() {
  glfwPollEvents();
}

void lovrEventQuit() {
  lovrDestroy();
}
