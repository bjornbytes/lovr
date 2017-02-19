#include "headset/headset.h"
#include "headset/vive.h"
#include "event/event.h"

void lovrControllerDestroy(const Ref* ref) {
  Controller* controller = containerof(ref, Controller);
  free(controller);
}
