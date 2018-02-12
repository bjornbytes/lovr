#include "thread/thread.h"

Thread* lovrThreadCreate() {
  Thread* thread = lovrAlloc(sizeof(Thread), lovrThreadDestroy);
  if (!thread) return NULL;

  return thread;
}

void lovrThreadDestroy(const Ref* ref) {
  Thread* thread = containerof(ref, Thread);
  free(thread);
}
