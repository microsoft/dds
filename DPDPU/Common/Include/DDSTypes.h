#include <time.h>

#include "Protocol.h"

//
// Types used in DDS
//
//
typedef void* BufferT;
typedef void* ContextT;
typedef int DirIdT;
typedef int ErrorCodeT;
typedef unsigned long FileAccessT;
typedef unsigned long FileAttributesT;
typedef unsigned long FileIdT;
typedef long long PositionInFileT;
typedef unsigned long FileShareModeT;
typedef unsigned long FileCreationDispositionT;
typedef unsigned long FileIOSizeT;
typedef unsigned long long FileSizeT;
typedef unsigned long PollIdT;
typedef unsigned int RingSizeT;
typedef unsigned int RingSlotSizeT;

//
// Types of position in file
//
//
enum class FilePointerPosition {
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
  PositionInFileT Position;
  FileShareModeT ShareMode;
  char FileName[DDS_MAX_FILE_PATH];
} FilePropertiesT;