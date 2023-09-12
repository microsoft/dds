#pragma once

#ifdef __GNUC__
#include <stdatomic.h>
#elif defined (_MSC_VER)
#include <atomic>
#endif
#include <stdint.h>
#include <time.h>

#include "Protocol.h"

#if defined (_MSC_VER)
template <class C>
using Atomic=std::atomic<C>;
#endif

//
// Types used in DDS
//
//
typedef char* BufferT;
typedef void* ContextT;
typedef uint16_t DirIdT;
typedef uint16_t ErrorCodeT;
typedef uint32_t FileAccessT;
typedef uint32_t FileAttributesT;
typedef uint16_t FileIdT;
typedef int64_t PositionInFileT;
typedef uint32_t FileShareModeT;
typedef uint32_t FileCreationDispositionT;
typedef uint32_t FileIOSizeT;
typedef uint64_t FileSizeT;
typedef uint16_t PollIdT;
typedef uint32_t RingSizeT;
typedef uint32_t RingSlotSizeT;
typedef uint16_t RequestIdT;
typedef uint8_t BoolT;

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
  FileSizeT FileSize ;
  FileAccessT Access;
#ifdef __GNUC__
  _Atomic PositionInFileT Position;
#elif defined (_MSC_VER)
  Atomic<PositionInFileT> Position = 0;
#endif
  FileShareModeT ShareMode;
  char FileName[DDS_MAX_FILE_PATH];
} FilePropertiesT;

//
// Describe an object that might be split on the ring buffer
//
//
typedef struct {
    RingSizeT TotalSize;
    RingSizeT FirstSize;
    BufferT FirstAddr;
    BufferT SecondAddr;
} SplittableBufferT;