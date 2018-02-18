#include "thread/channel.h"

Channel* lovrChannelCreate() {
  Channel* channel = lovrAlloc(sizeof(Channel), lovrChannelDestroy);
  if (!channel) return NULL;

  return channel;
}

void lovrChannelDestroy(const Ref* ref) {
  Channel* channel = containerof(ref, Channel);
  free(channel);
}
