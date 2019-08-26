#include "filesystem/filesystem.h"
#include "core/fs.h"

typedef enum {
  ARCHIVE_DIR,
  ARCHIVE_ZIP
} ArchiveType;

typedef struct Archive {
  struct Archive* next;
  ArchiveType type;
  const char* path;
} Archive;

static struct {
  bool initialized;
  Archive* head;
} state;

bool lovrFilesystemInit(const char* argExe, const char* argGame, const char* argRoot) {
  if (state.initialized) return false;
  state.initialized = true;
  return true;
}

void lovrFilesystemDestroy() {
  if (!state.initialized) return;
  memset(&state, 0, sizeof(state));
}

bool lovrFilesystemMount(const char* path, const char* mountpoint, bool append, const char* root) {
  Archive* archive = malloc(sizeof(Archive));
  archive->next = state.head;
  state.head = archive;
}
