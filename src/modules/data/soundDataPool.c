#include "data/soundDataPool.h"
#include "core/ref.h"
#include "core/util.h"
#include <string.h>

SoundDataPool* lovrSoundDataPoolInit(SoundDataPool* pool, size_t samples, uint32_t sampleRate, uint32_t bitDepth, uint32_t channels) {
  arr_init(&pool->available);
  arr_init(&pool->used);
  pool->samples = samples;
  pool->sampleRate = sampleRate;
  pool->bitDepth = bitDepth;
  pool->channels = channels;
  return pool;
}

static bool markForReuse(void* o, void* ref) {
  SoundDataPool* pool = (SoundDataPool*)ref;
  SoundData* sd = o;
  for (size_t i = 0; i < pool->used.length; i++) {
    if (pool->used.data[i] == sd) {
      arr_splice(&pool->used, i, 1);
      break;
    }
  }
  arr_push(&pool->available, sd);
  return false;
}

SoundData* lovrSoundDataPoolCreateSoundData(SoundDataPool* pool) {
  if (pool->available.length > 0) {
    SoundData *sd = arr_pop(&pool->available);
    arr_push(&pool->used, sd);
    lovrRetain(sd);
    return sd;
  } else {
    SoundData* sd = lovrSoundDataCreate(pool->samples, pool->sampleRate, pool->bitDepth, pool->channels);
    RefHeader* h = toRefHeader(sd);
    h->destructor = markForReuse;
    h->destructorExtra = pool;
    arr_push(&pool->used, sd);
    return sd;
  }
}

void lovrSoundDataPoolDestroy(void* ref) {
  SoundDataPool* pool = (SoundDataPool*)ref;
  // these are unused anywhere and can just be deleted
  for (size_t i = 0; i < pool->available.length; i++) {
    SoundData* sd = pool->used.data[i];
    RefHeader* h = toRefHeader(sd);
    lovrSoundDataDestroy(pool->available.data[i]);
    free(toRefHeader(h));
  }
  // these are in use and we just have to make sure they aren't reused
  for (size_t i = 0; i < pool->used.length; i++) {
    RefHeader* h = toRefHeader(pool->used.data[i]);
    h->destructor = NULL;
    h->destructorExtra = NULL;
  }
  arr_free(&pool->available);
  arr_free(&pool->used);
}
