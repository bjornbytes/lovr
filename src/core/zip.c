#include "zip.h"

#define ZIP_HEADER_SIZE 22
#define ZIP_ENTRY_SIZE 46
#define ZIP_HEADER_MAGIC 0x06054b50
#define ZIP_ENTRY_MAGIC 0x02014b50

typedef struct {
  uint32_t magic;
  uint16_t disk;
  uint16_t startDisk;
  uint16_t localEntryCount;
  uint16_t totalEntryCount;
  uint32_t centralDirectorySize;
  uint32_t centralDirectoryOffset;
  uint16_t commentLength;
} zipHeader;

typedef struct {
  uint32_t magic;
  uint16_t version;
  uint16_t minimumVersion;
  uint16_t flags;
  uint16_t compression;
  uint16_t lastModifiedTime;
  uint16_t lastModifiedDate;
  uint32_t crc32;
  uint32_t compressedSize;
  uint32_t size;
  uint16_t nameLength;
  uint16_t extraLength;
  uint16_t commentLength;
  uint16_t disk;
  uint16_t internalAttributes;
  uint16_t externalAttributes;
  uint32_t offset;
} zipEntry;

bool zip_open(zip_t* zip, const char* path) {
  if (!fs_open(path, OPEN_READ, &zip->file)) {
    return false;
  }

  // Seek to the end of the file, minus the header size
  if (!fs_seek(zip->file, -ZIP_HEADER_SIZE, FROM_END)) {
    fs_close(zip->file);
    return false;
  }

  // Read the header
  size_t size = ZIP_HEADER_SIZE;
  uint8_t headerData[ZIP_HEADER_SIZE];
  if (!fs_read(zip->file, headerData, &size) || size != sizeof(headerData)) {
    fs_close(zip->file);
    return false;
  }

  // Basic header validation
  zipHeader* header = (zipHeader*) headerData;
  if (header->magic != ZIP_HEADER_MAGIC || header->disk != header->startDisk || header->localEntryCount != header->totalEntryCount) {
    fs_close(zip->file);
    return false;
  }

  // Seek to the entry list
  if (!fs_seek(zip->file, header->centralDirectoryOffset, FROM_START)) {
    fs_close(zip->file);
    return false;
  }

  // Process the entries
  for (uint16_t i = 0; i < header->totalEntryCount; i++) {
    size_t size = ZIP_ENTRY_SIZE;
    uint8_t entryData[ZIP_ENTRY_SIZE];
    if (!fs_read(zip->file, entryData, &size) || size != sizeof(entryData)) {
      fs_close(zip->file);
      return false;
    }

    zipEntry* entry = (zipEntry*) entryData;

    //

    if (!fs_seek(zip->file, entry->nameLength + entry->extraLength + entry->commentLength, FROM_CURRENT)) {
      fs_close(zip->file);
      return false;
    }
  }

  return true;
}

bool zip_close(zip_t* zip) {
  return fs_close(zip->file);
}

bool zip_read(zip_t* zip, const char* path, void* buffer, size_t* bytes) {
  return false;
}

bool zip_stat(zip_t* zip, const char* path, FileInfo* info) {
  return false;
}

bool zip_list(zip_t* zip, const char* path, zip_list_cb* callback) {
  return false;
}
