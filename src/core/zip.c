#include "zip.h"

#define ZIP_HEADER_SIZE 22
#define ZIP_METADATA_SIZE 46
#define ZIP_HEADER_MAGIC 0x06054b50
#define ZIP_METADATA_MAGIC 0x02014b50

typedef struct {
  uint32_t magic;
  uint16_t disk;
  uint16_t startDisk;
  uint16_t localEntryCount;
  uint16_t totalEntryCount;
  uint32_t centralDirectorySize;
  uint32_t centralDirectoryOffset;
  uint16_t commentLength;
} zip_header;

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
} zip_metadata;

bool zip_open(zip_t* zip, zip_io io, void* context) {
  zip->ctx = context;
  zip->io = io;

  // Seek to the end of the file, minus the header size
  if (!zip->io.seek(zip->ctx, -ZIP_HEADER_SIZE, 1)) {
    return false;
  }

  // Read the header
  size_t size = ZIP_HEADER_SIZE;
  uint8_t headerData[ZIP_HEADER_SIZE];
  if (!zip->io.read(zip->ctx, headerData, &size) || size != sizeof(headerData)) {
    return false;
  }

  // Validation
  zip_header* header = (zip_header*) headerData;
  if (header->magic != ZIP_HEADER_MAGIC) {
    return false;
  }

  // Seek to the entry list
  if (!zip->io.seek(zip->ctx, header->centralDirectoryOffset, -1)) {
    return false;
  }

  return true;
}

bool zip_next(zip_t* zip, zip_entry* entry) {
  size_t size = ZIP_METADATA_SIZE;
  uint8_t metadata[ZIP_METADATA_SIZE];

  // Read the metadata
  if (!zip->io.read(zip->ctx, metadata, &size) || size != sizeof(metadata)) {
    return false;
  }

  zip_metadata* file = (zip_metadata*) metadata;

  entry->offset = file->offset;

  // Seek to the next entry
  if (!zip->io.seek(zip->ctx, file->nameLength + file->extraLength + file->commentLength, 0)) {
    return false;
  }

  return true;
}
