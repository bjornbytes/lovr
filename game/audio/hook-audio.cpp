extern "C" {
#include "api/api.h"
#include "audio/audio.h"
#include "core/util.h"
#include "data/sound.h"
#include "event/event.h"
#include "data/blob.h"
#include "thread/thread.h"
#include "thread/channel.h"
#include <lua.h>
#include <lauxlib.h>
}
#include <string>
#include "lib/tinycthread/tinycthread.h"

// State of a thread-based Sound
struct SoundCallbackData {
	bool dead;                 // An audio Source will never call a dead Sound, but a user calling Sound methods manually might
	Thread *thread;
	lua_State *L;
	Blob *blob;                // Attempt to reuse blob storage between frames // FIXME use Sound instead of Blob?
	size_t blobDataTrueCount;  // Count in frames [floats]
};

extern "C" {
lua_State *threadSetup(Thread *thread);
void threadError(Thread *thread, lua_State*L);

// FIXME: Why does this exist separately from threadError?
int CrashAndReturnEmpty(SoundCallbackData *soundCallbackData, std::string err) {
	err = "Audio render thread: " + err;
	Thread *thread = soundCallbackData->thread;
	lua_State *L = soundCallbackData->L;

	lua_pushstring(L, err.c_str());
	threadError(thread, L);
	soundCallbackData->dead = true;
	return 0;
}

static const char *audioBlobName = "Audio thread output";

uint32_t readCallback(Sound* sound, uint32_t offset, uint32_t frameCount, void* data) {
	SoundCallbackData *soundCallbackData = (SoundCallbackData *)lovrSoundGetCallbackMemo(sound);

	if (soundCallbackData->dead) return 0;

	// We can't predict frame size until we start getting requests, so we wait to allocate the blob storage until then
	if (frameCount > soundCallbackData->blobDataTrueCount) { // Every time request size increases, re-allocate
		lovrRelease(soundCallbackData->blob, lovrBlobDestroy);
		soundCallbackData->blob = lovrBlobCreate(malloc(frameCount*sizeof(float)), frameCount*sizeof(float), audioBlobName);
		soundCallbackData->blobDataTrueCount = frameCount;
	}
	void *blobDataWas = soundCallbackData->blob->data; // Track this so we can detect if the blob changed during thread run
	soundCallbackData->blob->size = frameCount*sizeof(float); // Allow the blob to under-report its size. This is safe, but violates Bjorn's rules

	Blob *resultBlob = NULL; // Blob returned from thread
	bool resultNil = false;  // Did blob return nil?
	int blobProgress = 0; // Used to track how close we got to success so we can display a useful error message
	lua_State *L = soundCallbackData->L;
	lua_getglobal(L, "lovr"); // We will execute the function lovr.audio()
	if (!lua_isnoneornil(L, -1)) {
		blobProgress++;
		lua_getfield(L, -1, "audio");
		if (!lua_isnoneornil(L, -1)) {
			blobProgress++;
			Variant variant; variant.type = TYPE_OBJECT; // Wrap blob in variant
			variant.value.object.pointer = soundCallbackData->blob;
			variant.value.object.type = "Blob";
			variant.value.object.destructor = lovrBlobDestroy;

			luax_pushvariant(L, &variant);
			int result = lua_pcall (L, 1, 1, 0);

			if (result) {
				// Failure
				threadError(soundCallbackData->thread, L);
				soundCallbackData->dead = true;
			} else {
				blobProgress++;
				resultNil = lua_isnil(L, -1);
				if (!resultNil) {
					resultBlob = luax_totype(L, -1, Blob);
					lovrRetain(resultBlob);
				}
				lua_settop(L, 0);
			}
		}
	}
	if (resultNil)
		return 0; // No error, code simply indicated it is done returning audio
	if (!resultBlob) { // Blob was never set, but this is not because nil was returned. Some error occurred.
		switch (blobProgress) {
			case 0:
				return CrashAndReturnEmpty(soundCallbackData, "No lovr object in audio thread global scope");
			case 1:
				return CrashAndReturnEmpty(soundCallbackData, "No lovr.audio in audio thread");
			case 2: // ThreadError case
				return 0;
			default:
				return CrashAndReturnEmpty(soundCallbackData, "lovr.audio returned wrong type");
		}
	}
	int resultCount = std::min<int>(resultBlob->size/sizeof(float), frameCount);
	memcpy(data, resultBlob->data, resultCount*sizeof(float));
	if (resultBlob != soundCallbackData->blob) {
		lovrRelease(soundCallbackData->blob, lovrBlobDestroy);
		soundCallbackData->blob = resultBlob;
	}
	if (blobDataWas != soundCallbackData->blob->data)
		soundCallbackData->blobDataTrueCount = resultCount;

	return resultCount;
}

static void destroyCallback(Sound* sound) {
  SoundCallbackData *soundCallbackData = (SoundCallbackData *)lovrSoundGetCallbackMemo(sound);
  lovrRelease(soundCallbackData->thread, lovrThreadDestroy);
  if (soundCallbackData->L)
  	lua_close(soundCallbackData->L);
  lovrRelease(soundCallbackData->blob, lovrBlobDestroy);
  free(soundCallbackData);
}

static int l_audioNewThreadSound(lua_State *L) {
  Thread* thread = luax_checktype(L, 1, Thread);
  lovrAssert(!lovrThreadIsRunning(thread), "Thread for audio is already started");

  thread->argumentCount = MIN(MAX_THREAD_ARGUMENTS, lua_gettop(L) - 1);
  for (size_t i = 0; i < thread->argumentCount; i++) {
    luax_checkvariant(L, 2 + i, &thread->arguments[i]);
  }

  SoundCallbackData *soundCallbackData = (SoundCallbackData *)calloc(1, sizeof(SoundCallbackData));	
  lovrRetain(thread);
  soundCallbackData->thread = thread;
  soundCallbackData->L = threadSetup(thread); // FIXME: Should do this on audio thread?
  if (!soundCallbackData->L) {
  	soundCallbackData->dead = true;
  }

  Sound *sound = lovrSoundCreateFromCallback(readCallback, soundCallbackData, destroyCallback, SAMPLE_F32, SAMPLE_RATE, CHANNEL_MONO, LOVR_SOUND_ENDLESS);
  luax_pushtype(L, Sound, sound);
  lovrRelease(sound, lovrSoundDestroy);

  return 1;
}

static int l_blobCopy(lua_State *L) {
  Blob* a = luax_checktype(L, 1, Blob);
  Blob* b = luax_checktype(L, 2, Blob);
  int len = MIN(a->size, b->size);
  memcpy(a->data, b->data, len);
  return 0;
}

const luaL_Reg audioLua[] = {
  { "newThreadSound", l_audioNewThreadSound },

  { "blobCopy", l_blobCopy },
  { NULL, NULL }
};

// Register Lua module-- not the same as gameInitFUnc
int luaopen_ext_audio(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, audioLua);

  return 1;
}

}