#include "event.h"
#include "../lovr.h"
#include "../glfw.h"
#include "../osvr.h"

extern OSVR_ClientContext ctx;

void lovrEventPoll() {
  glfwPollEvents();

  if (ctx != NULL && osvrClientCheckStatus(ctx) != OSVR_RETURN_FAILURE) {
    osvrClientUpdate(ctx);
  }
}

void lovrEventQuit() {
  lovrDestroy();
}
