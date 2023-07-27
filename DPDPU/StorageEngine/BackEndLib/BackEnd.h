#pragma once

#include <time.h>

#define DDS_MAX_DIRS 2
#define DDS_MAX_FILE_PATH 64
#define DDS_MAX_FILES 1
#define DDS_DIR_INVALID 0xffffffffUL
#define DDS_DIR_ROOT 0
#define DDS_FILE_INVALID 0xffffffffUL
#define DDS_PAGE_SIZE 4096

#define DDS_ERROR_CODE_SUCCESS 0
#define DDS_ERROR_CODE_DIRS_TOO_MANY 1
#define DDS_ERROR_CODE_FILES_TOO_MANY 2
#define DDS_ERROR_CODE_DIR_EXISTS 3
#define DDS_ERROR_CODE_DIR_NOT_FOUND 4
#define DDS_ERROR_CODE_DIR_NOT_EMPTY 5
#define DDS_ERROR_CODE_FILE_EXISTS 6
#define DDS_ERROR_CODE_FILE_NOT_FOUND 7
#define DDS_ERROR_CODE_READ_OVERFLOW 8
#define DDS_ERROR_CODE_WRITE_OVERFLOW 9
#define DDS_ERROR_CODE_STORAGE_OUT_OF_SPACE 16
#define DDS_ERROR_CODE_INVALID_FILE_POSITION 17

namespace DDS_BackEnd {

//
// Types used in DDS
//
//
typedef char* BufferT;
typedef void* ContextT;
typedef int DirIdT;
typedef int ErrorCodeT;
typedef unsigned long FileAttributesT;
typedef unsigned long FileIdT;
typedef unsigned long FileIOSizeT;
typedef unsigned long long FileSizeT;

//
// Properties of a file
// Differences from front end:
// 1. No file pointer
// 2. No access
// 3. No share mode
// These are not needed in back end
//
//
typedef struct FileProperties {
    FileAttributesT FileAttributes;
    time_t CreationTime;
    time_t LastAccessTime;
    time_t LastWriteTime;
    FileSizeT FileSize;
    char FileName[DDS_MAX_FILE_PATH];
} FilePropertiesT;

//
// Callbacks for async operations
//
//
typedef void (*ReadWriteCallback)(
    ErrorCodeT ErrorCode,
    FileSizeT BytesServiced,
    ContextT Context
);

}