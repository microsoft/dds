#include "DPUBackEndStorage.h"



// struct DPUStorage* Sto;
// SPDKContextT *SPDKContext;

//
// Constructor
// 
//
struct DPUStorage* BackEndStorage(){
    struct DPUStorage *tmp;
    tmp = malloc(sizeof(struct DPUStorage));
    tmp->AllSegments = NULL;
    tmp->AvailableSegments = 0;

    for (size_t d = 0; d != DDS_MAX_DIRS; d++) {
        tmp->AllDirs[d] = NULL;
    }

    for (size_t f = 0; f != DDS_MAX_FILES; f++) {
        tmp->AllFiles[f] = NULL;
    }

    tmp->TotalDirs = 0;
    tmp->TotalFiles = 0;

    tmp->TotalSegments = DDS_BACKEND_CAPACITY / DDS_BACKEND_SEGMENT_SIZE;
    pthread_mutex_init(&tmp->SectorModificationMutex, NULL);
    pthread_mutex_init(&tmp->SegmentAllocationMutex, NULL);
    return tmp;
}

//
// Destructor
// 
//
void DeBackEndStorage(
    struct DPUStorage* Sto
){
    //
    // Free all bufffers
    //
    //
    for (size_t d = 0; d != DDS_MAX_DIRS; d++) {
        if (Sto->AllDirs[d] != NULL) {
            free(Sto->AllDirs[d]);
        }
    }

    for (size_t f = 0; f != DDS_MAX_FILES; f++) {
        if (Sto->AllFiles[f] != NULL) {
            free(Sto->AllFiles[f]);
        }
    }

    //
    // Return all segments
    //
    //
    ReturnSegments(Sto);
}

ErrorCodeT ReadvFromDiskAsyncZC(
    struct iovec *Iov,
    int IovCnt,
    SegmentIdT SegmentId,
    SegmentSizeT SegmentOffset,
    FileIOSizeT Bytes,
    DiskIOCallback Callback,
    ContextT Context,
    struct DPUStorage* Sto,
    void *SPDKContext
) {
    SegmentT* seg = &Sto->AllSegments[SegmentId];
    int rc;
    rc = bdev_readv(SPDKContext, Iov, IovCnt, seg->DiskAddress + SegmentOffset, Bytes, 
        Callback, Context);

    if (rc)
    {
        printf("ReadFromDiskAsync called bdev_read(), but failed with: %d\n", rc);
        return DDS_ERROR_CODE_INVALID_FILE_POSITION;  // there should be an SPDK error log before this
    }

    return DDS_ERROR_CODE_SUCCESS; // since it's async, we don't know if the actual read will be successful
}

ErrorCodeT ReadvFromDiskAsyncNonZC(
    struct iovec *Iov,
    int IovCnt,
    FileIOSizeT NonZCBuffOffset,
    SegmentIdT SegmentId,
    SegmentSizeT SegmentOffset,
    FileIOSizeT Bytes,
    DiskIOCallback Callback,
    struct PerSlotContext *SlotContext,
    struct DPUStorage* Sto,
    void *SPDKContext
) {
    SegmentT* seg = &Sto->AllSegments[SegmentId];
    int rc;
    int position = SlotContext->Position;
    SPDKContextT *arg = SPDKContext;
    if (Bytes > DDS_BACKEND_SPDK_BUFF_BLOCK_SPACE) {
        SPDK_WARNLOG("A read with %d bytes exceeds buff block space: %d\n", Bytes, DDS_BACKEND_SPDK_BUFF_BLOCK_SPACE);
    }

    rc = bdev_read(SPDKContext, (&arg->buff[position * DDS_BACKEND_SPDK_BUFF_BLOCK_SPACE]), 
    seg->DiskAddress + SegmentOffset, Bytes, Callback, SlotContext);

    if (rc)
    {
        printf("ReadFromDiskAsync called bdev_read(), but failed with: %d\n", rc);
        return DDS_ERROR_CODE_INVALID_FILE_POSITION;  // there should be an SPDK error log before this
    }

    return DDS_ERROR_CODE_SUCCESS; // since it's async, we don't know if the actual read will be successful
}


ErrorCodeT ReadFromDiskAsyncZC(
    BufferT DstBuffer,
    SegmentIdT SegmentId,
    SegmentSizeT SegmentOffset,
    FileIOSizeT Bytes,
    DiskIOCallback Callback,
    ContextT Context,
    struct DPUStorage* Sto,
    void *SPDKContext
){
    SegmentT* seg = &Sto->AllSegments[SegmentId];
    int rc;
    rc = bdev_read(SPDKContext, DstBuffer, seg->DiskAddress + SegmentOffset, Bytes, 
        Callback, Context);

    if (rc)
    {
        printf("ReadFromDiskAsync called bdev_read(), but failed with: %d\n", rc);
        return DDS_ERROR_CODE_INVALID_FILE_POSITION;  // there should be an SPDK error log before this
    }

    return DDS_ERROR_CODE_SUCCESS; // since it's async, we don't know if the actual read will be successful
}

//
// Read from disk asynchronously, FileIOSizeT BytesRead is how many we already read, only used for non zero copy
//
//
ErrorCodeT ReadFromDiskAsyncNonZC(
    FileIOSizeT NonZCBuffOffset,  // how many we already read
    SegmentIdT SegmentId,
    SegmentSizeT SegmentOffset,
    FileIOSizeT Bytes,
    DiskIOCallback Callback,
    struct PerSlotContext *SlotContext,
    struct DPUStorage* Sto,
    void *SPDKContext
){
    SegmentT* seg = &Sto->AllSegments[SegmentId];
    int rc;
    int position = SlotContext->Position;
    SPDKContextT *arg = SPDKContext;
    if (Bytes > DDS_BACKEND_SPDK_BUFF_BLOCK_SPACE) {
        SPDK_WARNLOG("A read with %d bytes exceeds buff block space: %d\n", Bytes, DDS_BACKEND_SPDK_BUFF_BLOCK_SPACE);
    }
    rc = bdev_read(SPDKContext, (&arg->buff[position * DDS_BACKEND_SPDK_BUFF_BLOCK_SPACE]) + NonZCBuffOffset, 
    seg->DiskAddress + SegmentOffset, Bytes, Callback, SlotContext);

    if (rc)
    {
        printf("ReadFromDiskAsync called bdev_read(), but failed with: %d\n", rc);
        return DDS_ERROR_CODE_INVALID_FILE_POSITION;  // there should be an SPDK error log before this
    }

    return DDS_ERROR_CODE_SUCCESS; // since it's async, we don't know if the actual read will be successful
}


ErrorCodeT WritevToDiskAsyncZC(
    struct iovec *Iov,
    int IovCnt,
    SegmentIdT SegmentId,
    SegmentSizeT SegmentOffset,
    FileIOSizeT Bytes,
    DiskIOCallback Callback,
    struct PerSlotContext *SlotContext,  // this should be the callback arg
    struct DPUStorage* Sto,
    void *SPDKContext
){
    SegmentT* seg = &Sto->AllSegments[SegmentId];
    int rc;
    
    rc = bdev_writev(SPDKContext, Iov, IovCnt, seg->DiskAddress + SegmentOffset, Bytes, 
        Callback, SlotContext);

    if (rc)
    {
        printf("WriteToDiskAsync called bdev_write(), but failed with: %d\n", rc);
        return rc;  // there should be an SPDK error log before this?
    }

    return DDS_ERROR_CODE_SUCCESS;
}

ErrorCodeT WritevToDiskAsyncNonZC(
    struct iovec *Iov,
    int IovCnt,
    FileIOSizeT NonZCBuffOffset,  // how many we already read
    SegmentIdT SegmentId,
    SegmentSizeT SegmentOffset,
    FileIOSizeT Bytes,
    DiskIOCallback Callback,
    struct PerSlotContext *SlotContext,  // this should be the callback arg
    struct DPUStorage* Sto,
    void *SPDKContext
){
    SegmentT* seg = &Sto->AllSegments[SegmentId];
    int rc;

    int position = SlotContext->Position;
    SPDKContextT *arg = SPDKContext;
    if (Bytes > DDS_BACKEND_SPDK_BUFF_BLOCK_SPACE) {
        SPDK_WARNLOG("A write with %d bytes exceeds buff block space: %d\n", Bytes, DDS_BACKEND_SPDK_BUFF_BLOCK_SPACE);
    }
    char *toCopy = (&arg->buff[position * DDS_BACKEND_SPDK_BUFF_BLOCK_SPACE]) + NonZCBuffOffset;
    memcpy(toCopy, Iov[0].iov_base, Iov[0].iov_len);
    memcpy(toCopy + Iov[0].iov_len, Iov[1].iov_base, Iov[1].iov_len);
    rc = bdev_write(SPDKContext, toCopy,
        seg->DiskAddress + SegmentOffset, Bytes, Callback, SlotContext);

    if (rc)
    {
        printf("WriteToDiskAsync called bdev_write(), but failed with: %d\n", rc);
        return rc;  // there should be an SPDK error log before this?
    }

    return DDS_ERROR_CODE_SUCCESS;
}

//
// Write to disk asynchronously with zero copy
//
//
ErrorCodeT WriteToDiskAsyncZC(
    BufferT SrcBuffer,
    SegmentIdT SegmentId,
    SegmentSizeT SegmentOffset,
    FileIOSizeT Bytes,
    DiskIOCallback Callback,
    ContextT Context,  // this should be the callback arg
    struct DPUStorage* Sto,
    void *SPDKContext
){
    SegmentT* seg = &Sto->AllSegments[SegmentId];
    int rc;

    rc = bdev_write(SPDKContext, SrcBuffer, seg->DiskAddress + SegmentOffset, Bytes, 
        Callback, Context);

    if (rc)
    {
        printf("WriteToDiskAsync called bdev_write(), but failed with: %d\n", rc);
        return rc;  // there should be an SPDK error log before this?
    }

    return DDS_ERROR_CODE_SUCCESS;
}

ErrorCodeT WriteToDiskAsyncNonZC(
    BufferT SrcBuffer,
    FileIOSizeT NonZCBuffOffset,  // how many we've already written (i.e. the progress)
    SegmentIdT SegmentId,
    SegmentSizeT SegmentOffset,
    FileIOSizeT Bytes,
    DiskIOCallback Callback,
    // ContextT Context,  // this should be the callback arg
    struct PerSlotContext *SlotContext,
    struct DPUStorage* Sto,
    void *SPDKContext
){
    SegmentT* seg = &Sto->AllSegments[SegmentId];
    int rc;
    
    int position = SlotContext->Position;
    SPDKContextT *arg = SPDKContext;
    if (Bytes > DDS_BACKEND_SPDK_BUFF_BLOCK_SPACE) {
        SPDK_WARNLOG("A write with %d bytes exceeds buff block space: %d\n", Bytes, DDS_BACKEND_SPDK_BUFF_BLOCK_SPACE);
    }

    // copy into our own buffer, then write
    char *toCopy = (&arg->buff[position * DDS_BACKEND_SPDK_BUFF_BLOCK_SPACE]) + NonZCBuffOffset;
    memcpy(toCopy, SrcBuffer, Bytes);
    rc = bdev_write(SPDKContext, toCopy, 
        seg->DiskAddress + SegmentOffset, Bytes, Callback, SlotContext);

    if (rc)
    {
        printf("WriteToDiskAsync called bdev_write(), but failed with: %d\n", rc);
        return rc;  // there should be an SPDK error log before this?
    }

    return DDS_ERROR_CODE_SUCCESS;
}

//
// Retrieve all segments on the disk, replace all new... by malloc() inside
// This initializes Sto->AllSegments
//
//
ErrorCodeT RetrieveSegments(
    struct DPUStorage* Sto
){
    //
    // We should get data from the disk,
    // but just create all segments using memory for now
    //
    //
    Sto->AllSegments = malloc(sizeof(SegmentT) * Sto->TotalSegments);

    for (SegmentIdT i = 0; i != Sto->TotalSegments; i++) {
        Sto->AllSegments[i].Id = i;
        Sto->AllSegments[i].FileId = DDS_FILE_INVALID;
        
        //
        // We dont allocate memory to newSegment. Instead, we calculate 
        // the start position of each segment by i*DDS_BACKEND_SEGMENT_SIZE
        // and use this position as disk address
        //
        //
        DiskSizeT newSegment = i * DDS_BACKEND_SEGMENT_SIZE;

        Sto->AllSegments[i].DiskAddress = newSegment;
        
        if (i == DDS_BACKEND_RESERVED_SEGMENT) {
            Sto->AllSegments[i].Allocatable = false;
        }
        else {
            Sto->AllSegments[i].Allocatable = true;
            Sto->AvailableSegments++;
        }
    }

    return DDS_ERROR_CODE_SUCCESS;
}

//
// Return all segments back to the disk
//
//
void ReturnSegments(
    struct DPUStorage* Sto
){
    //
    // Release the memory buffers
    //
    //
    if (Sto->AllSegments) {
        for (size_t i = 0; i != Sto->TotalSegments; i++) {

            if (Sto->AllSegments[i].Allocatable) {
                Sto->AvailableSegments--;
            }
        }

        free(Sto->AllSegments);
    }
}

void LoadFilesCallback(
    struct spdk_bdev_io *bdev_io,
    bool Success,
    ContextT Context
) {
    struct LoadDirectoriesAndFilesCtx *Ctx = Context;


    if (Ctx->FileOnDisk->Id != DDS_DIR_INVALID) {
        SPDK_NOTICELOG("Initializing file with ID %hu\n", Ctx->FileOnDisk->Id);
        Sto->AllFiles[Ctx->FileLoopIndex] =
            BackEndFileI(Ctx->FileOnDisk->Id, Ctx->FileOnDisk->FProperties.FileName, Ctx->FileOnDisk->FProperties.FileAttributes);
        if (Sto->AllFiles[Ctx->FileLoopIndex]) {
            memcpy(GetFileProperties(Sto->AllFiles[Ctx->FileLoopIndex]), Ctx->FileOnDisk, sizeof(DPUFilePropertiesT));
            SetNumAllocatedSegments(Sto->AllFiles[Ctx->FileLoopIndex]);
        }
        else {
            Ctx->FailureStatus->HasFailed = true;
            Ctx->FailureStatus->HasAborted = true;
            // TODO: call FS app stop func
        }

        *(Ctx->LoadedFiles)++;
        if (*(Ctx->LoadedFiles) == Sto->TotalFiles) {
            SPDK_NOTICELOG("Loaded all files, total: %d\n", Sto->TotalFiles);
        }
    }
    free(Ctx->FileOnDisk);
    free(Ctx);

    if (Ctx->FileLoopIndex == DDS_MAX_FILES - 1) {
        return;
    }
}

void LoadDirectoriesCallback(
    struct spdk_bdev_io *bdev_io,
    bool Success,
    ContextT Context
) {
    struct LoadDirectoriesAndFilesCtx *Ctx = Context;
    SPDKContextT *SPDKContext = Ctx->SPDKContext;

    if (Ctx->DirOnDisk->Id != DDS_DIR_INVALID) {
        SPDK_NOTICELOG("Initializing Dir with ID: %hu\n", Ctx->DirOnDisk->Id);
        Sto->AllDirs[Ctx->DirLoopIndex] = BackEndDirI(Ctx->DirOnDisk->Id, DDS_DIR_ROOT, Ctx->DirOnDisk->Name);
        if (Sto->AllDirs[Ctx->DirLoopIndex]) {
            memcpy(GetDirProperties(Sto->AllDirs[Ctx->DirLoopIndex]), Ctx->DirOnDisk, sizeof(DPUDirPropertiesT));
        }
        else {
            //
            // This should be very unlikely
            //
            //
            // return DDS_ERROR_CODE_OUT_OF_MEMORY;
            // TODO: call app stop func
        }

        *(Ctx->LoadedDirs) += 1;
        if (*(Ctx->LoadedDirs) == Sto->TotalDirs) {
            SPDK_NOTICELOG("Loaded all directories, total: %d\n", Sto->TotalDirs);
        }
    }
    free(Ctx->DirOnDisk);
    free(Ctx);

    //
    // This is the last callback
    //
    //
    if (Ctx->DirLoopIndex == DDS_MAX_DIRS - 1) {
        //
        // Load the files
        //
        //
        int result;
        DPUFilePropertiesT *fileOnDisk = malloc(sizeof(*fileOnDisk));
        SegmentSizeT nextAddress = DDS_BACKEND_SEGMENT_SIZE - sizeof(DPUFilePropertiesT);
        Ctx->FileOnDisk = fileOnDisk;
        Ctx->LoadedFiles = 0;
        for (size_t f = 0; f != DDS_MAX_FILES; f++) {
            struct LoadDirectoriesAndFilesCtx *CallbackCtx = malloc(sizeof(*CallbackCtx));
            memcpy(CallbackCtx, Ctx, sizeof(Ctx));
            CallbackCtx->FileLoopIndex = f;
            CallbackCtx->FileOnDisk = malloc(sizeof(*CallbackCtx->FileOnDisk));

            result = ReadFromDiskAsyncZC(
                (BufferT) fileOnDisk,
                DDS_BACKEND_RESERVED_SEGMENT,
                nextAddress,
                sizeof(DPUFilePropertiesT),
                LoadFilesCallback,
                CallbackCtx,
                Sto,
                SPDKContext
            );

            if (result != DDS_ERROR_CODE_SUCCESS) {
                Ctx->FailureStatus->HasFailed = true;
                Ctx->FailureStatus->HasAborted = true;
                SPDK_ERRLOG("Read failed with %d, exiting...\n", result);
                exit(-1);
            }

            nextAddress -= sizeof(DPUFilePropertiesT);
        }
    }
}

//
// Load the directories, in other future callbacks will then load the files
//
//
void LoadDirectoriesAndFilesCallback(
    struct spdk_bdev_io *bdev_io,
    bool Success,
    ContextT Context
) {
    struct LoadDirectoriesAndFilesCtx *Ctx = Context;
    SPDKContextT *SPDKContext = Ctx->SPDKContext;

    Sto->TotalDirs = *((int*)(Ctx->TmpSectorBuffer + DDS_BACKEND_INITIALIZATION_MARK_LENGTH));
    Sto->TotalFiles = *((int*)(Ctx->TmpSectorBuffer + DDS_BACKEND_INITIALIZATION_MARK_LENGTH + sizeof(int)));
    SPDK_NOTICELOG("Got Sto->TotalDirs: %d, Sto->TotalFiles: %d\n", Sto->TotalDirs, Sto->TotalFiles);

    //
    // Load the directories
    //
    //
    ErrorCodeT result = DDS_ERROR_CODE_SUCCESS;

    SegmentSizeT nextAddress = DDS_BACKEND_SECTOR_SIZE;

    Ctx->LoadedDirs = malloc(sizeof(Ctx->LoadedDirs));
    *(Ctx->LoadedDirs) = 0;
    for (size_t d = 0; d != DDS_MAX_DIRS; d++) {
        struct LoadDirectoriesAndFilesCtx *CallbackCtx = malloc(sizeof(*CallbackCtx));
        memcpy(CallbackCtx, Ctx, sizeof(Ctx));
        CallbackCtx->DirLoopIndex = d;
        CallbackCtx->DirOnDisk = malloc(sizeof(*CallbackCtx->DirOnDisk));
        result = ReadFromDiskAsyncZC(
            (BufferT) CallbackCtx->DirOnDisk,
            DDS_BACKEND_RESERVED_SEGMENT,
            nextAddress,
            sizeof(DPUDirPropertiesT),
            LoadDirectoriesCallback,
            CallbackCtx,
            Sto,
            SPDKContext
        );

        if (result != DDS_ERROR_CODE_SUCCESS) {
            Ctx->FailureStatus->HasFailed = true;
            Ctx->FailureStatus->HasAborted = true;
            SPDK_ERRLOG("Read failed with %d, exiting...\n", result);
            exit(-1);
        }
       
        nextAddress += sizeof(DPUDirPropertiesT);
    }
}

//
// Load all directories and files from the reserved segment
//
//
ErrorCodeT LoadDirectoriesAndFiles(
    struct DPUStorage* Sto,
    void *Arg,
    struct InitializeCtx *InitializeCtx
){
    if (!Sto->AllSegments || Sto->TotalSegments == 0) {
        return DDS_ERROR_CODE_RESERVED_SEGMENT_ERROR;
    }

    ErrorCodeT result = DDS_ERROR_CODE_SUCCESS;

    //
    // Read the first sector
    //
    //
    char tmpSectorBuffer[DDS_BACKEND_SECTOR_SIZE];
    struct LoadDirectoriesAndFilesCtx *Ctx = malloc(sizeof(*Ctx));
    Ctx->TmpSectorBuffer = tmpSectorBuffer;
    Ctx->FailureStatus = InitializeCtx->FailureStatus;
    Ctx->SPDKContext = InitializeCtx->SPDKContext;
    result = ReadFromDiskAsyncZC(
        tmpSectorBuffer,
        DDS_BACKEND_RESERVED_SEGMENT,
        0,
        DDS_BACKEND_SECTOR_SIZE,
        LoadDirectoriesAndFilesCallback,
        Ctx,
        Sto,
        Arg
    );

    if (result != DDS_ERROR_CODE_SUCCESS) {
        Ctx->FailureStatus->HasFailed = true;
        Ctx->FailureStatus->HasAborted = true;
        SPDK_ERRLOG("ReadFromDiskAsync reserved seg failed with %d, EXITING...\n", result);
        exit(-1);
    }
    return DDS_ERROR_CODE_SUCCESS;
}

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
){
    return WriteToDiskAsyncZC(
        (BufferT)GetDirProperties(Dir),
        DDS_BACKEND_RESERVED_SEGMENT,
        GetDirAddressOnSegment(Dir),
        sizeof(DPUDirPropertiesT),
        Callback,
        Context,
        Sto,
        SPDKContext
    );
}

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
){
    return WriteToDiskAsyncZC(
        (BufferT)GetFileProperties(File),
        DDS_BACKEND_RESERVED_SEGMENT,
        GetFileAddressOnSegment(File),
        sizeof(DPUFilePropertiesT),
        Callback,
        Context,
        Sto,
        SPDKContext
    );
}

//
// Synchronize the first sector on the reserved segment
//
//
ErrorCodeT SyncReservedInformationToDisk(
    struct DPUStorage* Sto,
    void *SPDKContext,
    DiskIOCallback Callback,
    ContextT Context
){
    char tmpSectorBuf[DDS_BACKEND_SECTOR_SIZE];

    memset(tmpSectorBuf, 0, DDS_BACKEND_SECTOR_SIZE);
    strcpy(tmpSectorBuf, DDS_BACKEND_INITIALIZATION_MARK);

    int* numDirs = (int*)(tmpSectorBuf + DDS_BACKEND_INITIALIZATION_MARK_LENGTH);
    *numDirs = Sto->TotalDirs;
    int* numFiles = numDirs + 1;
    *numFiles = Sto->TotalFiles;

    SPDK_NOTICELOG("Sto->TotalDirs %d, Sto->TotalFiles: %d\n", Sto->TotalDirs, Sto->TotalFiles);

    return WriteToDiskAsyncZC(
        tmpSectorBuf,
        DDS_BACKEND_RESERVED_SEGMENT,
        0,
        DDS_BACKEND_SECTOR_SIZE,
        Callback,
        Context,
        Sto,
        SPDKContext
    );
}

void InitializeSyncReservedInfoCallback(
    struct spdk_bdev_io *bdev_io,
    bool Success,
    ContextT Context
) {
    spdk_bdev_free_io(bdev_io);
    struct InitializeCtx *Ctx = Context;
    if (!Success) {
        SPDK_ERRLOG("Initialize() SyncReservedInformationToDisk IO failed...\n");
        exit(-1);
    }

    SPDK_NOTICELOG("Initialize() done!\n");
    G_INITIALIZATION_DONE = true;
}

void InitializeSyncDirToDiskCallback(
    struct spdk_bdev_io *bdev_io,
    bool Success,
    ContextT Context
) {
    spdk_bdev_free_io(bdev_io);
    struct InitializeCtx *Ctx = Context;
    if (!Success) {
        SPDK_NOTICELOG("Initialize() SyncDirToDisk IO failed...\n");
        exit(-1);
    }

    Sto->AllDirs[DDS_DIR_ROOT] = Ctx->RootDir;

    //
    // Set the formatted mark and the numbers of dirs and files
    //
    //
    ErrorCodeT result;
    Sto->TotalDirs = 1;
    Sto->TotalFiles = 0;
    result = SyncReservedInformationToDisk(Sto, Ctx->SPDKContext, InitializeSyncReservedInfoCallback, Ctx);

    if (result != DDS_ERROR_CODE_SUCCESS) {
        SPDK_ERRLOG("Initialize() SyncReservedInformationToDisk failed!!!\n");
        exit(-1);
    }

}

void RemainingPagesProgressCallback(
    struct spdk_bdev_io *bdev_io,
    bool Success,
    ContextT Context
) {
    spdk_bdev_free_io(bdev_io);
    struct InitializeCtx *Ctx = Context;
    if (Ctx->FailureStatus->HasAborted) {
        SPDK_ERRLOG("HasAborted, exiting\n");
        exit(-1);
        return;
    }
    if (!Success) {
        Ctx->FailureStatus->HasFailed = true;
        SPDK_ERRLOG("HasFailed!\n");
    }

    if (Ctx->CurrentProgress == Ctx->TargetProgress) {
        SPDK_NOTICELOG("Initialize() RemainingPagesProgressCallback last one running\n");
        if (Ctx->FailureStatus->HasFailed) {
            //
            // something went wrong, can't continue to do work
            //
            //
            SPDK_ERRLOG("Initialize() RemainingPagesProgressCallback has failed IO, stopping...\n");
            exit(-1);
        }
        else {
            //
            // Create the root directory, which is on the second sector on the segment
            //
            //
            ErrorCodeT result;
            struct DPUDir* rootDir = BackEndDirI(DDS_DIR_ROOT, DDS_DIR_INVALID, DDS_BACKEND_ROOT_DIR_NAME);
            Ctx->RootDir = rootDir;
            result = SyncDirToDisk(rootDir, Sto, Ctx->SPDKContext, InitializeSyncDirToDiskCallback, Ctx);

            if (result != DDS_ERROR_CODE_SUCCESS) {
                Ctx->FailureStatus->HasFailed = true;
                Ctx->FailureStatus->HasAborted = true;
                SPDK_ERRLOG("Initialize() SyncDirToDisk early fail!\n");
                exit(-1);
            }
        }
    }
}

//
// Increment progress callback
//
//
void
IncrementProgressCallback(
    struct spdk_bdev_io *bdev_io,
    bool Success,
    ContextT Context)
{
    spdk_bdev_free_io(bdev_io);
    struct InitializeCtx *Ctx = Context;
    
    if (!Success) {
        SPDK_ERRLOG("HasFailed!\n");
        Ctx->FailureStatus->HasFailed = true;
        exit(-1);
    }
    ErrorCodeT result;
    if (Ctx->PagesLeft == 0) {
        //
        // All done, continue initialization
        // Create the root directory, which is on the second sector on the segment
        //
        //
        SPDK_NOTICELOG("Last of writing zeroed page to reserved seg!\n");
        struct DPUDir* rootDir = BackEndDirI(DDS_DIR_ROOT, DDS_DIR_INVALID, DDS_BACKEND_ROOT_DIR_NAME);
        Ctx->RootDir = rootDir;
        result = SyncDirToDisk(rootDir, Sto, Ctx->SPDKContext, InitializeSyncDirToDiskCallback, Ctx);

        if (result != DDS_ERROR_CODE_SUCCESS) {
            Ctx->FailureStatus->HasFailed = true;
            Ctx->FailureStatus->HasAborted = true;
            SPDK_ERRLOG("Initialize() SyncDirToDisk early fail!!\n");
            // FS app stop func
            exit(-1);
        }
    }
    else {
        //
        // Else not last, issue more zeroing writes
        //
        //
        Ctx->NumPagesWritten += 1;
        Ctx->PagesLeft -= 1;
        
        result = WriteToDiskAsyncZC(
            Ctx->tmpPageBuf,
            DDS_BACKEND_RESERVED_SEGMENT,
            (SegmentSizeT)((Ctx->NumPagesWritten - 1) * DDS_BACKEND_PAGE_SIZE),
            DDS_BACKEND_PAGE_SIZE,
            IncrementProgressCallback,
            Ctx,
            Sto,
            Ctx->SPDKContext
        );
    }
}

void InitializeReadReservedSectorCallback(
    struct spdk_bdev_io *bdev_io,
    bool Success,
    ContextT Context
) {
    struct InitializeCtx *Ctx = Context;
    ErrorCodeT result;

    if (!Success) {
        SPDK_ERRLOG("Initialization failed! Exiting...\n");
        exit(-1);
    }
    if (strcmp(DDS_BACKEND_INITIALIZATION_MARK, Ctx->tmpSectorBuf)) {
        SPDK_NOTICELOG("Backend is NOT initialized!\n");
        //
        // Empty every byte on the reserved segment
        //
        //
        char *tmpPageBuf = malloc(DDS_BACKEND_PAGE_SIZE);
        memset(tmpPageBuf, 0, DDS_BACKEND_PAGE_SIZE);
        Ctx->tmpPageBuf = tmpPageBuf;
        
        size_t pagesPerSegment = DDS_BACKEND_SEGMENT_SIZE / DDS_BACKEND_PAGE_SIZE;

        size_t numPagesWritten = 1;
        Ctx->NumPagesWritten = numPagesWritten;
        Ctx->PagesLeft = pagesPerSegment - numPagesWritten;

        //
        // Issue writes with the maximum queue depth
        //
        //
        Ctx->TargetProgress = pagesPerSegment;
        
        
        Ctx->CurrentProgress = 1;

        result = WriteToDiskAsyncZC(
            tmpPageBuf,
            DDS_BACKEND_RESERVED_SEGMENT,
            (SegmentSizeT)(numPagesWritten * DDS_BACKEND_PAGE_SIZE),
            DDS_BACKEND_PAGE_SIZE,
            IncrementProgressCallback,
            Ctx,
            Sto,
            Ctx->SPDKContext
        );
        if (result != DDS_ERROR_CODE_SUCCESS) {
            Ctx->FailureStatus->HasFailed = true;
            Ctx->FailureStatus->HasAborted = true;  // don't need to run callbacks anymore
            SPDK_ERRLOG("Clearing backend pages failed with %d\n", result);
            exit(-1);
        }
    }
    else {
        SPDK_NOTICELOG("Backend is initialized! Load directories and files...\n");
        //
        // Load directories and files
        //
        //
        result = LoadDirectoriesAndFiles(Sto, Ctx->SPDKContext, Ctx);

        if (result != DDS_ERROR_CODE_SUCCESS) {
            return;
        }

        //
        // Update segment information
        //
        //
        int checkedFiles = 0;
        for (FileIdT f = 0; f != DDS_MAX_FILES; f++) {
            struct DPUFile* file = Sto->AllFiles[f];
            if (file) {
                SegmentIdT* segments = GetFileProperties(file)->Segments;
                for (size_t s = 0; s != DDS_BACKEND_MAX_SEGMENTS_PER_FILE; s++) {
                    SegmentIdT currentSeg = segments[s];
                    if (currentSeg == DDS_BACKEND_SEGMENT_INVALID) {
                        break;
                    }
                    else {
                        Sto->AllSegments[currentSeg].FileId = f;
                    }
                }

                checkedFiles++;
                if (checkedFiles == Sto->TotalFiles) {
                    break;
                }
            }
        }
    }
    spdk_bdev_free_io(bdev_io);
}

//
// Initialize the backend service
// This needs to be async, so using callback chains to accomplish this
//
//
ErrorCodeT Initialize(
    struct DPUStorage* Sto,
    void *Arg // NOTE: this is currently an spdkContext, but depending on the callbacks, they need different arg than this
){
    SPDKContextT *SPDKContext = Arg;
    //
    // Retrieve all segments on disk
    //
    //
    ErrorCodeT result = RetrieveSegments(Sto);

    if (result != DDS_ERROR_CODE_SUCCESS) {
        return result;
    }

    //
    // The first sector on the reserved segment contains initialization information
    //
    //
    bool diskInitialized = false;
    SegmentT* rSeg = &Sto->AllSegments[DDS_BACKEND_RESERVED_SEGMENT];
    char *tmpSectorBuf = malloc(DDS_BACKEND_SECTOR_SIZE + 1);
    memset(tmpSectorBuf, 0, DDS_BACKEND_SECTOR_SIZE + 1);

    // read reserved sector
    struct InitializeCtx *InitializeCtx = malloc(sizeof(*InitializeCtx));
    InitializeCtx->tmpSectorBuf = tmpSectorBuf;
    struct InitializeFailureStatus *FailureStatus = malloc(sizeof(*FailureStatus));
    InitializeCtx->FailureStatus = FailureStatus;
    InitializeCtx->FailureStatus->HasFailed = false;
    InitializeCtx->FailureStatus->HasStopped = false;
    InitializeCtx->FailureStatus->HasStopped = false;
    InitializeCtx->CurrentProgress = 0;
    InitializeCtx->TargetProgress = 0;
    InitializeCtx->SPDKContext = SPDKContext;
    result = ReadFromDiskAsyncZC(
        tmpSectorBuf,
        DDS_BACKEND_RESERVED_SEGMENT,
        0,
        DDS_BACKEND_SECTOR_SIZE,
        InitializeReadReservedSectorCallback,
        InitializeCtx,
        Sto,
        Arg
    );
    SPDK_NOTICELOG("Reserved segment has been read\n");

    if (result != DDS_ERROR_CODE_SUCCESS) {
        printf("Initialize read reserved sector failed!!!\n");
        return result;
    }
    return DDS_ERROR_CODE_SUCCESS;
}

void CreateDirectorySyncReservedInformationToDiskCallback(struct spdk_bdev_io *bdev_io, bool Success, ContextT Context) {
    ControlPlaneHandlerCtx *HandlerCtx = Context;
    spdk_bdev_free_io(bdev_io);
    if (Success) {
        //
        // This is basically the last line of the original CreateDirectory()
        //
        //
        pthread_mutex_unlock(&Sto->SectorModificationMutex);
        *(HandlerCtx->Result) = DDS_ERROR_CODE_SUCCESS;
    }
    else {
        SPDK_ERRLOG("CreateDirectorySyncReservedInformationToDiskCallback failed\n");
        *(HandlerCtx->Result) = DDS_ERROR_CODE_IO_FAILURE;
    }

    //
    // We should free the handler ctx at the end of the last callback
    //
    //
    free(HandlerCtx);
}

void CreateDirectorySyncDirToDiskCallback(
    struct spdk_bdev_io *bdev_io,
    bool Success,
    ContextT Context
) {
    ControlPlaneHandlerCtx *HandlerCtx = Context;
    spdk_bdev_free_io(bdev_io);
    if (Success) {
        Sto->AllDirs[HandlerCtx->DirId] = HandlerCtx->dir;

        pthread_mutex_lock(&Sto->SectorModificationMutex);
        Sto->TotalDirs++;
        ErrorCodeT result = SyncReservedInformationToDisk(Sto, HandlerCtx->SPDKContext,
            CreateDirectorySyncReservedInformationToDiskCallback, HandlerCtx);
        if (result != 0) {
            //
            // Fatal, can't continue, and the callback won't run
            //
            //
            SPDK_ERRLOG("SyncReservedInformationToDisk() returned %hu\n", result);
            pthread_mutex_unlock(&Sto->SectorModificationMutex);
            *(HandlerCtx->Result) = DDS_ERROR_CODE_IO_FAILURE;
        }
        //
        // Else, the callback passed to it should be guaranteed to run
        //
        //
    }
    else {
        SPDK_ERRLOG("CreateDirectorySyncDirToDisk failed, can't create directory\n");
        *(HandlerCtx->Result) = DDS_ERROR_CODE_IO_FAILURE;
    }
}

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
    void *SPDKContext,
    ControlPlaneHandlerCtx *HandlerCtx
){
    HandlerCtx->SPDKContext = SPDKContext;

    struct DPUDir* dir = BackEndDirI(DirId, ParentId, PathName);
    if (!dir) {
        *(HandlerCtx->Result) = DDS_ERROR_CODE_OUT_OF_MEMORY;
        return DDS_ERROR_CODE_OUT_OF_MEMORY;
    }
    HandlerCtx->DirId = DirId;
    HandlerCtx->dir = dir;
    ErrorCodeT result = SyncDirToDisk(dir, Sto, SPDKContext, CreateDirectorySyncDirToDiskCallback, HandlerCtx);

    if (result != DDS_ERROR_CODE_SUCCESS) {
        *(HandlerCtx->Result) = DDS_ERROR_CODE_IO_FAILURE;
        return result;
    }
    return result;
}

void RemoveDirectoryCallback2(struct spdk_bdev_io *bdev_io, bool Success, ContextT Context) {
    ControlPlaneHandlerCtx *HandlerCtx = Context;
    spdk_bdev_free_io(bdev_io);
    if (Success) {
        pthread_mutex_unlock(&Sto->SectorModificationMutex);
        *(HandlerCtx->Result) = DDS_ERROR_CODE_SUCCESS;
        free(HandlerCtx);
    }
    else {
        SPDK_ERRLOG("RemoveDirectoryCallback2() failed\n");
        *(HandlerCtx->Result) = DDS_ERROR_CODE_IO_FAILURE;
    }
}

void RemoveDirectoryCallback1(struct spdk_bdev_io *bdev_io, bool Success, ContextT Context) {
    ControlPlaneHandlerCtx *HandlerCtx = Context;
    spdk_bdev_free_io(bdev_io);
    if (Success) {  
        //
        // Continue to do work
        //
        //
        free(Sto->AllDirs[HandlerCtx->DirId]);
        Sto->AllDirs[HandlerCtx->DirId] = NULL;

        pthread_mutex_lock(&Sto->SectorModificationMutex);

        Sto->TotalDirs--;
        ErrorCodeT result = SyncReservedInformationToDisk(Sto, HandlerCtx->SPDKContext,
            RemoveDirectoryCallback2, HandlerCtx);
        if (result != 0) {
            //
            // Fatal, can't continue, and the callback won't run
            //
            //
            SPDK_ERRLOG("RemoveDirectoryCallback1() returned %hu\n", result);
            pthread_mutex_unlock(&Sto->SectorModificationMutex);
            *(HandlerCtx->Result) = DDS_ERROR_CODE_IO_FAILURE;
        }
        //
        // else, the callback passed to it should be guaranteed to run
        //
        //
    }
    else {
        SPDK_ERRLOG("RemoveDirectoryCallback1() failed, can't create directory\n");
        *(HandlerCtx->Result) = DDS_ERROR_CODE_IO_FAILURE;
    }
}

//
// Delete a directory
// 
//
ErrorCodeT RemoveDirectory(
    DirIdT DirId,
    struct DPUStorage* Sto,
    void *SPDKContext,
    ControlPlaneHandlerCtx *HandlerCtx
){
    HandlerCtx->SPDKContext = SPDKContext;
    if (!Sto->AllDirs[DirId]) {
       *(HandlerCtx->Result) = DDS_ERROR_CODE_DIR_NOT_FOUND;
       return DDS_ERROR_CODE_DIR_NOT_FOUND;
    }

    GetDirProperties(Sto->AllDirs[DirId])->Id = DDS_DIR_INVALID;
    HandlerCtx->DirId = DirId;
    ErrorCodeT result = SyncDirToDisk(Sto->AllDirs[DirId], Sto, SPDKContext, RemoveDirectoryCallback1, HandlerCtx);

    if (result != DDS_ERROR_CODE_SUCCESS) {
        *(HandlerCtx->Result) = DDS_ERROR_CODE_IO_FAILURE;
        return DDS_ERROR_CODE_IO_FAILURE;
    }
    return result;
}

void CreateFileCallback3(
    struct spdk_bdev_io *bdev_io,
    bool Success,
    ContextT Context
) {
    ControlPlaneHandlerCtx *HandlerCtx = Context;
    spdk_bdev_free_io(bdev_io);
    if (Success) {
        pthread_mutex_unlock(&Sto->SectorModificationMutex);
        *(HandlerCtx->Result) = DDS_ERROR_CODE_SUCCESS;
        free(HandlerCtx);
    }
    else {
        SPDK_ERRLOG("CreateFileCallback3() failed\n");
        *(HandlerCtx->Result) = DDS_ERROR_CODE_IO_FAILURE;
    }
}

void CreateFileCallback2(
    struct spdk_bdev_io *bdev_io,
    bool Success,
    ContextT Context
) {
    ControlPlaneHandlerCtx *HandlerCtx = Context;
    spdk_bdev_free_io(bdev_io);
    if (Success) {
        Unlock(HandlerCtx->dir);
        //
        // Finally, file is added
        //
        //
        Sto->AllFiles[HandlerCtx->DirId] = HandlerCtx->File;

        pthread_mutex_lock(&Sto->SectorModificationMutex);

        Sto->TotalFiles++;
        ErrorCodeT result = SyncReservedInformationToDisk(Sto, HandlerCtx->SPDKContext,
            CreateFileCallback3, HandlerCtx);
        if (result != 0) {
            //
            // Fatal, can't continue, and the callback won't run
            //
            //
            SPDK_ERRLOG("CreateFileCallback2() returned %hu\n", result);
            pthread_mutex_unlock(&Sto->SectorModificationMutex);
            *(HandlerCtx->Result) =  DDS_ERROR_CODE_IO_FAILURE;
        }
        //
        // Else, the callback passed to it should be guaranteed to run
        //
        //
    }
    else {
        SPDK_ERRLOG("CreateFileCallback2() failed\n");
        *(HandlerCtx->Result) =  DDS_ERROR_CODE_IO_FAILURE;
    }
}

void CreateFileCallback1(
    struct spdk_bdev_io *bdev_io,
    bool Success,
    ContextT Context
) {
    ControlPlaneHandlerCtx *HandlerCtx = Context;
    spdk_bdev_free_io(bdev_io);
    if (Success) {
        //
        // Continue to do work
        // Add this file to its directory and make the update persistent
        //
        //
        Lock(HandlerCtx->dir);
    
        AddFile(HandlerCtx->FileId, HandlerCtx->dir);
        ErrorCodeT result = SyncDirToDisk(HandlerCtx->dir, Sto, HandlerCtx->SPDKContext,
            CreateFileCallback2, HandlerCtx);
        if (result != 0) {
            //
            // Fatal, can't continue, and the callback won't run
            //
            //
            SPDK_ERRLOG("CreateFileCallback1() returned %hu\n", result);
            Unlock(HandlerCtx->dir);
            *(HandlerCtx->Result) =  DDS_ERROR_CODE_IO_FAILURE;
        }
        //
        // Else, the callback passed to it should be guaranteed to run
        //
        //
    }
    else {
        SPDK_ERRLOG("CreateFileCallback1() failed, can't create directory\n");
        *(HandlerCtx->Result) =  DDS_ERROR_CODE_IO_FAILURE;
    }
}

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
){
    HandlerCtx->SPDKContext = SPDKContext;
    struct DPUDir* dir = Sto->AllDirs[DirId];
    if (!dir) {
        *(HandlerCtx->Result) = DDS_ERROR_CODE_DIR_NOT_FOUND;
        SPDK_ERRLOG("DDS_ERROR_CODE_DIR_NOT_FOUND\n");
        return DDS_ERROR_CODE_DIR_NOT_FOUND;
    }

    struct DPUFile* file = BackEndFileI(FileId, FileName, FileAttributes);
    if (!file) {
        *(HandlerCtx->Result) = DDS_ERROR_CODE_OUT_OF_MEMORY;
        SPDK_ERRLOG("DDS_ERROR_CODE_OUT_OF_MEMORY\n");
        return DDS_ERROR_CODE_OUT_OF_MEMORY;
    }

    //
    // Make file persistent
    //
    //
    HandlerCtx->DirId = DirId;
    HandlerCtx->dir = dir;
    HandlerCtx->FileId = FileId;
    HandlerCtx->File = file;
    ErrorCodeT result = SyncFileToDisk(file, Sto, SPDKContext, CreateFileCallback1, HandlerCtx);

    if (result != DDS_ERROR_CODE_SUCCESS) {
        SPDK_ERRLOG("DDS_ERROR_CODE_IO_FAILURE\n");
        *(HandlerCtx->Result) = DDS_ERROR_CODE_IO_FAILURE;
    }
    return result;
}
void DeleteFileCallback3(struct spdk_bdev_io *bdev_io, bool Success, ContextT Context) {
    ControlPlaneHandlerCtx *HandlerCtx = Context;
    spdk_bdev_free_io(bdev_io);
    if (Success) {
        pthread_mutex_unlock(&Sto->SectorModificationMutex);
        *(HandlerCtx->Result) = DDS_ERROR_CODE_SUCCESS;
        free(HandlerCtx);
    }
    else {
        SPDK_ERRLOG("DeleteFileCallback3() failed\n");
        *(HandlerCtx->Result) = DDS_ERROR_CODE_IO_FAILURE;
    }
}

void DeleteFileCallback2(
    struct spdk_bdev_io *bdev_io,
    bool Success,
    ContextT Context
) {
    ControlPlaneHandlerCtx *HandlerCtx = Context;
    spdk_bdev_free_io(bdev_io);
    if (Success) {
        Unlock(HandlerCtx->dir);

        //
        // Finally, delete the file
        //
        //
        free(HandlerCtx->File);
        Sto->AllFiles[HandlerCtx->FileId] = NULL;

        pthread_mutex_lock(&Sto->SectorModificationMutex);

        Sto->TotalFiles--;
        ErrorCodeT result = SyncReservedInformationToDisk(Sto, HandlerCtx->SPDKContext,
            DeleteFileCallback3, HandlerCtx);
        if (result != 0) {
            //
            // Fatal, can't continue, and the callback won't run
            //
            //
            SPDK_ERRLOG("DeleteFileCallback2() returned %hu\n", result);
            pthread_mutex_unlock(&Sto->SectorModificationMutex);
            *(HandlerCtx->Result) = DDS_ERROR_CODE_IO_FAILURE;
        }
        // 
        // Else, the callback passed to it should be guaranteed to run
        //
        //
    }
    else {
        SPDK_ERRLOG("DeleteFileCallback2() failed\n");
        *(HandlerCtx->Result) = DDS_ERROR_CODE_IO_FAILURE;
    }
}

void DeleteFileCallback1(
    struct spdk_bdev_io *bdev_io,
    bool Success,
    ContextT Context
) {
    ControlPlaneHandlerCtx *HandlerCtx = Context;
    spdk_bdev_free_io(bdev_io);
    if (Success) {
        //
        // Continue to do work
        // Delete this file from its directory and make the update persistent
        //
        //
        Lock(HandlerCtx->dir);

        DeleteFile(HandlerCtx->FileId, HandlerCtx->dir);
        ErrorCodeT result = SyncDirToDisk(HandlerCtx->dir, Sto, HandlerCtx->SPDKContext,
            DeleteFileCallback2, HandlerCtx);
        if (result != 0) {
            //
            // Fatal, can't continue, and the callback won't run
            //
            //
            SPDK_ERRLOG("DeleteFileCallback1() returned %hu\n", result);
            Unlock(HandlerCtx->dir);
            *(HandlerCtx->Result) = DDS_ERROR_CODE_IO_FAILURE;
        }
        //
        // Else, the callback passed to it should be guaranteed to run
        //
        //
    }
    else {
        SPDK_ERRLOG("DeleteFileCallback1() failed, can't create directory\n");
        *(HandlerCtx->Result) = DDS_ERROR_CODE_IO_FAILURE;
    }
}

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
){
    HandlerCtx->SPDKContext = SPDKContext;
    struct DPUFile* file = Sto->AllFiles[FileId];
    struct DPUDir* dir = Sto->AllDirs[DirId];
    if (!file) {
        *(HandlerCtx->Result) = DDS_ERROR_CODE_FILE_NOT_FOUND;
        return DDS_ERROR_CODE_FILE_NOT_FOUND;
    }
    if (!dir) {
        *(HandlerCtx->Result) = DDS_ERROR_CODE_DIR_NOT_FOUND;
        return DDS_ERROR_CODE_DIR_NOT_FOUND;
    }

    GetFileProperties(file)->Id = DDS_FILE_INVALID;
    
    //
    // Make the change persistent
    //
    //
    HandlerCtx->DirId = DirId;
    HandlerCtx->dir = dir;
    HandlerCtx->FileId = FileId;
    HandlerCtx->File = file;
    ErrorCodeT result = SyncFileToDisk(file, Sto, SPDKContext, DeleteFileCallback1, HandlerCtx);

    if (result != DDS_ERROR_CODE_SUCCESS) {
        *(HandlerCtx->Result) = DDS_ERROR_CODE_IO_FAILURE;
    }
    return result;
}

//
// Change the size of a file
// 
//
ErrorCodeT ChangeFileSize(
    FileIdT FileId,
    FileSizeT NewSize,
    struct DPUStorage* Sto,
    CtrlMsgB2FAckChangeFileSize *Resp
){
    struct DPUFile* file = Sto->AllFiles[FileId];

    if (!file) {
        Resp->Result = DDS_ERROR_CODE_FILE_NOT_FOUND;
        return DDS_ERROR_CODE_FILE_NOT_FOUND;
    }

    //
    // There might be concurrent writes, hoping apps will handle the concurrency
    //
    //
    FileSizeT currentFileSize = GetFileProperties(file)->FProperties.FileSize;
    long long sizeToChange = NewSize - currentFileSize;
    if (sizeToChange >= 0) {
        //
        // The previous code was "(size_t)GetNumSegments(file)", but it cannot be
        // understood by compiler, so I rewrite in this format
        // 
        //
        size_t numSeg = GetNumSegments(file); 
        SegmentSizeT remainingAllocatedSize = (SegmentSizeT)(numSeg * DDS_BACKEND_SEGMENT_SIZE - currentFileSize);
        long long bytesToBeAllocated = sizeToChange - (long long)remainingAllocatedSize;
        
        if (bytesToBeAllocated > 0) {
            SegmentIdT numSegmentsToAllocate = (SegmentIdT)(
                bytesToBeAllocated / DDS_BACKEND_SEGMENT_SIZE +
                (bytesToBeAllocated % DDS_BACKEND_SEGMENT_SIZE == 0 ? 0 : 1)
            );

            //
            // Allocate segments
            //
            //
            pthread_mutex_lock(&Sto->SegmentAllocationMutex);

            if (numSegmentsToAllocate > Sto->AvailableSegments) {
                pthread_mutex_unlock(&Sto->SegmentAllocationMutex);
                Resp->Result = DDS_ERROR_CODE_STORAGE_OUT_OF_SPACE;
                return DDS_ERROR_CODE_STORAGE_OUT_OF_SPACE;
            }

            SegmentIdT allocatedSegments = 0;
            for (SegmentIdT s = 0; s != Sto->TotalSegments; s++) {
                SegmentT* seg = &Sto->AllSegments[s];
                if (seg->Allocatable && seg->FileId == DDS_FILE_INVALID) {
                    AllocateSegment(s,file);
                    seg->FileId = FileId;
                    allocatedSegments++;
                    if (allocatedSegments == numSegmentsToAllocate) {
                        break;
                    }
                }
            }

            pthread_mutex_unlock(&Sto->SegmentAllocationMutex);
        }
    }
    else {
        SegmentIdT newSegments = (SegmentIdT)(NewSize / DDS_BACKEND_SEGMENT_SIZE + (NewSize % DDS_BACKEND_SEGMENT_SIZE == 0 ? 0 : 1));
        SegmentIdT numSegmentsToDeallocate = GetNumSegments(file) - newSegments;
        if (numSegmentsToDeallocate > 0) {

            //
            // Deallocate segments
            //
            //
            pthread_mutex_lock(&Sto->SegmentAllocationMutex);

            for (SegmentIdT s = 0; s != numSegmentsToDeallocate; s++) {
                DeallocateSegment(file);
            }

            pthread_mutex_unlock(&Sto->SegmentAllocationMutex);
        }
    }

    //
    // Segment (de)allocation has been taken care of
    //
    //
    SetSize(NewSize, file);
    Resp->Result = DDS_ERROR_CODE_SUCCESS;
    return DDS_ERROR_CODE_SUCCESS;
}

//
// Get file size
// 
//
ErrorCodeT GetFileSize(
    FileIdT FileId,
    FileSizeT* FileSize,
    struct DPUStorage* Sto,
    CtrlMsgB2FAckGetFileSize *Resp
){
    struct DPUFile* file = Sto->AllFiles[FileId];

    if (!file) {
        Resp->Result = DDS_ERROR_CODE_FILE_NOT_FOUND;
        return DDS_ERROR_CODE_FILE_NOT_FOUND;
    }

    *FileSize = GetSize(file);
    Resp->Result = DDS_ERROR_CODE_SUCCESS;
    return DDS_ERROR_CODE_SUCCESS;
}

//
// Async read from a file
// TODO: Currently doesn't handle non block sized IO, deals with split buffer with readv/writev but logic still relies on block sized IO
// 
//
ErrorCodeT ReadFile(
    FileIdT FileId,
    FileSizeT Offset,
    SplittableBufferT *DestBuffer,
    DiskIOCallback Callback,
    ContextT Context,
    struct DPUStorage* Sto,
    void *SPDKContext
){
    ErrorCodeT result = DDS_ERROR_CODE_SUCCESS;
    struct DPUFile* file = Sto->AllFiles[FileId];
    struct PerSlotContext* SlotContext = (struct PerSlotContext*)Context;
    SlotContext->DestBuffer = DestBuffer;
    FileSizeT remainingBytes = GetSize(file) - Offset;

    FileIOSizeT bytesLeftToRead = DestBuffer->TotalSize;
    FileSizeT fileSize = GetSize(file);

    if (Offset > GetSize(file)) {
        SPDK_WARNLOG("Offset > GetSize(file)!\n");
    }

    if (remainingBytes < DestBuffer->TotalSize) {
        //
        // TODO: bytes serviced can be smaller than requested, probably not a problem?
        //
        //
        SPDK_WARNLOG("found remainingBytes < DestBuffer->TotalSize, GetSize(file): %llu, Offset: %llu\n", fileSize, Offset);
        bytesLeftToRead = (FileIOSizeT)remainingBytes;
    }

    SlotContext->BytesIssued = bytesLeftToRead;

    FileSizeT curOffset = Offset;
    SegmentIdT curSegment;
    SegmentSizeT offsetOnSegment;
    SegmentSizeT bytesToIssue;
    SegmentSizeT remainingBytesOnCurSeg;
    BufferT DestAddr;

    while (bytesLeftToRead) {
        //
        // Cross-boundary detection
        //
        //
        curSegment = (SegmentIdT)(curOffset / DDS_BACKEND_SEGMENT_SIZE);
        offsetOnSegment = curOffset % DDS_BACKEND_SEGMENT_SIZE;
        remainingBytesOnCurSeg = DDS_BACKEND_SEGMENT_SIZE - offsetOnSegment;

        FileIOSizeT bytesRead = DestBuffer->TotalSize - bytesLeftToRead;
        int firstSplitLeftToRead = DestBuffer->FirstSize - bytesRead;
        if (DestBuffer->FirstSize - bytesRead < 0) {
            // 
            // TODO: check
            //
            //
            SPDK_ERRLOG("int firstSplitLeftToRead = DestBuffer->FirstSize - bytesRead, underflow, %u - %u = %d\n",
                DestBuffer->FirstSize, bytesRead, firstSplitLeftToRead);
        }

        //
        // May be 1st or 2nd split, the no. of bytes left to read onto it
        //
        //
        FileIOSizeT bytesLeftOnCurrSplit;
        FileIOSizeT splitBufferOffset;

        //
        // XXX: it is assumed that both are block size aligned, so that bytesToIssue is always block size aligned
        // also, bytesToIssue >= bytesLeftOnCurrSplit should always hold true
        //
        //
        bytesToIssue = min(bytesLeftToRead, remainingBytesOnCurSeg);


        if (firstSplitLeftToRead > 0) {
            //
            // First addr
            //
            //
            DestAddr = DestBuffer->FirstAddr;
            bytesLeftOnCurrSplit = firstSplitLeftToRead;
            splitBufferOffset = bytesRead;
        }
        else {
            //
            // Second addr
            //
            //
            SPDK_NOTICELOG("reading into second split, bytesLeftToRead: %d\n", bytesLeftToRead);
            DestAddr = DestBuffer->SecondAddr;
            bytesLeftOnCurrSplit = bytesLeftToRead;
            splitBufferOffset = bytesRead - DestBuffer->FirstSize;
        }

        //
        // If bytes to issue can all fit on curr split, then we don't need sg IO
        //
        //
        if (bytesLeftOnCurrSplit >= bytesToIssue) {
            if (USE_ZERO_COPY) {
                result = ReadFromDiskAsyncZC(
                    DestAddr + splitBufferOffset,
                    curSegment,
                    offsetOnSegment,
                    bytesToIssue,
                    Callback,
                    Context,
                    Sto,
                    SPDKContext
                );
            }
            else {
                result = ReadFromDiskAsyncNonZC(
                    bytesRead,
                    curSegment,
                    offsetOnSegment,
                    bytesToIssue,
                    Callback,
                    Context,
                    Sto,
                    SPDKContext
                );
            }
        }
        else {
            //
            // We need sg, and exactly 2 iovec's, one on 1st split, the other on 2nd split
            //
            //
            if (!(DestAddr == DestBuffer->FirstAddr)) {
                SPDK_ERRLOG("ERROR: SG start addr is not FirstAddr\n");
            }
            SlotContext->Iov[0].iov_base = DestAddr + splitBufferOffset;
            SlotContext->Iov[0].iov_len = bytesLeftOnCurrSplit;

            SlotContext->Iov[1].iov_base = DestBuffer->SecondAddr;
            SlotContext->Iov[1].iov_len = bytesToIssue - bytesLeftOnCurrSplit;

            if (USE_ZERO_COPY) {
                result = ReadvFromDiskAsyncZC(
                    &SlotContext->Iov,
                    2,
                    curSegment,
                    offsetOnSegment,
                    bytesToIssue,
                    Callback,
                    Context,
                    Sto,
                    SPDKContext
                );
            }
            else {
                result = ReadvFromDiskAsyncNonZC(
                    &SlotContext->Iov,
                    2,
                    bytesRead,
                    curSegment,
                    offsetOnSegment,
                    bytesToIssue,
                    Callback,
                    Context,
                    Sto,
                    SPDKContext
                );
            }
        }

        if (result != DDS_ERROR_CODE_SUCCESS) {
            SPDK_WARNLOG("ReadFromDiskAsync() failed with ret: %d\n", result);
            SlotContext->Ctx->Response->Result = DDS_ERROR_CODE_IO_FAILURE;
            SlotContext->Ctx->Response->BytesServiced = 0;
            return result;
        }
        else {
            SlotContext->CallbacksToRun += 1;
        }

        curOffset += bytesToIssue;
        bytesLeftToRead -= bytesToIssue;
    }

    return result;
}


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
){
    struct DPUFile* file = Sto->AllFiles[FileId];
    FileSizeT newSize = Offset + SourceBuffer->TotalSize;
    ErrorCodeT result = DDS_ERROR_CODE_SUCCESS;
    struct PerSlotContext* SlotContext = (struct PerSlotContext*)Context;

    //
    // Check if allocation is needed
    //
    //
    FileSizeT currentFileSize = GetFileProperties(file)->FProperties.FileSize;
    long long sizeToChange = newSize - currentFileSize;

    if (sizeToChange >= 0) {
        size_t numSeg = GetNumSegments(file);
        SegmentSizeT remainingAllocatedSize = (SegmentSizeT)(numSeg * DDS_BACKEND_SEGMENT_SIZE - currentFileSize);
        long long bytesToBeAllocated = sizeToChange - (long long)remainingAllocatedSize;

        if (bytesToBeAllocated > 0) {
            SegmentIdT numSegmentsToAllocate = (SegmentIdT)(
                bytesToBeAllocated / DDS_BACKEND_SEGMENT_SIZE +
                (bytesToBeAllocated % DDS_BACKEND_SEGMENT_SIZE == 0 ? 0 : 1)
                );

            //
            // Allocate segments
            //
            //
            pthread_mutex_lock(&Sto->SegmentAllocationMutex);

            if (numSegmentsToAllocate > Sto->AvailableSegments) {
                SPDK_ERRLOG("Need to allocate seg for file, but not enough available segs left!\n");
                pthread_mutex_unlock(&Sto->SegmentAllocationMutex);
                return DDS_ERROR_CODE_STORAGE_OUT_OF_SPACE;
            }

            SegmentIdT allocatedSegments = 0;
            for (SegmentIdT s = 0; s != Sto->TotalSegments; s++) {
                SegmentT* seg = &Sto->AllSegments[s];
                if (seg->Allocatable && seg->FileId == DDS_FILE_INVALID) {
                    AllocateSegment(s,file);
                    seg->FileId = FileId;
                    allocatedSegments++;
                    if (allocatedSegments == numSegmentsToAllocate) {
                        break;
                    }
                }
            }

            pthread_mutex_unlock(&Sto->SegmentAllocationMutex);
        }

        SetSize(newSize,file);
    }

    FileIOSizeT bytesLeftToWrite = SourceBuffer->TotalSize;
    SlotContext->BytesIssued = bytesLeftToWrite;

    FileSizeT curOffset = Offset;
    SegmentIdT curSegment;
    SegmentSizeT offsetOnSegment;
    SegmentSizeT bytesToIssue;
    SegmentSizeT remainingBytesOnCurSeg;
    BufferT SourceAddr = SourceBuffer->FirstAddr;
    while (bytesLeftToWrite) {
        //
        // Cross-boundary detection
        //
        //
        curSegment = (SegmentIdT)(curOffset / DDS_BACKEND_SEGMENT_SIZE);
        offsetOnSegment = curOffset % DDS_BACKEND_SEGMENT_SIZE;
        remainingBytesOnCurSeg = DDS_BACKEND_SEGMENT_SIZE - offsetOnSegment;

        FileIOSizeT bytesWritten = SourceBuffer->TotalSize - bytesLeftToWrite;
        int firstSplitLeftToWrite = SourceBuffer->FirstSize - bytesWritten;

        //
        // May be 1st or 2nd split, the no. of bytes left to write on it
        //
        //
        FileIOSizeT bytesLeftOnCurrSplit;
        FileIOSizeT splitBufferOffset;

        bytesToIssue = min(bytesLeftToWrite, remainingBytesOnCurSeg);
        
        if (firstSplitLeftToWrite > 0) {
            //
            // Writing from first addr
            //
            //
            SourceAddr = SourceBuffer->FirstAddr;
            bytesLeftOnCurrSplit = firstSplitLeftToWrite;
            splitBufferOffset = bytesWritten;
        }
        else {
            //
            // Writing from second addr
            //
            //
            SPDK_NOTICELOG("Writing from second split, bytesLeftToWrite: %d\n", bytesLeftToWrite);
            SourceAddr = SourceBuffer->SecondAddr;
            bytesLeftOnCurrSplit = bytesLeftToWrite;
            splitBufferOffset = bytesWritten - SourceBuffer->FirstSize;
        }
        
        if (bytesLeftOnCurrSplit >= bytesToIssue) {
            //
            // If bytes to issue can all fit on curr split, then we don't need sg IO
            //
            //
            if (USE_ZERO_COPY) {
                result = WriteToDiskAsyncZC(
                    SourceAddr + splitBufferOffset,
                    curSegment,
                    offsetOnSegment,
                    bytesToIssue,
                    Callback,
                    SlotContext,
                    Sto,
                    SPDKContext
                );
            }
            else {
                result = WriteToDiskAsyncNonZC(
                    SourceAddr + splitBufferOffset,
                    bytesWritten,
                    curSegment,
                    offsetOnSegment,
                    bytesToIssue,
                    Callback,
                    SlotContext,
                    Sto,
                    SPDKContext
                );
            }
        }
        else {
            //
            // We need sg, and exactly 2 iovec's, one on 1st split, the other on 2nd split
            //
            //
            assert (SourceAddr == SourceBuffer->FirstAddr);
            SlotContext->Iov[0].iov_base = SourceAddr + splitBufferOffset;
            SlotContext->Iov[0].iov_len = bytesLeftOnCurrSplit;
            SlotContext->Iov[1].iov_base = SourceBuffer->SecondAddr;
            SlotContext->Iov[1].iov_len = bytesToIssue - bytesLeftOnCurrSplit;
            
            if (USE_ZERO_COPY) {
                result = WritevToDiskAsyncZC(
                    &SlotContext->Iov,
                    2,
                    curSegment,
                    offsetOnSegment,
                    bytesToIssue,
                    Callback,
                    Context,
                    Sto,
                    SPDKContext
                );
            }
            else {
                result = WritevToDiskAsyncNonZC(
                    &SlotContext->Iov,
                    2,
                    bytesWritten,
                    curSegment,
                    offsetOnSegment,
                    bytesToIssue,
                    Callback,
                    Context,
                    Sto,
                    SPDKContext
                );
            }
        }

        if (result != DDS_ERROR_CODE_SUCCESS) {
            SPDK_WARNLOG("WriteToDiskAsync() failed with ret: %d\n", result);
            SlotContext->Ctx->Response->Result = DDS_ERROR_CODE_IO_FAILURE;
            SlotContext->Ctx->Response->BytesServiced = 0;
            return result;
        }
        else {
            SlotContext->CallbacksToRun += 1;
        }

        curOffset += bytesToIssue;
        bytesLeftToWrite -= bytesToIssue;
    }

    return result;
}

//
// Get file properties by file id
// 
//
ErrorCodeT GetFileInformationById(
    FileIdT FileId,
    DDSFilePropertiesT* FileProperties,
    struct DPUStorage* Sto,
    CtrlMsgB2FAckGetFileInfo *Resp
){
    struct DPUFile* file = Sto->AllFiles[FileId];
    if (!file) {
        Resp->Result = DDS_ERROR_CODE_FILE_NOT_FOUND;
        return DDS_ERROR_CODE_FILE_NOT_FOUND;
    }

    FileProperties->FileAttributes = GetAttributes(file);
    strcpy(FileProperties->FileName, GetName(file));
    FileProperties->FileSize = GetSize(file);
    Resp->Result = DDS_ERROR_CODE_SUCCESS;
    return DDS_ERROR_CODE_SUCCESS;
}

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
){
    struct DPUFile* file = Sto->AllFiles[FileId];
    if (!file) {
        Resp->Result = DDS_ERROR_CODE_FILE_NOT_FOUND;
        return DDS_ERROR_CODE_FILE_NOT_FOUND;
    }

    *FileAttributes = GetAttributes(file);
    Resp->Result = DDS_ERROR_CODE_SUCCESS;
    return DDS_ERROR_CODE_SUCCESS;
}

//
// Get the size of free space on the storage
// 
//
ErrorCodeT GetStorageFreeSpace(
    FileSizeT* StorageFreeSpace,
    struct DPUStorage* Sto,
    CtrlMsgB2FAckGetFreeSpace *Resp
){
    pthread_mutex_lock(&Sto->SegmentAllocationMutex);

    *StorageFreeSpace = Sto->AvailableSegments * (FileSizeT)DDS_BACKEND_SEGMENT_SIZE;

    pthread_mutex_unlock(&Sto->SegmentAllocationMutex);
    Resp->Result = DDS_ERROR_CODE_SUCCESS;
    return DDS_ERROR_CODE_SUCCESS;
}

void MoveFileCallback2(struct spdk_bdev_io *bdev_io, bool Success, ContextT Context) {
    ControlPlaneHandlerCtx *HandlerCtx = Context;
    spdk_bdev_free_io(bdev_io);
    if (Success) {
        Unlock(HandlerCtx->NewDir);
        SetName(HandlerCtx->NewFileName, HandlerCtx->File);
        *(HandlerCtx->Result) = DDS_ERROR_CODE_SUCCESS;
        free(HandlerCtx);
    }
    else {
        SPDK_ERRLOG("RemoveDirectoryCallback2() failed\n");
        *(HandlerCtx->Result) = DDS_ERROR_CODE_IO_FAILURE;
    }
}

void MoveFileCallback1(struct spdk_bdev_io *bdev_io, bool Success, ContextT Context) {
    ControlPlaneHandlerCtx *HandlerCtx = Context;
    spdk_bdev_free_io(bdev_io);
    if (Success) {  
        Unlock(HandlerCtx->dir);

        //
        // Update new directory
        //
        //
        Lock(HandlerCtx->NewDir);
    
        ErrorCodeT result = AddFile(HandlerCtx->FileId, HandlerCtx->NewDir);

        if (result != DDS_ERROR_CODE_SUCCESS) {
            Unlock(HandlerCtx->NewDir);
            *(HandlerCtx->Result) = result;
        }
        result = SyncDirToDisk(HandlerCtx->NewDir, Sto, HandlerCtx->SPDKContext,
            MoveFileCallback2, HandlerCtx);
        if (result != 0) {
            //
            // Fatal, can't continue, and the callback won't run
            //
            //
            SPDK_ERRLOG("MoveFileCallback1() returned %hu\n", result);
            Unlock(HandlerCtx->NewDir);
            *(HandlerCtx->Result) = DDS_ERROR_CODE_IO_FAILURE;
        }
        //
        // Else, the callback passed to it should be guaranteed to run
        //
        //
    }
    else {
        Unlock(HandlerCtx->dir);
        SPDK_ERRLOG("MoveFileCallback1() failed, can't create directory\n");
        *(HandlerCtx->Result) = DDS_ERROR_CODE_IO_FAILURE;
    }
}

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
){
    HandlerCtx->SPDKContext = SPDKContext;
    struct DPUFile* file = Sto->AllFiles[FileId];
    struct DPUDir* OldDir = Sto->AllDirs[OldDirId];
    struct DPUDir* NewDir = Sto->AllDirs[NewDirId];
    if (!file) {
        *(HandlerCtx->Result) = DDS_ERROR_CODE_DIR_NOT_FOUND;
        return DDS_ERROR_CODE_DIR_NOT_FOUND;
    }
    if (!OldDir || !NewDir) {
        *(HandlerCtx->Result) = DDS_ERROR_CODE_FILE_NOT_FOUND;
        return DDS_ERROR_CODE_FILE_NOT_FOUND;
    }

    //
    // Update old directory
    //
    //
    Lock(OldDir);

    ErrorCodeT result = DeleteFile(FileId, OldDir);

    if (result != DDS_ERROR_CODE_SUCCESS) {
        Unlock(OldDir);
        *(HandlerCtx->Result) = result;
        return result;
    }
    HandlerCtx->dir = OldDir;
    HandlerCtx->NewDir = NewDir;
    HandlerCtx->FileId = FileId;
    HandlerCtx->File = file;
    HandlerCtx->NewFileName = NewFileName;
    result = SyncDirToDisk(OldDir, Sto, SPDKContext, MoveFileCallback1, HandlerCtx);

    if (result != DDS_ERROR_CODE_SUCCESS) {
        *(HandlerCtx->Result) = DDS_ERROR_CODE_IO_FAILURE;
    }
    return result;
}