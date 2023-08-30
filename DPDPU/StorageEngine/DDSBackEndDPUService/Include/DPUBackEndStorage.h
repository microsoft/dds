#pragma once

// #include <atomic>
#include <stdatomic.h>
// #include <mutex>
#include <pthread.h>
#include <time.h>
#include <stdbool.h>

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
    //remove const before SegmentIdT
    SegmentIdT TotalSegments;
    SegmentIdT AvailableSegments;
    
    struct DPUDir* AllDirs[DDS_MAX_DIRS];
    struct DPUFile* AllFiles[DDS_MAX_FILES];
    int TotalDirs;
    int TotalFiles;

    pthread_mutex_t SectorModificationMutex;
    pthread_mutex_t SegmentAllocationMutex;
    
    atomic_size_t TargetProgress;
    atomic_size_t CurrentProgress;
};

struct DPUStorage* BackEndStorage();

void DeBackEndStorage(struct DPUStorage* Sto);

//
// Read from disk synchronously
//
//
ErrorCodeT ReadFromDiskSync(
    BufferT DstBuffer,
    SegmentIdT SegmentId,
    SegmentSizeT SegmentOffset,
    FileIOSizeT Bytes,
    struct DPUStorage* Sto,
    void *arg
);

//
// Write from disk synchronously
//
//
ErrorCodeT WriteToDiskSync(
    BufferT SrcBuffer,
    SegmentIdT SegmentId,
    SegmentSizeT SegmentOffset,
    FileIOSizeT Bytes,
    struct DPUStorage* Sto,
    void *arg
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
    ContextT Context,
    struct DPUStorage* Sto,
    void *arg
);

//
// Write from disk asynchronously
//
//
ErrorCodeT WriteToDiskAsync(
    BufferT SrcBuffer,
    SegmentIdT SegmentId,
    SegmentSizeT SegmentOffset,
    FileIOSizeT Bytes,
    DiskIOCallback Callback,
    ContextT Context,
    struct DPUStorage* Sto,
    void *arg
);

//
// Retrieve all segments on the disk
//
//
ErrorCodeT RetrieveSegments(struct DPUStorage* Sto);

//
// Return all segments back to the disk
//
//
void ReturnSegments(struct DPUStorage* Sto);

//
// Load all directories and files from the reserved segment
//
//
ErrorCodeT LoadDirectoriesAndFiles(
    struct DPUStorage* Sto,
    void *arg
);

//
// Synchronize a directory to the disk
//
//
inline ErrorCodeT SyncDirToDisk(
    struct DPUDir* Dir, 
    struct DPUStorage* Sto,
    void *arg
);

//
// Synchronize a file to the disk
//
//
inline ErrorCodeT SyncFileToDisk(
    struct DPUFile* File,
    struct DPUStorage* Sto,
    void *arg
);

//
// Synchronize the first sector on the reserved segment
//
//
inline ErrorCodeT SyncReservedInformationToDisk(
    struct DPUStorage* Sto,
    void *arg
);


//
// Initialize the backend service
//
//
ErrorCodeT Initialize(
    struct DPUStorage* Sto,
    void *arg
);

//
// Create a diretory
// Assuming id and parent id have been computed by host
// 
//
ErrorCodeT CreateDirectory(
    const char* PathName,
    DirIdT DirId,
    DirIdT ParentId,
    struct DPUStorage* Sto,
    void *arg
);

//
// Delete a directory
// 
//
ErrorCodeT RemoveDirectory(
    DirIdT DirId,
    struct DPUStorage* Sto,
    void *arg
);

//
// Create a file
// 
//
ErrorCodeT CreateFile(
    const char* FileName,
    FileAttributesT FileAttributes,
    FileIdT FileId,
    DirIdT DirId,
    struct DPUStorage* Sto,
    void *arg
);

//
// Delete a file
// 
//
ErrorCodeT DeleteFileOnSto(
    FileIdT FileId,
    DirIdT DirId,
    struct DPUStorage* Sto,
    void *arg
);

//
// Change the size of a file
// 
//
ErrorCodeT ChangeFileSize(
    FileIdT FileId,
    FileSizeT NewSize,
    struct DPUStorage* Sto
);

//
// Get file size
// 
//
ErrorCodeT GetFileSize(
    FileIdT FileId,
    FileSizeT* FileSize,
    struct DPUStorage* Sto
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
    ContextT Context,
    struct DPUStorage* Sto,
    void *arg
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
    ContextT Context,
    struct DPUStorage* Sto,
    void *arg
);

//
// Get file properties by file id
// 
//
ErrorCodeT GetFileInformationById(
    FileIdT FileId,
    FilePropertiesT* FileProperties,
    struct DPUStorage* Sto
);

//
// Get file attributes by file name
// 
//
ErrorCodeT
GetFileAttributes(
    FileIdT FileId,
    FileAttributesT* FileAttributes,
    struct DPUStorage* Sto
);

//
// Get the size of free space on the storage
// 
//
ErrorCodeT GetStorageFreeSpace(
    FileSizeT* StorageFreeSpace,
    struct DPUStorage* Sto
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
    const char* NewFileName,
    struct DPUStorage* Sto,
    void *arg
);