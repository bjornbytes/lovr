#include "osvr.h"

void initOsvr() {
  ctx = osvrClientInit("es.bjornbyt", 0);
}
