#include "fs.h"
#include <SDL3/SDL.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

#ifdef LOVR_USE_SDL3
#ifndef LOVR_HAS_MMAP
#define SDL_USE_BASIC_TYPES
#endif
#endif

/*
 * POINTER TAGGING FOR MEMORY MAPPING TYPE DISCRIMINATION
 * 
 * PROBLEM:
 * fs_map() may return either:
 *   1. A pointer from mmap() (POSIX/Windows) - must be unmapped with munmap()
 *   2. A pointer from SDL_LoadFile() (fallback) - must be freed with SDL_free()
 * 
 * We need to know at fs_unmap() time which deallocation method to use.
 * 
 * SOLUTION:
 * Use the least significant bit of the pointer as a type tag.
 * 
 * WHY THIS IS SAFE:
 * 
 * 1. MMAP ALIGNMENT GUARANTEES:
 *    - Linux:   mmap() returns page-aligned addresses (4KB alignment)
 *              Documentation: "If addr is NULL, then the kernel chooses the 
 *              (page-aligned) address" - mmap(2) man page, Linux man-pages 6.16
 *    
 *    - Windows: MapViewOfFile() returns allocation-granularity aligned addresses
 *              (typically 64KB alignment)
 *              Documentation: "the offset must be a multiple of the VirtualAlloc 
 *              allocation granularity" - Microsoft Learn
 *    
 *    - macOS:   mmap() returns page-aligned addresses (4KB alignment)
 *              BSD mmap guarantees system-selected addresses are page-aligned
 * 
 * 2. HEAP ALIGNMENT GUARANTEES:
 *    - SDL_malloc() returns memory aligned to at least 8 bytes (usually 16)
 *    - The C standard requires malloc() to return memory suitably aligned for 
 *      any fundamental type, which is at least sizeof(void*) alignment
 * 
 * 3. PLATFORM COVERAGE:
 *    - x86/x64: Page size is 4KB (2^12), so bottom 12 bits are always 0
 *    - ARM/ARM64: Page size is typically 4KB or 16KB, bottom 12-14 bits are 0
 *    - All modern systems use power-of-2 page sizes for efficiency
 * 
 * 4. TAG CHOICE:
 *    - We use ONLY bit 0 (the least significant bit)
 *    - MMAP addresses always have bit 0 = 0 (even alignment)
 *    - HEAP addresses always have bit 0 = 0 (8+ byte alignment)
 *    - We set bit 0 = 1 to tag MMAP pointers, leave as 0 for HEAP
 *    - This gives us 1 bit of type information with ZERO risk of collision
 */

// Tag values stored in bit 0
#define FS_MAP_TAG_MMAP  0x1  // Set this bit for mmap'd memory
#define FS_MAP_TAG_HEAP  0x0  // Leave clear for heap-allocated memory

// Mask to extract just the tag bit
#define FS_MAP_TAG_MASK  0x1

// Helper: Extract pointer from tagged value (clear the tag bit)
#define FS_MAP_UNTAG_PTR(ptr) ((void*)((uintptr_t)(ptr) & ~FS_MAP_TAG_MASK))

// Helper: Extract tag from pointer value
#define FS_MAP_GET_TAG(ptr) ((uintptr_t)(ptr) & FS_MAP_TAG_MASK)

// Helper: Tag a pointer as mmap'd memory
#define FS_MAP_TAG_AS_MMAP(ptr) ((void*)((uintptr_t)(ptr) | FS_MAP_TAG_MMAP))

// Helper: Tag a pointer as heap memory (identity, but explicit)
#define FS_MAP_TAG_AS_HEAP(ptr) ((void*)((uintptr_t)(ptr) | FS_MAP_TAG_HEAP))


// Internal structure to hold SDL file handle
typedef struct {
    SDL_IOStream* stream;
} sdl_file_handle;

static bool sdl_stream_stat(SDL_IOStream* stream, fs_info* info) {
    SDL_PropertiesID props = SDL_GetIOProperties(stream);
    if (!props) {
        return false;
    }

#ifdef _WIN32
    HANDLE handle = (HANDLE) SDL_GetPointerProperty(props, SDL_PROP_IOSTREAM_WINDOWS_HANDLE_POINTER, NULL);
    if (!handle || handle == INVALID_HANDLE_VALUE) {
        return false;
    }

    BY_HANDLE_FILE_INFORMATION attributes;
    if (!GetFileInformationByHandle(handle, &attributes)) {
        return false;
    }

    FILETIME lastModified = attributes.ftLastWriteTime;
    info->type = (attributes.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? FILE_DIRECTORY : FILE_REGULAR;
    info->lastModified = ((uint64_t) lastModified.dwHighDateTime << 32) | lastModified.dwLowDateTime;
    info->lastModified /= 10000000ULL;
    info->lastModified -= 11644473600ULL;

    LARGE_INTEGER size;
    if (!GetFileSizeEx(handle, &size)) {
        return false;
    }

    info->size = size.QuadPart;
    return true;
#else
    int fd = (int) SDL_GetNumberProperty(props, SDL_PROP_IOSTREAM_FILE_DESCRIPTOR_NUMBER, -1);
    if (fd < 0) {
        return false;
    }

    struct stat stats;
    if (fstat(fd, &stats) < 0) {
        return false;
    }

    info->size = (uint64_t) stats.st_size;
    info->lastModified = (uint64_t) stats.st_mtime;
    info->type = S_ISDIR(stats.st_mode) ? FILE_DIRECTORY : FILE_REGULAR;
    return true;
#endif
}

static fs_error sdl_error_to_fs(void) {
    const char* err = SDL_GetError();
    if (strstr(err, "not found") || strstr(err, "No such file")) return FS_NOT_FOUND;
    if (strstr(err, "permission") || strstr(err, "access denied") || strstr(err, "denied")) return FS_PERMISSION;
    if (strstr(err, "already exists") || strstr(err, "file exists")) return FS_EXISTS;
    if (strstr(err, "is a directory")) return FS_IS_DIR;
    if (strstr(err, "not a directory")) return FS_NOT_DIR;
    if (strstr(err, "directory not empty")) return FS_NOT_EMPTY;
    if (strstr(err, "no space") || strstr(err, "disk full") || strstr(err, "out of memory")) return FS_FULL;
    if (strstr(err, "too long") || strstr(err, "name too long")) return FS_TOO_LONG;
    if (strstr(err, "busy") || strstr(err, "resource busy")) return FS_BUSY;
    return FS_UNKNOWN_ERROR;
}

static fs_error posix_error_to_fs(void) {
    switch (errno) {
        case EACCES: return FS_PERMISSION;
        case EPERM: return FS_PERMISSION;
        case EROFS: return FS_READ_ONLY;
        case EEXIST: return FS_EXISTS;
        case ENOENT: return FS_NOT_FOUND;
        case EDQUOT: return FS_FULL;
        case ENOSPC: return FS_FULL;
        case ENOTDIR: return FS_NOT_DIR;
        case EISDIR: return FS_IS_DIR;
        case ENOTEMPTY: return FS_NOT_EMPTY;
        case ELOOP: return FS_LOOP;
        case ETXTBSY: return FS_BUSY;
        case EIO: return FS_IO;
        default: return FS_UNKNOWN_ERROR;
    }
}

fs_error fs_open(const char* path, char mode, fs_handle* file) {
    const char* smode;
    switch (mode) {
        case 'r': smode = "rb"; break;
        case 'w': smode = "wb"; break;
        case 'a': smode = "ab"; break;
        default: return FS_UNKNOWN_ERROR;
    }

    SDL_IOStream* stream = SDL_IOFromFile(path, smode);
    if (!stream) {
        return sdl_error_to_fs();
    }

    fs_info info;
    if (sdl_stream_stat(stream, &info) && info.type == FILE_DIRECTORY) {
        SDL_CloseIO(stream);
        return FS_IS_DIR;
    }

    sdl_file_handle* internal = SDL_malloc(sizeof(sdl_file_handle));
    if (!internal) {
        SDL_CloseIO(stream);
        return FS_UNKNOWN_ERROR;
    }

    internal->stream = stream;
    file->handle = internal;
    return FS_OK;
}

fs_error fs_close(fs_handle file) {
    sdl_file_handle* internal = (sdl_file_handle*)file.handle;
    if (!internal) {
        return FS_UNKNOWN_ERROR;
    }
    bool ok = true;
    if (internal->stream) {
        ok = SDL_CloseIO(internal->stream);
    }
    SDL_free(internal);
    return ok ? FS_OK : sdl_error_to_fs();
}

fs_error fs_read(fs_handle file, void* data, size_t size, size_t* count) {
    if (!count) {
        return FS_UNKNOWN_ERROR;
    }
    sdl_file_handle* internal = (sdl_file_handle*)file.handle;
    if (!internal || !internal->stream) {
        *count = 0;
        return FS_UNKNOWN_ERROR;
    }
    *count = SDL_ReadIO(internal->stream, data, size);
    if (*count == 0) {
        if (SDL_GetIOStatus(internal->stream) != SDL_IO_STATUS_EOF) {
            return sdl_error_to_fs();
        }
    }
    return FS_OK;
}

fs_error fs_write(fs_handle file, const void* data, size_t size, size_t* count) {
    if (!count) {
        return FS_UNKNOWN_ERROR;
    }
    sdl_file_handle* internal = (sdl_file_handle*)file.handle;
    if (!internal || !internal->stream) {
        *count = 0;
        return FS_UNKNOWN_ERROR;
    }
    *count = SDL_WriteIO(internal->stream, data, size);
    if (*count < size) {
        return sdl_error_to_fs();
    }
    return FS_OK;
}

fs_error fs_seek(fs_handle file, uint64_t offset) {
    sdl_file_handle* internal = (sdl_file_handle*)file.handle;
    if (!internal || !internal->stream) {
        return FS_UNKNOWN_ERROR;
    }
    Sint64 result = SDL_SeekIO(internal->stream, (Sint64)offset, SDL_IO_SEEK_SET);
    if (result < 0) {
        return sdl_error_to_fs();
    }
    return FS_OK;
}

fs_error fs_fstat(fs_handle file, fs_info* info) {
    sdl_file_handle* internal = (sdl_file_handle*)file.handle;
    if (!internal || !internal->stream) {
        return FS_UNKNOWN_ERROR;
    }

    if (sdl_stream_stat(internal->stream, info)) {
        return FS_OK;
    }

    Sint64 size = SDL_GetIOSize(internal->stream);
    if (size < 0) {
        return sdl_error_to_fs();
    }

    info->size = (uint64_t) size;
    info->lastModified = 0;
    info->type = FILE_REGULAR;

    return FS_OK;
}

fs_error fs_stat(const char* path, fs_info* info) {
    SDL_PathInfo pathinfo;
    if (!SDL_GetPathInfo(path, &pathinfo)) {
        return sdl_error_to_fs();
    }

    info->size = pathinfo.size;
    info->lastModified = pathinfo.modify_time / 1000000000ULL;

    switch (pathinfo.type) {
        case SDL_PATHTYPE_DIRECTORY:
            info->type = FILE_DIRECTORY;
            break;
        case SDL_PATHTYPE_FILE:
            info->type = FILE_REGULAR;
            break;
        default:
            info->type = FILE_REGULAR;
            break;
    }

    return FS_OK;
}

fs_error fs_map(const char* path, void** pointer, size_t* size) {
#ifdef LOVR_USE_SDL3
    #ifdef LOVR_HAS_MMAP
    {
        int fd = open(path, O_RDONLY);
        if (fd >= 0) {
            struct stat st;
            if (fstat(fd, &st) == 0) {
                *size = (size_t)st.st_size;
                void* addr = mmap(NULL, *size, PROT_READ, MAP_PRIVATE, fd, 0);
                close(fd);
                if (addr != MAP_FAILED) {
                    // Tag the pointer to indicate it came from mmap.
                    // This is safe because mmap guarantees page alignment
                    // (minimum 4KB on all platforms), so the bottom bits are always 0.
                    *pointer = FS_MAP_TAG_AS_MMAP(addr);
                    return FS_OK;
                }
            } else {
                close(fd);
            }
        }
    }
    #endif

    void* addr = SDL_LoadFile(path, size);
    if (addr) {
        // Tag as heap memory (technically no-op since bit 0 is already 0,
        // but explicit for clarity and consistency)
        *pointer = FS_MAP_TAG_AS_HEAP(addr);
        return FS_OK;
    }
    return sdl_error_to_fs();
#else
    #ifdef _WIN32
    {
        WCHAR wpath[1024];
        if (!MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, 1024)) {
            return FS_UNKNOWN_ERROR;
        }
        HANDLE file = CreateFileW(wpath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (file == INVALID_HANDLE_VALUE) {
            return FS_UNKNOWN_ERROR;
        }
        DWORD hi;
        DWORD lo = GetFileSize(file, &hi);
        if (lo == INVALID_FILE_SIZE) {
            CloseHandle(file);
            return FS_UNKNOWN_ERROR;
        }
        if (SIZE_MAX > UINT32_MAX) {
            *size = ((size_t)hi << 32) | lo;
        } else {
            *size = lo;
        }
        HANDLE mapping = CreateFileMappingA(file, NULL, PAGE_READONLY, hi, lo, NULL);
        if (!mapping) {
            CloseHandle(file);
            return FS_UNKNOWN_ERROR;
        }
        *pointer = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, *size);
        CloseHandle(mapping);
        CloseHandle(file);
        if (*pointer) {
            // Tag the pointer to indicate it came from MapViewOfFile.
            // Windows allocation granularity is typically 64KB, so bottom bits are 0.
            *pointer = FS_MAP_TAG_AS_MMAP(*pointer);
            return FS_OK;
        }
        return FS_UNKNOWN_ERROR;
    }
    #else
    {
        int fd = open(path, O_RDONLY);
        if (fd < 0) return posix_error_to_fs();
        struct stat st;
        if (fstat(fd, &st) < 0) {
            close(fd);
            return posix_error_to_fs();
        }
        *size = (size_t)st.st_size;
        *pointer = mmap(NULL, *size, PROT_READ, MAP_PRIVATE, fd, 0);
        close(fd);
        if (*pointer == MAP_FAILED) {
            *pointer = NULL;
            return posix_error_to_fs();
        }
        // Tag the pointer to indicate it came from mmap.
        *pointer = FS_MAP_TAG_AS_MMAP(*pointer);
        return FS_OK;
    }
    #endif
#endif
}

fs_error fs_unmap(void* data, size_t size) {
    if (!data) {
        return FS_UNKNOWN_ERROR;
    }
    
    // Extract the tag and untagged pointer
    uintptr_t tag = FS_MAP_GET_TAG(data);
    void* ptr = FS_MAP_UNTAG_PTR(data);
    
    if (tag == FS_MAP_TAG_MMAP) {
        // This was mmap'd memory - unmap it properly
        #ifdef LOVR_USE_SDL3
            #ifdef LOVR_HAS_MMAP
            if (munmap(ptr, size) != 0) {
                return posix_error_to_fs();
            }
            #else
            // Should never happen - if LOVR_HAS_MMAP is not defined,
            // we should never have tagged anything as MMAP
            return FS_UNKNOWN_ERROR;
            #endif
        #else
            #ifdef _WIN32
            if (!UnmapViewOfFile(ptr)) {
                return FS_UNKNOWN_ERROR;
            }
            #else
            if (munmap(ptr, size) != 0) {
                return posix_error_to_fs();
            }
            #endif
        #endif
        return FS_OK;
    } else {
        // This was heap-allocated via SDL_LoadFile
        SDL_free(ptr);
        return FS_OK;
    }
}

fs_error fs_remove(const char* path) {
    if (!SDL_RemovePath(path)) {
        return sdl_error_to_fs();
    }
    return FS_OK;
}

fs_error fs_mkdir(const char* path) {
    if (!SDL_CreateDirectory(path)) {
        return sdl_error_to_fs();
    }
    return FS_OK;
}

struct fs_list_context {
    fs_list_cb* callback;
    void* context;
};

static SDL_EnumerationResult SDLCALL fs_list_callback(void* userdata, const char* dirname, const char* fname) {
    (void)dirname;
    if (strcmp(fname, ".") == 0 || strcmp(fname, "..") == 0) {
        return SDL_ENUM_CONTINUE;
    }
    struct fs_list_context* ctx = (struct fs_list_context*)userdata;
    ctx->callback(ctx->context, fname);
    return SDL_ENUM_CONTINUE;
}

fs_error fs_list(const char* path, fs_list_cb* callback, void* context) {
    struct fs_list_context ctx = { .callback = callback, .context = context };
    if (!SDL_EnumerateDirectory(path, fs_list_callback, &ctx)) {
        return sdl_error_to_fs();
    }
    return FS_OK;
}
