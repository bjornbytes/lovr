#include <stdbool.h>
#include <stdint.h>

// Note: Channels retrieved with lovrThreadGetChannel don't need to be released.  They're just all
// cleaned up when the thread module is destroyed.

#pragma once

#define MAX_THREAD_ARGUMENTS 4

struct Blob;
struct Variant;

typedef struct Thread Thread;
typedef struct Channel Channel;

bool lovrThreadModuleInit(void);
void lovrThreadModuleDestroy(void);
struct Channel* lovrThreadGetChannel(const char* name);

// Thread

typedef char* ThreadFunction(Thread* thread, struct Blob* body, struct Variant* arguments, uint32_t argumentCount);

Thread* lovrThreadCreate(ThreadFunction* function, struct Blob* body);
void lovrThreadDestroy(void* ref);
void lovrThreadStart(Thread* thread, struct Variant* arguments, uint32_t argumentCount);
void lovrThreadWait(Thread* thread);
bool lovrThreadIsRunning(Thread* thread);
const char* lovrThreadGetError(Thread* thread);

// Channel

Channel* lovrChannelCreate(uint64_t hash);
void lovrChannelDestroy(void* ref);
bool lovrChannelPush(Channel* channel, struct Variant* variant, double timeout, uint64_t* id);
bool lovrChannelPop(Channel* channel, struct Variant* variant, double timeout);
bool lovrChannelPeek(Channel* channel, struct Variant* variant);
void lovrChannelClear(Channel* channel);
uint64_t lovrChannelGetCount(Channel* channel);
bool lovrChannelHasRead(Channel* channel, uint64_t id);
