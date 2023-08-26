#pragma once

#include <atomic>
#include <stdint.h>
#include <time.h>

#include "Protocol.h"

template <class C>
using Atomic=std::atomic<C>;

//
// Types used in DDS
//
//
typedef char* BufferT;
typedef void* ContextT;
typedef uint32_t DirIdT;
typedef uint32_t ErrorCodeT;
typedef uint32_t FileAccessT;
typedef uint32_t FileAttributesT;
typedef uint32_t FileIdT;
typedef int64_t PositionInFileT;
typedef uint32_t FileShareModeT;
typedef uint32_t FileCreationDispositionT;
typedef uint32_t FileIOSizeT;
typedef uint64_t FileSizeT;
typedef uint32_t PollIdT;
typedef uint32_t RingSizeT;
typedef uint32_t RingSlotSizeT;
typedef uint16_t RequestIdT;

//
// Types of position in file
//
//
enum FilePointerPosition {
    BEGIN,
    CURRENT,
    END
};

//
// Properties of a file
//
//
typedef struct FileProperties {
  FileAttributesT FileAttributes;
  time_t CreationTime;
  time_t LastAccessTime;
  time_t LastWriteTime;
  FileSizeT FileSize;
  FileAccessT Access;
  Atomic<PositionInFileT> Position;
  FileShareModeT ShareMode;
  char FileName[DDS_MAX_FILE_PATH];
} FilePropertiesT;
