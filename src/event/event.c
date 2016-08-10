#include "event.h"
#include "../lovr.h"
#include "../glfw.h"
#include "../osvr.h"

void lovrEventPoll() {
  glfwPollEvents();

  if (osvrClientCheckStatus(ctx) != OSVR_RETURN_FAILURE) {
    osvrClientUpdate(ctx);
  }
}

void lovrEventQuit() {
  lovrDestroy();
}
