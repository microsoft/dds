#pragma once

#include <time.h>

#define DDS_MAX_FILE_PATH 64
#define DDS_MAX_DEVICE_NAME_LEN 32
#define DDS_MAX_DIRS 2
#define DDS_MAX_FILES 1
#define DDS_MAX_POLLS 1
#define DDS_DIR_INVALID 0xffffffffUL
#define DDS_DIR_ROOT 0
#define DDS_FILE_INVALID 0xffffffffUL
#define DDS_PAGE_SIZE 4096
#define DDS_POLL_DEFAULT 0
#define DDS_POLL_MAX_LATENCY_MICROSECONDS 100
#define DDS_REQUEST_RING_SIZE 4096
#define DDS_REQUEST_RING_SLOT_SIZE DDS_PAGE_SIZE
#define DDS_REQUEST_RING_BYTES 16777216
#define DDS_CACHE_LINE_SIZE 64
#define DDS_CACHE_LINE_SIZE_BY_INT 16
#define DDS_CACHE_LINE_SIZE_BY_POINTER 8

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
#define DDS_ERROR_CODE_BUFFER_OVERFLOW 10
#define DDS_ERROR_CODE_NOT_IMPLEMENTED 11
#define DDS_ERROR_CODE_REQUIRE_POLLING 12
#define DDS_ERROR_CODE_POLLS_TOO_MANY 13
#define DDS_ERROR_CODE_INVALID_POLL_DELETION 14
#define DDS_ERROR_CODE_IO_PENDING 15
#define DDS_ERROR_CODE_STORAGE_OUT_OF_SPACE 16
#define DDS_ERROR_CODE_INVALID_FILE_POSITION 17

//
// Checking a few parameters at the compile time
//
//
#pragma warning (disable : 4804)
#define assert_static(e, num) \
    enum { assert_static__##num = 1/(e) }

assert_static(DDS_REQUEST_RING_BYTES == DDS_REQUEST_RING_SIZE * DDS_REQUEST_RING_SLOT_SIZE, 0);
assert_static(DDS_REQUEST_RING_BYTES % DDS_CACHE_LINE_SIZE == 0, 1);

namespace DDS_FrontEnd {

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

//
// Callbacks for async operations
//
//
typedef void (*ReadWriteCallback)(
    ErrorCodeT ErrorCode,
    FileIOSizeT BytesServiced,
    ContextT Context
);

//
// The interface of file system control plane
//
//
class DDSFrontEndInterface
{
public:

    //
    // Intialize the front end, including connecting to back end
    // and setting up the root directory and default poll
    //
    //
    virtual
    ErrorCodeT
    Initialize() = 0;
    
    //
    // Create a diretory
    // 
    //
    virtual
    ErrorCodeT
    CreateDirectory(
        const char* PathName,
        DirIdT* DirId
    ) = 0;

    //
    // Delete a directory
    // 
    //
    virtual
    ErrorCodeT
    RemoveDirectory(
        const char* PathName
    ) = 0;

    //
    // Create a file
    // 
    //
    virtual
    ErrorCodeT
    CreateFile(
        const char* FileName,
        FileAccessT DesiredAccess,
        FileShareModeT ShareMode,
        FileAttributesT FileAttributes,
        FileIdT* FileId
    ) = 0;

    //
    // Delete a file
    // 
    //
    virtual
    ErrorCodeT
    DeleteFile(
        const char* FileName
    ) = 0;

    //
    // Change the size of a file
    // 
    //
    virtual
    ErrorCodeT
    ChangeFileSize(
        FileIdT FileId,
        FileSizeT NewSize
    ) = 0;
    
    //
    // Set the physical file size for the specified file
    // to the current position of the file pointer
    // 
    //
    virtual
    ErrorCodeT
    SetEndOfFile(
        FileIdT FileId
    ) = 0;

    //
    // Move the file pointer of the specified file
    // 
    //
    virtual
    ErrorCodeT
    SetFilePointer(
        FileIdT FileId,
        PositionInFileT DistanceToMove,
        FilePointerPosition MoveMethod
    ) = 0;

    //
    // Get file size
    // 
    //
    virtual
    ErrorCodeT
    GetFileSize(
        FileIdT FileId,
        FileSizeT* FileSize
    ) = 0;

    //
    // Async read from a file
    // 
    //
    virtual
    ErrorCodeT
    ReadFile(
        FileIdT FileId,
        BufferT DestBuffer,
        FileIOSizeT BytesToRead,
        FileIOSizeT* BytesRead,
        ReadWriteCallback Callback,
        ContextT Context
    ) = 0;

    //
    // Async read from a file with scattering
    // 
    //
    virtual
    ErrorCodeT
    ReadFileScatter(
        FileIdT FileId,
        BufferT* DestBufferArray,
        FileIOSizeT BytesToRead,
        FileIOSizeT* BytesRead,
        ReadWriteCallback Callback,
        ContextT Context
    ) = 0;

    //
    // Async write to a file
    // 
    //
    virtual
    ErrorCodeT
    WriteFile(
        FileIdT FileId,
        BufferT SourceBuffer,
        FileIOSizeT BytesToWrite,
        FileIOSizeT* BytesWritten,
        ReadWriteCallback Callback,
        ContextT Context
    ) = 0;

    //
    // Async write to a file with gathering
    // 
    //
    virtual
    ErrorCodeT
    WriteFileGather(
        FileIdT FileId,
        BufferT* SourceBufferArray,
        FileIOSizeT BytesToWrite,
        FileIOSizeT* BytesWritten,
        ReadWriteCallback Callback,
        ContextT Context
    ) = 0;

    //
    // Flush buffered data to storage
    // 
    //
    virtual
    ErrorCodeT
    FlushFileBuffers(
        FileIdT FileId
    ) = 0;

    //
    // Search a directory for a file or subdirectory
    // with a name that matches a specific name
    // (or partial name if wildcards are used)
    // 
    //
    virtual
    ErrorCodeT
    FindFirstFile(
        const char* FileName,
        FileIdT* FileId
    ) = 0;

    //
    // Continue a file search from a previous call
    // to the FindFirstFile
    // 
    //
    virtual
    ErrorCodeT
    FindNextFile(
        FileIdT LastFileId,
        FileIdT* FileId
    ) = 0;

    //
    // Get file properties by file Id
    // 
    //
    virtual
    ErrorCodeT
    GetFileInformationById(
        FileIdT FileId,
        FilePropertiesT* FileProperties
    ) = 0;
    
    //
    // Get file attributes by file name
    // 
    //
    virtual
    ErrorCodeT
    GetFileAttributes(
        const char* FileName,
        FileAttributesT* FileAttributes
    ) = 0;

    //
    // Retrieve the current directory for the
    // current process
    // 
    //
    virtual
    ErrorCodeT
    GetCurrentDirectory(
        unsigned BufferLength,
        BufferT DirectoryBuffer
    ) = 0;

    //
    // Get the size of free space on the storage
    // 
    //
    virtual
    ErrorCodeT
    GetStorageFreeSpace(
        FileSizeT* StorageFreeSpace
    ) = 0;

    //
    // Lock the specified file for exclusive
    // access by the calling process
    // 
    //
    virtual
    ErrorCodeT
    LockFile(
        FileIdT FileId,
        PositionInFileT FileOffset,
        PositionInFileT NumberOfBytesToLock
    ) = 0;

    //
    // Unlock a region in an open file;
    // Unlocking a region enables other
    // processes to access the region
    // 
    //
    virtual
    ErrorCodeT
    UnlockFile(
        FileIdT FileId,
        PositionInFileT FileOffset,
        PositionInFileT NumberOfBytesToUnlock
    ) = 0;

    //
    // Move an existing file or a directory,
    // including its children
    // 
    //
    virtual
    ErrorCodeT
    MoveFile(
        const char* ExistingFileName,
        const char* NewFileName
    ) = 0;

    //
    // Get the default poll structure for async I/O
    //
    //
    virtual
    ErrorCodeT
    GetDefaultPoll(
        PollIdT* PollId
    ) = 0;

    //
    // Create a poll structure for async I/O
    //
    //
    virtual
    ErrorCodeT
    PollCreate(
        PollIdT* PollId
    ) = 0;

    //
    // Delete a poll structure
    //
    //
    virtual
    ErrorCodeT
    PollDelete(
        PollIdT PollId
    ) = 0;

    //
    // Add a file to a pool
    //
    //
    virtual
    ErrorCodeT
    PollAdd(
        FileIdT FileId,
        PollIdT PollId,
        ContextT FileContext
    ) = 0;

    //
    // Poll a completion event
    //
    //
    virtual
    ErrorCodeT
    PollWait(
        PollIdT PollId,
        FileIOSizeT* BytesServiced,
        ContextT* FileContext,
        ContextT* IOContext,
        size_t WaitTime,
        bool* PollResult
    ) = 0;
};

}