#pragma once

#include <atomic>

#include "DDSDir.h"
#include "DDSFile.h"
#include "DDSFrontEndInterface.h"
#include "DMABuffer.h"
#include "RingBufferProgressive.h"

#define BACKEND_TYPE_LOCAL_MEMORY 0
#define BACKEND_TYPE_DPU 1
#define BACKEND_TYPE BACKEND_TYPE_DPU

#if BACKEND_TYPE == BACKEND_TYPE_DPU
#include "DDSBackEndBridge.h"
#elif BACKEND_TYPE == BACKEND_TYPE_LOCAL_MEMORY
#include "DDSBackEndBridgeForLocalMemory.h"
#else
#error "Unknown backend type"
#endif

#undef CreateDirectory
#undef RemoveDirectory
#undef CreateFile
#undef DeleteFile
#undef FindFirstFile
#undef FindNextFile
#undef GetFileAttributes
#undef GetCurrentDirectory
#undef MoveFile

#define DDS_FRONTEND_MAX_OUTSTANDING 128

namespace DDS_FrontEnd {

template <typename T>
using Atomic = std::atomic<T>;

//
// File read/write operation
//
//
typedef struct FileIOT {
    bool IsRead = false;
    bool IsSegmented = false;
    DDSFile* FileReference = (DDSFile*)nullptr;
    FileIdT FileId = (FileIdT)DDS_FILE_INVALID;
    FileSizeT Offset = (FileSizeT)0;
    BufferT AppBuffer = (BufferT)nullptr;
    BufferT* AppBufferArray = (BufferT*)nullptr;
    FileIOSizeT BytesDesired = (FileIOSizeT)0;
    FileIOSizeT BytesServiced = (FileIOSizeT)0;
    ReadWriteCallback AppCallback = (ReadWriteCallback)nullptr;
    ContextT Context = (ContextT)nullptr;
    Atomic<bool> IsComplete = true;
    Atomic<bool> IsReadyForPoll = false;
} FileIOT;

//
// Poll structure
//
//
typedef struct PollT {
    FileIOT* OutstandingRequests[DDS_FRONTEND_MAX_OUTSTANDING];
    Atomic<size_t> NextRequestSlot;
#if BACKEND_TYPE == BACKEND_TYPE_DPU
    DMABuffer* MsgBuffer;
    struct RequestRingBufferProgressive* RequestRing;
    struct ResponseRingBufferProgressive* ResponseRing;
#endif

    PollT();

#if BACKEND_TYPE == BACKEND_TYPE_DPU
    ErrorCodeT SetUpDMABuffer(DDSBackEndBridge* BackEndDPU);
    void DestroyDMABuffer();
    void InitializeRings();
#endif
} PollT;

//
// Callback for back end
//
//
void
BackEndReadWriteCallbackHandler(
    ErrorCodeT ErrorCode,
    bool IsRead,
    FileIdT FileId,
    FileSizeT Offset,
    BufferT AppBuffer,
    FileIOSizeT BytesDesired,
    FileIOSizeT* BytesServiced,
    ContextT Context
);

//
// A dummy callback for app
//
//
void
DummyReadWriteCallback(
    ErrorCodeT ErrorCode,
    FileIOSizeT BytesServiced,
    ContextT Context
);

//
// A generic front end
//
//
class DDSFrontEnd : public DDSFrontEndInterface
{
private:
    char StoreName[DDS_MAX_DEVICE_NAME_LEN];
    DDSBackEndBridgeBase* BackEnd;
    DDSDir* AllDirs[DDS_MAX_DIRS];
    DirIdT DirIdEnd;
    DDSFile* AllFiles[DDS_MAX_FILES];
    FileIdT FileIdEnd;
    PollT* AllPolls[DDS_MAX_POLLS];
    PollIdT PollIdEnd;

private:
    void
    RemoveDirectoryRecursive(
        DDSDir* Dir
    );

public:
    DDSFrontEnd(
        const char* StoreName
    );

    ~DDSFrontEnd();

    //
    // Intialize the front end, including connecting to back end
    // and setting up the root directory and default poll
    //
    //
    ErrorCodeT
    Initialize();

    //
    // Create a diretory
    // 
    //
    ErrorCodeT
    CreateDirectory(
        const char* PathName,
        DirIdT* DirId
    );

    //
    // Delete a directory
    // 
    //
    ErrorCodeT
    RemoveDirectory(
        const char* PathName
    );

    //
    // Create a file
    // 
    //
    ErrorCodeT
    CreateFile(
        const char* FileName,
        FileAccessT DesiredAccess,
        FileShareModeT ShareMode,
        FileAttributesT FileAttributes,
        FileIdT* FileId
    );

    //
    // Delete a file
    // 
    //
    ErrorCodeT
    DeleteFile(
        const char* FileName
    );

    //
    // Change the size of a file
    // 
    //
    ErrorCodeT
    ChangeFileSize(
        FileIdT FileId,
        FileSizeT NewSize
    );
    
    //
    // Set the physical file size for the specified file
    // to the current position of the file pointer
    // 
    //
    ErrorCodeT
    SetEndOfFile(
        FileIdT FileId
    );

    //
    // Move the file pointer of the specified file
    // 
    //
    ErrorCodeT
    SetFilePointer(
        FileIdT FileId,
        PositionInFileT DistanceToMove,
        FilePointerPosition MoveMethod
    );

    //
    // Get file size
    // 
    //
    ErrorCodeT
    GetFileSize(
        FileIdT FileId,
        FileSizeT* FileSize
    );

    //
    // Async read from a file
    // 
    //
    ErrorCodeT
    ReadFile(
        FileIdT FileId,
        BufferT DestBuffer,
        FileIOSizeT BytesToRead,
        FileIOSizeT* BytesRead,
        ReadWriteCallback Callback,
        ContextT Context
    );

    //
    // Async read from a file with scattering
    // 
    //
    ErrorCodeT
    ReadFileScatter(
        FileIdT FileId,
        BufferT* DestBufferArray,
        FileIOSizeT BytesToRead,
        FileIOSizeT* BytesRead,
        ReadWriteCallback Callback,
        ContextT Context
    );

    //
    // Async write to a file
    // 
    //
    ErrorCodeT
    WriteFile(
        FileIdT FileId,
        BufferT SourceBuffer,
        FileIOSizeT BytesToWrite,
        FileIOSizeT* BytesWritten,
        ReadWriteCallback Callback,
        ContextT Context
    );

    //
    // Async write to a file with gathering
    // 
    //
    ErrorCodeT
    WriteFileGather(
        FileIdT FileId,
        BufferT* SourceBufferArray,
        FileIOSizeT BytesToWrite,
        FileIOSizeT* BytesWritten,
        ReadWriteCallback Callback,
        ContextT Context
    );

    //
    // Flush buffered data to storage
    // Note: buffering is disabled
    // 
    //
    ErrorCodeT
    FlushFileBuffers(
        FileIdT FileId
    );

    //
    // Search a directory for a file or subdirectory
    // with a name that matches a specific name
    // (or partial name if wildcards are used)
    // 
    //
    ErrorCodeT
    FindFirstFile(
        const char* FileName,
        FileIdT* FileId
    );

    //
    // Continue a file search from a previous call
    // to the FindFirstFile
    // 
    //
    ErrorCodeT
    FindNextFile(
        FileIdT LastFileId,
        FileIdT* FileId
    );

    //
    // Get file properties by file Id
    // 
    //
    ErrorCodeT
    GetFileInformationById(
        FileIdT FileId,
        FilePropertiesT* FileProperties
    );
    
    //
    // Get file attributes by file name
    // 
    //
    ErrorCodeT
    GetFileAttributes(
        const char* FileName,
        FileAttributesT* FileAttributes
    );

    //
    // Retrieve the current directory for the
    // current process
    // 
    //
    ErrorCodeT
    GetCurrentDirectory(
        unsigned BufferLength,
        BufferT DirectoryBuffer
    );

    //
    // Get the size of free space on the storage
    // 
    //
    ErrorCodeT
    GetStorageFreeSpace(
        FileSizeT* StorageFreeSpace
    );

    //
    // Lock the specified file for exclusive
    // access by the calling process
    // 
    //
    ErrorCodeT
    LockFile(
        FileIdT FileId,
        PositionInFileT FileOffset,
        PositionInFileT NumberOfBytesToLock
    );

    //
    // Unlock a region in an open file;
    // Unlocking a region enables other
    // processes to access the region
    // 
    //
    ErrorCodeT
    UnlockFile(
        FileIdT FileId,
        PositionInFileT FileOffset,
        PositionInFileT NumberOfBytesToUnlock
    );

    //
    // Move an existing file or a directory,
    // including its children
    // 
    //
    ErrorCodeT
    MoveFile(
        const char* ExistingFileName,
        const char* NewFileName
    );

    //
    // Get the default poll structure for async I/O
    //
    //
    ErrorCodeT
    GetDefaultPoll(
        PollIdT* PollId
    ) ;

    //
    // Create a poll structure for async I/O
    //
    //
    ErrorCodeT
    PollCreate(
        PollIdT* PollId
    );

    //
    // Delete a poll structure
    //
    //
    ErrorCodeT
    PollDelete(
        PollIdT PollId
    );

    //
    // Add a file to a pool
    //
    //
    ErrorCodeT
    PollAdd(
        FileIdT FileId,
        PollIdT PollId,
        ContextT FileContext
    );

    //
    // Poll a completion event
    //
    //
    ErrorCodeT
    PollWait(
        PollIdT PollId,
        FileIOSizeT* BytesServiced,
        ContextT* FileContext,
        ContextT* IOContext,
        size_t WaitTime,
        bool* PollResult
    );
};

}