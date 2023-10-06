#pragma once

// #include <atomic>
#include <string.h>
#include <stdlib.h>
#include <stdatomic.h>
// #include <mutex>
#include <pthread.h>
#include <time.h>
#include <stdbool.h>

#include "MsgTypes.h"
// #include "FileBackEnd.h"  // circular include here: FileService - DPUBackEndStorage - FileBackEnd - FileService
#include "DPUBackEndDir.h"
#include "ControlPlaneHandler.h"
#include "DPUBackEndFile.h"
#include "bdev.h"
// #include "Zmalloc.h"



//
// Used to manage the status of each slot in SPDK buffer
//
//
struct PerSlotContext{
    int Position;
    bool Available;  // should be unused now
    SPDKContextT *SPDKContext;  // thread specific SPDKContext
    DataPlaneRequestContext *Ctx;
    atomic_ushort CallbacksToRun;
    atomic_ushort CallbacksRan;
    FileIOSizeT BytesIssued;
    // bool IsRead;  // unused??
};

//
// Context for each Write I/O operation in the back end, same with BackEndIOContext
// Reason we need this: we may actually need multiple async bdev writes to fulfil the request
// request is fulfilled when BytesIssued == BytesCompleted; otherwise if failure happens,
// we should already be setting the response to fail outside the callback
// TODO: instead of tracking bytes all completed, might as well just track no. of writes all completed
//
//

typedef struct BackEndIOContext {
    /* DataPlaneRequestContext* RequestContext;  // now in request context */
    // per slot info
    int Position;
    bool IsAvailable;
    
    // callback specific info
    atomic_ushort CallbacksToRun;
    atomic_ushort CallbacksRan;
    FileIOSizeT BytesIssued; // equals request's no. of bytes
    /* SplittableBufferT *SplittableBuffer;  // free this in last callback, but now in request context */
    bool IsRead;
    /* BuffMsgB2FAckHeader *Resp  // the actual response, mutated in the callback, but now in RequestContext */
} BackEndIOContextT;
/* typedef struct BackEndIOContext {
    ContextT FrontEndBufferContext;
    ContextT BackEndBufferContext;
    atomic_ushort CallbacksToRun;
    atomic_ushort CallbacksRan;
    FileIOSizeT BytesIssued; // equals request's no. of bytes
    SplittableBufferT *SplittableBuffer;  // free this in last callback
    // atomic_ulong BytesCompleted;  // unused; TODO: might not need atomic, handler even with multi writes should be on same thread
    // atomic_bool IsAvailable;  // unused
    // ErrorCodeT ErrorCode;  // TODO: do we need this? resp has result anyway
    bool IsRead;
    BuffMsgB2FAckHeader *Resp  // the actual response, mutated in the callback
} BackEndIOContextT; */

//
// Callback for async disk operations
//
//
typedef void (*DiskIOCallback)(
    struct spdk_bdev_io *bdev_io,
    bool Success,
    ContextT Context
);

/* 
//
// DPU storage, highly similar with BackEndStorage
//
//
struct DPUStorage {
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
    
    //
    // these are used during `Initialize`, move them to their own context
    //
    //

    // atomic_size_t TargetProgress;
    // atomic_size_t CurrentProgress;
};

//
// DPU Storage var should be a global singleton
//
//
extern struct DPUStorage *Sto; */

struct DPUStorage* BackEndStorage();

void DeBackEndStorage(struct DPUStorage* Sto);


//
// Context used when Initializing Storage
//
//
struct InitializeCtx {
    //
    // This is the master, which holds the definitive HasFailed/Aborted/Stopped values, and have Master = NULL
    // When there is looped IO, copies (with some different values) will be created, but they should have ptr to Master
    // and set HasFailed in Master etc.
    //
    //
    // struct InitializeCtx *Master;

    SPDKContextT *SPDKContext;
    char *tmpSectorBuf;  // actually the buff that stores the reserved segment
    atomic_size_t TargetProgress;
    atomic_size_t CurrentProgress;
    size_t PagesLeft;
    size_t NumPagesWritten;
    struct DPUDir* RootDir;
    struct InitializeFailureStatus *FailureStatus;
    //
    // this will be set if some callback during the progress has callbacks run with failed param
    //
    //
    // atomic_bool HasFailed;
    //
    // already fatal, some callback won't run, don't need to run other callbacks anymore; will stop spdk app
    //
    //
    // atomic_bool HasAborted;
    //
    // The first callback to find HasAborted == true will stop SPDK app, and set HasStopped = true;
    // other callbacks should check this so they don't stop again
    //
    //
    // atomic_bool HasStopped;
};

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
    void *arg,
    bool ZeroCopy
);

void ReadFromDiskSyncCallback(
    struct spdk_bdev_io *bdev_io,
    bool success,
    void *cb_arg
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
    void *arg,
    bool ZeroCopy
);

ErrorCodeT WriteToDiskSyncCallback(
    struct spdk_bdev_io *bdev_io,
    bool success,
    void *cb_arg
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
    void *arg,
    bool ZeroCopy //true for using zero copy, false otherwise
);

//
// Write to disk asynchronously
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
    void *arg,
    bool ZeroCopy //true for using zero copy, false otherwise
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
    void *arg,
    struct InitializeCtx *InitializeCtx
);

//
// Synchronize a directory to the disk
//
//
ErrorCodeT SyncDirToDisk(
    struct DPUDir* Dir, 
    struct DPUStorage* Sto,
    void *SPDKContext,
    DiskIOCallback Callback,
    ContextT Context
);

//
// Synchronize a file to the disk
//
//
ErrorCodeT SyncFileToDisk(
    struct DPUFile* File,
    struct DPUStorage* Sto,
    void *SPDKContext,
    DiskIOCallback Callback,
    ContextT Context
);

//
// Synchronize the first sector on the reserved segment
//
//
ErrorCodeT SyncReservedInformationToDisk(
    struct DPUStorage* Sto,
    void *SPDKContext,
    DiskIOCallback Callback,
    ContextT Context
);


//
// Initialize the backend service
//
//
ErrorCodeT Initialize(
    struct DPUStorage* Sto,
    void *arg
);

struct InitializeFailureStatus {
    atomic_bool HasFailed;
    atomic_bool HasAborted;
    atomic_bool HasStopped;
};


//
// Ctx used during LoadDirectoriesAndFiles(), which is during Initialize()
//
//
struct LoadDirectoriesAndFilesCtx {
    // SegmentSizeT NextAddress;
    char *TmpSectorBuffer;  // the reserved segment is read into this
    size_t DirLoopIndex;
    size_t FileLoopIndex;
    DPUDirPropertiesT *DirOnDisk;
    DPUFilePropertiesT *FileOnDisk;
    atomic_int *LoadedDirs;
    atomic_int *LoadedFiles;
    struct InitializeFailureStatus *FailureStatus;  // points to the member in struct InitializeCtx
    SPDKContextT *SPDKContext;
    /* atomic_bool HasFailed;
    atomic_bool HasAborted;
    atomic_bool *HasStopped; */
};

//
// Create a directory
// Assuming id and parent id have been computed by host
// 
//
ErrorCodeT CreateDirectory(
    const char* PathName,
    DirIdT DirId,
    DirIdT ParentId,
    struct DPUStorage* Sto,
    void *SPDKContext,
    ControlPlaneHandlerCtx *HandlerCtx
);

//
// Delete a directory
// 
//
ErrorCodeT RemoveDirectory(
    DirIdT DirId,
    struct DPUStorage* Sto,
    void *SPDKContext,
    ControlPlaneHandlerCtx *HandlerCtx
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
    void *SPDKContext,
    ControlPlaneHandlerCtx *HandlerCtx
);

//
// Delete a file
// 
//
ErrorCodeT DeleteFileOnSto(
    FileIdT FileId,
    DirIdT DirId,
    struct DPUStorage* Sto,
    void *SPDKContext,
    ControlPlaneHandlerCtx *HandlerCtx
);

//
// Change the size of a file
// 
//
ErrorCodeT ChangeFileSize(
    FileIdT FileId,
    FileSizeT NewSize,
    struct DPUStorage* Sto,
    CtrlMsgB2FAckChangeFileSize *Resp
);

//
// Get file size
// 
//
ErrorCodeT GetFileSize(
    FileIdT FileId,
    FileSizeT* FileSize,
    struct DPUStorage* Sto,
    CtrlMsgB2FAckGetFileSize *Resp
);


//
// Async read from a file
// 
//
ErrorCodeT ReadFile(
    FileIdT FileId,
    FileSizeT Offset,
    SplittableBufferT *SourceBuffer,
    DiskIOCallback Callback,
    ContextT Context,
    struct DPUStorage* Sto,
    void *SPDKContext
);

//
// Async write to a file
// 
//
ErrorCodeT WriteFile(
    FileIdT FileId,
    FileSizeT Offset,
    SplittableBufferT *SourceBuffer,
    DiskIOCallback Callback,
    ContextT Context,
    struct DPUStorage* Sto,
    void *SPDKContext
);

//
// Get file properties by file id
// 
//
ErrorCodeT GetFileInformationById(
    FileIdT FileId,
    DDSFilePropertiesT* FileProperties,
    struct DPUStorage* Sto,
    CtrlMsgB2FAckGetFileInfo *Resp
);

//
// Get file attributes by file name
// 
//
ErrorCodeT
GetFileAttributes(
    FileIdT FileId,
    FileAttributesT* FileAttributes,
    struct DPUStorage* Sto,
    CtrlMsgB2FAckGetFileAttr *Resp
);

//
// Get the size of free space on the storage
// 
//
ErrorCodeT GetStorageFreeSpace(
    FileSizeT* StorageFreeSpace,
    struct DPUStorage* Sto,
    CtrlMsgB2FAckGetFreeSpace *Resp
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
    void *SPDKContext,
    ControlPlaneHandlerCtx *HandlerCtx
);

//
// For responding result/error with RDMA, such as when IO failed in callback and/or can't continue situations
// e.g. Result = DDS_ERROR_CODE_OUT_OF_MEMORY, MsgId = CTRL_MSG_B2F_ACK_CREATE_DIR
//
//
ErrorCodeT RespondWithResult(
    ControlPlaneHandlerCtx *HandlerCtx,
    int MsgId,
    ErrorCodeT Result
);