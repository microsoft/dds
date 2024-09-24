/*
 * Copyright (c) Microsoft Corporation.
 * Licensed under the MIT License
 */

#pragma once

#include <string.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <pthread.h>
#include <time.h>
#include <stdbool.h>

#include "MsgTypes.h"
#include "DPUBackEndDir.h"
#include "ControlPlaneHandler.h"
#include "DPUBackEndFile.h"
#include "bdev.h"

//
// Used to manage the status of each slot in SPDK buffer
//
//
struct PerSlotContext{
    int Position;
    bool Available;  // unused now
    struct iovec Iov[2];
    SplittableBufferT *DestBuffer;
    // enables batching, spdk send msg takes 1 of this, checks this value, and handles all in the batch
    // e.g. if there are 4 in a batch, the first should have value 4, the last 1.
    RequestIdT BatchSize;
    RequestIdT IndexBase;
    SPDKContextT *SPDKContext;  // thread specific SPDKContext
    DataPlaneRequestContext *Ctx;
    atomic_ushort CallbacksToRun;
    atomic_ushort CallbacksRan;
    FileIOSizeT BytesIssued;
};

//
// Context for each Write I/O operation in the back end, same with BackEndIOContext
// Reason we need this: we may actually need multiple async bdev writes to fulfil the request
// request is fulfilled when BytesIssued == BytesCompleted; otherwise if failure happens,
// we should already be setting the response to fail outside the callback
// instead of tracking bytes all completed, might as well just track no. of writes all completed
//
//
typedef struct BackEndIOContext {
    //
    // Per slot info
    //
    //
    int Position;
    bool IsAvailable;
    
    //
    // Callback specific info
    //
    //
    atomic_ushort CallbacksToRun;
    atomic_ushort CallbacksRan;
    FileIOSizeT BytesIssued;
    bool IsRead;
} BackEndIOContextT;

//
// Callback for async disk operations
//
//
typedef void (*DiskIOCallback)(
    struct spdk_bdev_io *bdev_io,
    bool Success,
    ContextT Context
);

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
    SPDKContextT *SPDKContext;
    char *tmpSectorBuf;  // actually the buff that stores the reserved segment
    char *tmpPageBuf;  // zeroed page to write
    atomic_size_t TargetProgress;
    atomic_size_t CurrentProgress;
    size_t PagesLeft;
    size_t NumPagesWritten;
    struct DPUDir* RootDir;
    struct InitializeFailureStatus *FailureStatus;
};

//
// Read from disk asynchronously
//
//
ErrorCodeT ReadFromDiskAsyncZC(
    BufferT DstBuffer,
    SegmentIdT SegmentId,
    SegmentSizeT SegmentOffset,
    FileIOSizeT Bytes,
    DiskIOCallback Callback,
    ContextT Context,
    struct DPUStorage* Sto,
    void *SPDKContext
);

ErrorCodeT ReadFromDiskAsyncNonZC(
    FileIOSizeT NonZCBuffOffset,  // how many we already read
    SegmentIdT SegmentId,
    SegmentSizeT SegmentOffset,
    FileIOSizeT Bytes,
    DiskIOCallback Callback,
    struct PerSlotContext *SlotContext,
    struct DPUStorage* Sto,
    void *SPDKContext
);

ErrorCodeT ReadvFromDiskAsyncZC(
    struct iovec *Iov,
    int iovcnt,
    SegmentIdT SegmentId,
    SegmentSizeT SegmentOffset,
    FileIOSizeT Bytes,
    DiskIOCallback Callback,
    ContextT Context,
    struct DPUStorage* Sto,
    void *SPDKContext
);

ErrorCodeT ReadvFromDiskAsyncNonZC(
    struct iovec *Iov,
    int iovcnt,
    FileIOSizeT NonZCBuffOffset,  // how many we already read
    SegmentIdT SegmentId,
    SegmentSizeT SegmentOffset,
    FileIOSizeT Bytes,
    DiskIOCallback Callback,
    struct PerSlotContext *SlotContext,
    struct DPUStorage* Sto,
    void *SPDKContext
);

//
// Write to disk asynchronously, may be called by all, e.g. init, data plane and control plane etc.
//
//
ErrorCodeT WriteToDiskAsyncZC(
    BufferT SrcBuffer,
    SegmentIdT SegmentId,
    SegmentSizeT SegmentOffset,
    FileIOSizeT Bytes,
    DiskIOCallback Callback,
    ContextT Context,
    struct DPUStorage* Sto,
    void *Arg
);

//
// This (Non ZC) should only be called in data plane, i.e. other calls must be zero copy (direct IO)
// the context is therefore only per slot ctx and nothing else
//
//
ErrorCodeT WriteToDiskAsyncNonZC(
    BufferT SrcBuffer,
    FileIOSizeT NonZCBuffOffset,  // how many we already read
    SegmentIdT SegmentId,
    SegmentSizeT SegmentOffset,
    FileIOSizeT Bytes,
    DiskIOCallback Callback,
    struct PerSlotContext *SlotContext,
    struct DPUStorage* Sto,
    void *Arg
);

ErrorCodeT WritevToDiskAsyncZC(
    struct iovec *Iov,
    int iovcnt,
    SegmentIdT SegmentId,
    SegmentSizeT SegmentOffset,
    FileIOSizeT Bytes,
    DiskIOCallback Callback,
    struct PerSlotContext *SlotContext,
    struct DPUStorage* Sto,
    void *SPDKContext
);

ErrorCodeT WritevToDiskAsyncNonZC(
    struct iovec *Iov,
    int iovcnt,
    FileIOSizeT NonZCBuffOffset,  // how many we already read
    SegmentIdT SegmentId,
    SegmentSizeT SegmentOffset,
    FileIOSizeT Bytes,
    DiskIOCallback Callback,
    struct PerSlotContext *SlotContext,
    struct DPUStorage* Sto,
    void *SPDKContext
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
    void *Arg,
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
    void *Arg
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
    char *TmpSectorBuffer;  // the reserved segment is read into this
    size_t DirLoopIndex;
    size_t FileLoopIndex;
    DPUDirPropertiesT *DirOnDisk;
    DPUFilePropertiesT *FileOnDisk;
    atomic_int *LoadedDirs;
    atomic_int *LoadedFiles;
    struct InitializeFailureStatus *FailureStatus;  // points to the member in struct InitializeCtx
    SPDKContextT *SPDKContext;
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
    FilePropertiesT* FileProperties,
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
