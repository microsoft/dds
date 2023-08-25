#pragma once

// #include <atomic>
#include <stdatomic.h>
// #include <mutex>
#include <pthread.h>
#include <time.h>

#include "DPUBackEndDir.h"
#include "DPUBackEndFile.h"

//
// Context for each I/O operation in the back end, same with BackEndIOContext
// In C, we use <stdatomic.h> to replace <atomic>. We use atomic_ulong and 
// atomic_bool for BytesCompleted and IsAvailable
//
//
typedef struct BackEndIOContext {
    ContextT FrontEndBufferContext;
    ContextT BackEndBufferContext;
    FileIOSizeT BytesIssued;
    atomic_ulong BytesCompleted;
    atomic_bool IsAvailable;
    ErrorCodeT ErrorCode;
    bool IsRead;
} BackEndIOContextT;

//
// Callback for async disk operations
//
//
typedef void (*DiskIOCallback)(
    bool Success,
    ContextT Context
);


//
// DPU storage, highly similar with BackEndStorage
//
//
typedef struct DPUStorage {
    SegmentT* AllSegments;
    const SegmentIdT TotalSegments;
    SegmentIdT AvailableSegments;
    
    struct DPUDir* AllDirs[DDS_MAX_DIRS];
    struct DPUFile* AllFiles[DDS_MAX_FILES];
    int TotalDirs;
    int TotalFiles;

    pthread_mutex_t SectorModificationMutex;
    pthread_mutex_t SegmentAllocationMutex;
    
    atomic_ulong TargetProgress;
    atomic_ulong CurrentProgress;
};

//
// Read from disk synchronously
//
//
ErrorCodeT ReadFromDiskSync(
    BufferT DstBuffer,
    SegmentIdT SegmentId,
    SegmentSizeT SegmentOffset,
    FileIOSizeT Bytes
);

//
// Write from disk synchronously
//
//
ErrorCodeT WriteFromDiskSync(
    BufferT DstBuffer,
    SegmentIdT SegmentId,
    SegmentSizeT SegmentOffset,
    FileIOSizeT Bytes
);

//
// Read from disk asynchronously
//
//
ErrorCodeT ReadFromDiskAsync(
    BufferT DstBuffer,
    SegmentIdT SegmentId,
    SegmentSizeT SegmentOffset,
    FileIOSizeT Bytes,
    DiskIOCallback Callback,
    ContextT Context
);

//
// Write from disk asynchronously
//
//
ErrorCodeT WriteFromDiskAsync(
    BufferT DstBuffer,
    SegmentIdT SegmentId,
    SegmentSizeT SegmentOffset,
    FileIOSizeT Bytes,
    DiskIOCallback Callback,
    ContextT Context
);

//
// Retrieve all segments on the disk
//
//
ErrorCodeT RetrieveSegments();

//
// Return all segments back to the disk
//
//
void ReturnSegments();

//
// Load all directories and files from the reserved segment
//
//
ErrorCodeT LoadDirectoriesAndFiles();

//
// Synchronize a directory to the disk
//
//
inline ErrorCodeT SyncDirToDisk(DPUDir* Dir);

//
// Synchronize a file to the disk
//
//
inline ErrorCodeT SyncFileToDisk(DPUFile* Dir);

//
// Synchronize the first sector on the reserved segment
//
//
inline ErrorCodeT SyncReservedInformationToDisk();

DPUStorage BackEndStorage();

//
// Initialize the backend service
//
//
ErrorCodeT Initialize();

//
// Create a diretory
// Assuming id and parent id have been computed by host
 // 
//
ErrorCodeT CreateDirectory(
    const char* PathName,
    DirIdT DirId,
    DirIdT ParentId
);

//
// Delete a directory
// 
//
ErrorCodeT RemoveDirectory(
    DirIdT DirId
);

//
// Create a file
// 
//
ErrorCodeT CreateFile(
    const char* FileName,
    FileAttributesT FileAttributes,
    FileIdT FileId,
    DirIdT DirId
);

//
// Delete a file
// 
//
ErrorCodeT DeleteFile(
    FileIdT FileId,
    DirIdT DirId
);

//
// Change the size of a file
// 
//
ErrorCodeT ChangeFileSize(
    FileIdT FileId,
    FileSizeT NewSize
);

//
// Get file size
// 
//
ErrorCodeT GetFileSize(
    FileIdT FileId,
    FileSizeT* FileSize
);


//
// Async read from a file
// 
//
ErrorCodeT ReadFile(
    FileIdT FileId,
    FileSizeT Offset,
    BufferT DestBuffer,
    FileIOSizeT BytesToRead,
    DiskIOCallback Callback,
    ContextT Context
);

//
// Async write to a file
// 
//
ErrorCodeT WriteFile(
    FileIdT FileId,
    FileSizeT Offset,
    BufferT SourceBuffer,
    FileIOSizeT BytesToWrite,
    DiskIOCallback Callback,
    ContextT Context
);

//
// Get file properties by file Id
// 
//
ErrorCodeT GetFileInformationById(
    FileIdT FileId,
    FilePropertiesT* FileProperties
);

//
// Get file attributes by file name
// 
//
ErrorCodeT
GetFileAttributes(
    FileIdT FileId,
    FileAttributesT* FileAttributes
);

//
// Get the size of free space on the storage
// 
//
ErrorCodeT GetStorageFreeSpace(
    FileSizeT* StorageFreeSpace
);

//
// Move an existing file or a directory,
// including its children
// 
//
ErrorCodeT MoveFile(
    FileIdT FileId,
    DirIdT OldDirId,
    DirIdT NewDirId,
     const char* NewFileName
);