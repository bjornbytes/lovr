#include "osvr.h"

void initOSVR() {
  ctx = osvrClientInit("es.bjornbyt", 0);
}
