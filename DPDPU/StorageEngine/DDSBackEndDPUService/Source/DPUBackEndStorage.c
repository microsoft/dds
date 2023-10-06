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

//
// Read from disk synchronously
// deprecated
//
//
/* ErrorCodeT ReadFromDiskSync(
    BufferT DstBuffer,
    SegmentIdT SegmentId,
    SegmentSizeT SegmentOffset,
    FileIOSizeT Bytes,
    struct DPUStorage* Sto,
    void *arg,
    bool ZeroCopy
){
    SegmentT* seg = &Sto->AllSegments[SegmentId];
    SyncRWCompletionStatus *completionStatus = malloc(sizeof(*completionStatus));
    *completionStatus = SyncRWCompletion_NOT_COMPLETED;
    SPDK_NOTICELOG("calling bdev_read() with cb_arg ptr: %p, value: %d\n", completionStatus, *completionStatus);
    int rc;
    if (ZeroCopy){
        rc = bdev_read(arg, DstBuffer, seg->DiskAddress + SegmentOffset, Bytes,
        ReadFromDiskSyncCallback, completionStatus);
    }
    else{
        int position = FindFreeSpace(arg);
        if(position == -1){
            return DDS_ERROR_CODE_FAILED_BUFFER_ALLOCATION;
        }
        else{
            SPDKContextT *SPDKContext = arg;
            rc = bdev_read(arg, SPDKContext->buff[position * DDS_BACKEND_SPDK_BUFF_BLOCK_SPACE], 
            seg->DiskAddress + SegmentOffset, Bytes, ReadFromDiskSyncCallback, completionStatus);
        }
    }
    // int rc = bdev_read(arg, DstBuffer, seg->DiskAddress + SegmentOffset, Bytes,
    //     ReadFromDiskSyncCallback, completionStatus, true, 0);
    SPDK_NOTICELOG("bdev_read() returned %d\n", rc);

    //memcpy(DstBuffer, (char*)seg->DiskAddress + SegmentOffset, Bytes);

    if (rc)
    {
        return DDS_ERROR_CODE_INVALID_FILE_POSITION;  // there should be an SPDK error log before this
    }
    // otherwise it should be fine, wait for completion
    printf("ReadFromDiskSync entering busy waiting\n");
    while (true)  // busy wait for IO completion
    {
        if (*completionStatus == SyncRWCompletionSUCCESS)
            return DDS_ERROR_CODE_SUCCESS;
        else if (*completionStatus == SyncRWCompletionFAILED)  // doc: use spdk_bdev_io_get_nvme_status() to obtain info
            return DDS_ERROR_CODE_STORAGE_OUT_OF_SPACE;
    }
} */

/* void ReadFromDiskSyncCallback(
    struct spdk_bdev_io *bdev_io,
    bool success,
    void *cb_arg
){
    SPDK_NOTICELOG("ReadFromDiskSyncCallback run on thread: %d\n", spdk_thread_get_id(spdk_get_thread()));
    SPDK_NOTICELOG("cb_args original value: %d\n", *((SyncRWCompletionStatus *) cb_arg));
    if (success)
    {
        *((SyncRWCompletionStatus *) cb_arg) = SyncRWCompletionSUCCESS;
        printf("ReadFromDiskSyncCallback called, success status: %d\n", *((SyncRWCompletionStatus *) cb_arg));
        spdk_bdev_free_io(bdev_io);
    }
    else
    {
        *((SyncRWCompletionStatus *) cb_arg) = SyncRWCompletionFAILED;
        printf("ReadFromDiskSyncCallback called, failed status: %d\n", *((SyncRWCompletionStatus *) cb_arg));
        spdk_bdev_free_io(bdev_io);
    }
} */

//
// Write to disk synchronously, deprecated
//
//
/* ErrorCodeT WriteToDiskSync(
    BufferT SrcBuffer,
    SegmentIdT SegmentId,
    SegmentSizeT SegmentOffset,
    FileIOSizeT Bytes,
    struct DPUStorage* Sto,
    void *arg,
    bool ZeroCopy
){
    SegmentT* seg = &Sto->AllSegments[SegmentId];
    SyncRWCompletionStatus completionStatus = SyncRWCompletion_NOT_COMPLETED;
    int rc;
    if (ZeroCopy){
        int rc = bdev_write(arg, SrcBuffer, seg->DiskAddress + SegmentOffset, Bytes, 
        WriteToDiskSyncCallback, &completionStatus);
    }
    else{
        int position = FindFreeSpace(arg);
        if(position == -1){
            return DDS_ERROR_CODE_FAILED_BUFFER_ALLOCATION;
        }
        else{
            SPDKContextT *SPDKContext = arg;
            rc = bdev_write(arg, SPDKContext->buff[position * DDS_BACKEND_SPDK_BUFF_BLOCK_SPACE], 
            seg->DiskAddress + SegmentOffset, Bytes, WriteToDiskSyncCallback, &completionStatus);
        }
    }
    // int rc = bdev_write(arg, SrcBuffer, seg->DiskAddress + SegmentOffset, Bytes, 
    // WriteToDiskSyncCallback, &completionStatus);
    //memcpy((char*)seg->DiskAddress + SegmentOffset, SrcBuffer, Bytes);

    if (rc)
    {
        return DDS_ERROR_CODE_INVALID_FILE_POSITION;  // there should be an SPDK error log before this
    }
    // otherwise it should be fine, wait for completion
    printf("WriteToDiskSync entering busy waiting\n");
    while (true)  // busy wait for IO completion
    {
        if (completionStatus == SyncRWCompletionSUCCESS)
            return DDS_ERROR_CODE_SUCCESS;
        else if (completionStatus == SyncRWCompletionFAILED)  // doc: use spdk_bdev_io_get_nvme_status() to obtain info
            return DDS_ERROR_CODE_STORAGE_OUT_OF_SPACE;
    }

    return DDS_ERROR_CODE_SUCCESS;
} */

/* ErrorCodeT WriteToDiskSyncCallback(
    struct spdk_bdev_io *bdev_io,
    bool success,
    void *cb_arg
){
    SPDK_NOTICELOG("cb_args original value: %d\n", *((int *) cb_arg));
    if (success)
    {
        *((SyncRWCompletionStatus *) cb_arg) = SyncRWCompletionSUCCESS;
        SPDK_NOTICELOG("WriteToDiskSyncCallback called, success status: %d\n", *((SyncRWCompletionStatus *) cb_arg));
        spdk_bdev_free_io(bdev_io);
    }
    else
    {
        *((SyncRWCompletionStatus *) cb_arg) = SyncRWCompletionFAILED;
        SPDK_NOTICELOG("WriteToDiskSyncCallback called, failed status: %d\n", *((SyncRWCompletionStatus *) cb_arg));
        spdk_bdev_free_io(bdev_io);
    }
} */

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
    void *SPDKContext,
    bool ZeroCopy
){
    SegmentT* seg = &Sto->AllSegments[SegmentId];
    int rc;
    if (ZeroCopy){
        rc = bdev_read(SPDKContext, DstBuffer, seg->DiskAddress + SegmentOffset, Bytes, 
        Callback, Context);
    }
    else{
        struct PerSlotContext* SlotContext = (struct PerSlotContext*) Context;
        int position = SlotContext->Position;
        SPDKContextT *arg = SPDKContext;
        if (Bytes > DDS_BACKEND_SPDK_BUFF_BLOCK_SPACE) {
            SPDK_WARNLOG("A read with %d bytes exceeds buff block space: %d\n", Bytes, DDS_BACKEND_SPDK_BUFF_BLOCK_SPACE);
        }
        rc = bdev_read(SPDKContext, &arg->buff[position * DDS_BACKEND_SPDK_BUFF_BLOCK_SPACE], 
        seg->DiskAddress + SegmentOffset, Bytes, Callback, Context);
    }
    
    //memcpy(DstBuffer, (char*)seg->DiskAddress + SegmentOffset, Bytes);

    if (rc)
    {
        printf("ReadFromDiskAsync called bdev_read(), but failed with: %d\n", rc);
        return DDS_ERROR_CODE_INVALID_FILE_POSITION;  // there should be an SPDK error log before this
    }

    return DDS_ERROR_CODE_SUCCESS; // TODO: since it's async, we don't know if the actual read will be successful
}

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
    ContextT Context,  // this should be the callback arg
    struct DPUStorage* Sto,
    void *SPDKContext,
    bool ZeroCopy
){
    SegmentT* seg = &Sto->AllSegments[SegmentId];
    int rc;
    if (ZeroCopy){
        rc = bdev_write(SPDKContext, SrcBuffer, seg->DiskAddress + SegmentOffset, Bytes, 
        Callback, Context);
    }
    else{
        struct PerSlotContext* SlotContext = (struct PerSlotContext*) Context;
        int position = SlotContext->Position;
        SPDKContextT *arg = SPDKContext;
        if (Bytes > DDS_BACKEND_SPDK_BUFF_BLOCK_SPACE) {
            SPDK_WARNLOG("A write with %d bytes exceeds buff block space: %d\n", Bytes, DDS_BACKEND_SPDK_BUFF_BLOCK_SPACE);
        }
        memcpy(&arg->buff[position * DDS_BACKEND_SPDK_BUFF_BLOCK_SPACE], SrcBuffer, Bytes);
        rc = bdev_write(SPDKContext, &arg->buff[position * DDS_BACKEND_SPDK_BUFF_BLOCK_SPACE], 
        seg->DiskAddress + SegmentOffset, Bytes, Callback, Context);
    }
    //memcpy((char*)seg->DiskAddress + SegmentOffset, SrcBuffer, Bytes);

    if (rc)
    {
        printf("WriteToDiskAsync called bdev_write(), but failed with: %d\n", rc);
        return DDS_ERROR_CODE_INVALID_FILE_POSITION;  // there should be an SPDK error log before this
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
        

        // we dont allocate memory to newSegment. Instead, we calculate 
        // the start position of each segment by i*DDS_BACKEND_SEGMENT_SIZE
        // and use this position as disk address

        // char* newSegment = malloc(sizeof(char) * DDS_BACKEND_SEGMENT_SIZE);

        // if (!newSegment) {
        //     return DDS_ERROR_CODE_SEGMENT_RETRIEVAL_FAILURE;
        // }

        DiskSizeT newSegment = i * DDS_BACKEND_SEGMENT_SIZE;

        // memset(newSegment, 0, sizeof(char) * DDS_BACKEND_SEGMENT_SIZE);

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
            // we should do nothing because now segment is on disk?
            // if (Sto->AllSegments[i].DiskAddress) {
            //     //delete[] (char*)AllSegments[i].DiskAddress;
            //     //free((char*)Sto->AllSegments[i].DiskAddress);
            // }

            if (Sto->AllSegments[i].Allocatable) {
                Sto->AvailableSegments--;
            }
        }

        //delete[] AllSegments;
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
            // return DDS_ERROR_CODE_OUT_OF_MEMORY;
            Ctx->FailureStatus->HasFailed = true;
            Ctx->FailureStatus->HasAborted = true;
            // TODO: call FS app stop func
        }

        *(Ctx->LoadedFiles)++;
        if (*(Ctx->LoadedFiles) == Sto->TotalFiles) {
            // break;
            SPDK_NOTICELOG("Loaded all files, total: %d\n", Sto->TotalFiles);
        }
    }
    free(Ctx->FileOnDisk);
    free(Ctx);

    // XXX TODO: we are done Initialize(), continue work
    if (Ctx->FileLoopIndex == DDS_MAX_FILES - 1) {
        // continue work
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
        else {  // this should be very unlikely
            // return DDS_ERROR_CODE_OUT_OF_MEMORY;
            // TODO: call app stop func
        }

        *(Ctx->LoadedDirs) += 1;
        if (*(Ctx->LoadedDirs) == Sto->TotalDirs) {
            // really not much to do
            SPDK_NOTICELOG("Loaded all directories, total: %d\n", Sto->TotalDirs);
        }
    }
    free(Ctx->DirOnDisk);
    free(Ctx);

    //
    // this is the last callback
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
        // int loadedFiles = 0;
        Ctx->FileOnDisk = fileOnDisk;
        Ctx->LoadedFiles = 0;
        for (size_t f = 0; f != DDS_MAX_FILES; f++) {
            struct LoadDirectoriesAndFilesCtx *CallbackCtx = malloc(sizeof(*CallbackCtx));
            memcpy(CallbackCtx, Ctx, sizeof(Ctx));
            CallbackCtx->FileLoopIndex = f;
            CallbackCtx->FileOnDisk = malloc(sizeof(*CallbackCtx->FileOnDisk));

            result = ReadFromDiskAsync(
                (BufferT) fileOnDisk,
                DDS_BACKEND_RESERVED_SEGMENT,
                nextAddress,
                sizeof(DPUFilePropertiesT),
                LoadFilesCallback,
                CallbackCtx,
                Sto,
                SPDKContext,
                true
            );

            if (result != DDS_ERROR_CODE_SUCCESS) {
                // return result;
                Ctx->FailureStatus->HasFailed = true;
                Ctx->FailureStatus->HasAborted = true;
                // FS app stop
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

    // DPUDirPropertiesT dirOnDisk;
    // DPUDirPropertiesT *dirOnDisk = malloc(sizeof(*dirOnDisk));
    ErrorCodeT result = DDS_ERROR_CODE_SUCCESS;

    SegmentSizeT nextAddress = DDS_BACKEND_SECTOR_SIZE;

    Ctx->LoadedDirs = malloc(sizeof(Ctx->LoadedDirs));
    *(Ctx->LoadedDirs) = 0;
    for (size_t d = 0; d != DDS_MAX_DIRS; d++) {
        struct LoadDirectoriesAndFilesCtx *CallbackCtx = malloc(sizeof(*CallbackCtx));
        memcpy(CallbackCtx, Ctx, sizeof(Ctx));
        CallbackCtx->DirLoopIndex = d;
        // CallbackCtx->NextAddress = nextAddress;
        CallbackCtx->DirOnDisk = malloc(sizeof(*CallbackCtx->DirOnDisk));
        result = ReadFromDiskAsync(
            (BufferT) CallbackCtx->DirOnDisk,
            DDS_BACKEND_RESERVED_SEGMENT,
            nextAddress,
            sizeof(DPUDirPropertiesT),
            LoadDirectoriesCallback,
            CallbackCtx,
            Sto,
            SPDKContext,
            true
        );

        if (result != DDS_ERROR_CODE_SUCCESS) {
            // return result;
            Ctx->FailureStatus->HasFailed = true;
            Ctx->FailureStatus->HasAborted = true;
            // FS app stop
            SPDK_ERRLOG("Read failed with %d, exiting...\n", result);
            exit(-1);
        }
       
        nextAddress += sizeof(DPUDirPropertiesT);
    }
    

    // return result;
}

//
// Load all directories and files from the reserved segment
//
//
ErrorCodeT LoadDirectoriesAndFiles(
    struct DPUStorage* Sto,
    void *arg,
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
    result = ReadFromDiskAsync(
        tmpSectorBuffer,
        DDS_BACKEND_RESERVED_SEGMENT,
        0,
        DDS_BACKEND_SECTOR_SIZE,
        LoadDirectoriesAndFilesCallback,
        Ctx,
        Sto,
        arg,
        true
    );

    if (result != DDS_ERROR_CODE_SUCCESS) {
        Ctx->FailureStatus->HasFailed = true;
        Ctx->FailureStatus->HasAborted = true;
        // FS app stop
        SPDK_ERRLOG("ReadFromDiskAsync reserved seg failed with %d, EXITING...\n", result);
        exit(-1);
        // return result;
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
    return WriteToDiskAsync(
        (BufferT)GetDirProperties(Dir),
        DDS_BACKEND_RESERVED_SEGMENT,
        GetDirAddressOnSegment(Dir),
        sizeof(DPUDirPropertiesT),
        Callback,
        Context,
        Sto,
        SPDKContext,
        true
    );
    /* return WriteToDiskSync(
        (BufferT)GetDirProperties(Dir),
        DDS_BACKEND_RESERVED_SEGMENT,
        GetDirAddressOnSegment(Dir),
        sizeof(DPUDirPropertiesT),
        Sto,
        SPDKContext,
        true
    ); */
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
    return WriteToDiskAsync(
        (BufferT)GetFileProperties(File),
        DDS_BACKEND_RESERVED_SEGMENT,
        GetFileAddressOnSegment(File),
        sizeof(DPUFilePropertiesT),
        Callback,
        Context,
        Sto,
        SPDKContext,
        true
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

    return WriteToDiskAsync(
        tmpSectorBuf,
        DDS_BACKEND_RESERVED_SEGMENT,
        0,
        DDS_BACKEND_SECTOR_SIZE,
        Callback,
        Context,
        Sto,
        SPDKContext,
        true
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
        // FS app stop
        exit(-1);
    }
    // else success, Initialize() done, continue work? FS would be started by now, nothing more to do
    SPDK_NOTICELOG("Initialize() done!!!\n");
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
        // FS app stop
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
            // something went wrong, can't continue to do work
            SPDK_ERRLOG("Initialize() RemainingPagesProgressCallback has failed IO, stopping...\n");
            // FS app stop
            exit(-1);
        }
        else {  // we can do the rest of the work
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
                SPDK_ERRLOG("Initialize() SyncDirToDisk early fail!!\n");
                // FS app stop func
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
    if (Ctx->FailureStatus->HasAborted) {
        if (Ctx->FailureStatus->HasStopped) {
            return;
        }
        // TODO: fatal, call func to stop FS spdk app
        //
        Ctx->FailureStatus->HasStopped = true;
        SPDK_ERRLOG("fatal, HasStopped and HasAborted, exiting...\n");
        exit(-1);
        return;
    }

    if (!Success) {
        SPDK_ERRLOG("HasFailed!\n");
        Ctx->FailureStatus->HasFailed = true;
    }

    // this is the last of looped IO callbacks
    if (Ctx->CurrentProgress == Ctx->TargetProgress) {
        SPDK_NOTICELOG("Initialize() IncrementProgressCallback last one running\n");
        if (Ctx->FailureStatus->HasFailed) {
            SPDK_ERRLOG("Initialize() IncrementProgressCallback has failed IO, stopping...\n");
            exit(-1);
            return;
        }
        else {  // we can do the rest of the work
            ErrorCodeT result;
            SPDK_NOTICELOG("continue Initialize() work\n");
            //
            // Handle the remaining pages
            //
            //
            SPDK_NOTICELOG("Initialize(), pagesLeft: %d\n", Ctx->PagesLeft);  // this should be 0?
            Ctx->TargetProgress = Ctx->PagesLeft;
            Ctx->CurrentProgress = 0;
            char tmpPageBuf[DDS_BACKEND_PAGE_SIZE];
            memset(tmpPageBuf, 0, DDS_BACKEND_PAGE_SIZE);

            for (size_t q = 0; q != Ctx->PagesLeft; q++) {
                result = WriteToDiskAsync(
                    tmpPageBuf,
                    0,
                    (SegmentSizeT)((Ctx->NumPagesWritten + q) * DDS_BACKEND_PAGE_SIZE),
                    DDS_BACKEND_PAGE_SIZE,
                    RemainingPagesProgressCallback,
                    Ctx,
                    Sto,
                    Ctx->SPDKContext,
                    true
                );

                if (result != DDS_ERROR_CODE_SUCCESS) {
                    Ctx->FailureStatus->HasAborted = true;  // don't need to run callbacks anymore
                    Ctx->FailureStatus->HasFailed = true;
                    return;
                }
            }

            /* while (Sto->CurrentProgress != Sto->TargetProgress) {
                //std::this_thread::yield();
                sched_yield();
            } */

            
        }
    }
    // else not last, wait until it's the last callback


    /* atomic_size_t* progress = (atomic_size_t*)Context;
    (*progress) += 1;
    spdk_bdev_free_io(bdev_io); */
}

void InitializeReadReservedSectorCallback(struct spdk_bdev_io *bdev_io, bool Success, ContextT Context) {
    spdk_bdev_free_io(bdev_io);
    struct InitializeCtx *Ctx = Context;
    ErrorCodeT result;

    if (strcmp(DDS_BACKEND_INITIALIZATION_MARK, Ctx->tmpSectorBuf)) {
        SPDK_NOTICELOG("Backend is NOT initialized!\n");
        //
        // Empty every byte on the reserved segment
        //
        //
        char tmpPageBuf[DDS_BACKEND_PAGE_SIZE];
        memset(tmpPageBuf, 0, DDS_BACKEND_PAGE_SIZE);  // original: DDS_BACKEND_SECTOR_SIZE probably wrong
        
        size_t pagesPerSegment = DDS_BACKEND_SEGMENT_SIZE / DDS_BACKEND_PAGE_SIZE;
        size_t numPagesWritten = 0;

        //
        // Issue writes with the maximum queue depth
        //
        //
        Ctx->TargetProgress = pagesPerSegment;
        
        while (numPagesWritten < pagesPerSegment) {
            struct InitializeCtx *CallbackCtx = malloc(sizeof(*CallbackCtx));
            memcpy(CallbackCtx, Ctx, sizeof(Ctx));
            CallbackCtx->CurrentProgress = numPagesWritten + 1;
            result = WriteToDiskAsync(
                tmpPageBuf,
                0,
                (SegmentSizeT)(numPagesWritten * DDS_BACKEND_PAGE_SIZE),
                DDS_BACKEND_PAGE_SIZE,
                IncrementProgressCallback,
                CallbackCtx,
                Sto,
                Ctx->SPDKContext,
                true
            );

            if (result != DDS_ERROR_CODE_SUCCESS) {
                Ctx->FailureStatus->HasFailed = true;
                Ctx->FailureStatus->HasAborted = true;  // don't need to run callbacks anymore
                SPDK_ERRLOG("Clearing backend pages failed with %d\n", result);
                return;
            }

            /* while (Sto->CurrentProgress != Sto->TargetProgress) {
                //std::this_thread::yield();
                //not sure does this function have the same effect
                //pthread_yield() is non-standard function so I replace it by
                sched_yield();
            } */
            
            numPagesWritten += 1;
        }
        Ctx->NumPagesWritten = numPagesWritten;
        Ctx->PagesLeft = pagesPerSegment - numPagesWritten;
    }
    else {
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
    
}

//
// Initialize the backend service
// This needs to be async, so using callback chains to accomplish this
//
//
ErrorCodeT Initialize(
    struct DPUStorage* Sto,
    void *arg // NOTE: this is currently an spdkContext, but depending on the callbacks, they need different arg than this
){
    SPDKContextT *SPDKContext = arg;
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
    char tmpSectorBuf[DDS_BACKEND_SECTOR_SIZE + 1];
    memset(tmpSectorBuf, 0, DDS_BACKEND_SECTOR_SIZE + 1);  // probably better to make sure it's NUL terminated

    // read reserved sector
    struct InitializeCtx *InitializeCtx = malloc(sizeof(*InitializeCtx));
    struct InitializeFailureStatus *FailureStatus = malloc(sizeof(*FailureStatus));
    InitializeCtx->FailureStatus = FailureStatus;  // this is the master
    // InitializeCtx->HasFailed = false;
    InitializeCtx->FailureStatus->HasFailed = false;
    // InitializeCtx->HasStopped = malloc(sizeof(InitializeCtx->HasStopped));
    InitializeCtx->FailureStatus->HasStopped = false;
    // *(InitializeCtx->HasStopped) = false;
    InitializeCtx->FailureStatus->HasStopped = false;
    InitializeCtx->CurrentProgress = 0;
    InitializeCtx->TargetProgress = 0;
    InitializeCtx->SPDKContext = SPDKContext;
    result = ReadFromDiskAsync(tmpSectorBuf, DDS_BACKEND_RESERVED_SEGMENT, 0, DDS_BACKEND_SECTOR_SIZE,
        InitializeReadReservedSectorCallback, InitializeCtx, Sto, arg, true);
    // result = ReadFromDiskSync(tmpSectorBuf, DDS_BACKEND_RESERVED_SEGMENT, 0, DDS_BACKEND_SECTOR_SIZE, Sto, arg, true);

    if (result != DDS_ERROR_CODE_SUCCESS) {
        printf("Initialize read reserved sector failed!!!\n");
        return result;
    }
    return DDS_ERROR_CODE_SUCCESS;
}


// ErrorCodeT RespondWithResult(struct ControlPlaneHandlerCtx *HandlerCtx, int MsgId, ErrorCodeT Result) {
//     HandlerCtx->Resp->Result = Result;  
//     MsgHeader* msgOut = (MsgHeader*)HandlerCtx->CtrlConn->SendBuff;
//     msgOut->MsgId = MsgId;
//     HandlerCtx->CtrlConn->SendWr.sg_list->length = sizeof(MsgHeader) + sizeof(CtrlMsgB2FAckCreateDirectory);
//     int ret = ibv_post_send(HandlerCtx->CtrlConn->QPair, &HandlerCtx->CtrlConn->SendWr, &HandlerCtx->badSendWr);
//     if (ret) {
//         fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
//         return -1;
//     }
//     return Result;
// }

void CreateDirectorySyncReservedInformationToDiskCallback(struct spdk_bdev_io *bdev_io, bool Success, ContextT Context) {
    ControlPlaneHandlerCtx *HandlerCtx = Context;
    spdk_bdev_free_io(bdev_io);
    if (Success) {
        // this is basically the last line of the original CreateDirectory()
        pthread_mutex_unlock(&Sto->SectorModificationMutex);
        *(HandlerCtx->Result) = DDS_ERROR_CODE_SUCCESS;
        free(HandlerCtx);
        // at this point, we've finished all work, don't do RDMA, just return
        // RespondWithResult(HandlerCtx, CTRL_MSG_B2F_ACK_CREATE_DIR, DDS_ERROR_CODE_SUCCESS);
    }
    else {
        SPDK_ERRLOG("CreateDirectorySyncReservedInformationToDiskCallback failed\n");
        *(HandlerCtx->Result) = DDS_ERROR_CODE_IO_FAILURE;
        // RespondWithResult(HandlerCtx, CTRL_MSG_B2F_ACK_CREATE_DIR, DDS_ERROR_CODE_OUT_OF_MEMORY);
    }
    // we should free the handler ctx at the end of the last callback
    free(HandlerCtx);
}

void CreateDirectorySyncDirToDiskCallback(struct spdk_bdev_io *bdev_io, bool Success, ContextT Context) {
    ControlPlaneHandlerCtx *HandlerCtx = Context;
    spdk_bdev_free_io(bdev_io);
    if (Success) {  // continue to do work
        Sto->AllDirs[HandlerCtx->DirId] = HandlerCtx->dir;

        pthread_mutex_lock(&Sto->SectorModificationMutex);
        Sto->TotalDirs++;
        ErrorCodeT result = SyncReservedInformationToDisk(Sto, HandlerCtx->SPDKContext,
            CreateDirectorySyncReservedInformationToDiskCallback, HandlerCtx);
        if (result != 0) {  // fatal, can't continue, and the callback won't run
            SPDK_ERRLOG("SyncReservedInformationToDisk() returned %hu\n", result);
            pthread_mutex_unlock(&Sto->SectorModificationMutex);
            *(HandlerCtx->Result) = DDS_ERROR_CODE_IO_FAILURE;
            // RespondWithResult(HandlerCtx, CTRL_MSG_B2F_ACK_CREATE_DIR, DDS_ERROR_CODE_OUT_OF_MEMORY);
        }  // else, the callback passed to it should be guaranteed to run
    }
    else {
        SPDK_ERRLOG("CreateDirectorySyncDirToDisk failed, can't create directory\n");
        *(HandlerCtx->Result) = DDS_ERROR_CODE_IO_FAILURE;
        // RespondWithResult(HandlerCtx, CTRL_MSG_B2F_ACK_CREATE_DIR, DDS_ERROR_CODE_OUT_OF_MEMORY);

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
        // don't do RDMA here, only update the result
        // return RespondWithResult(HandlerCtx, CTRL_MSG_B2F_ACK_CREATE_DIR, DDS_ERROR_CODE_OUT_OF_MEMORY);
    }
    HandlerCtx->DirId = DirId;
    HandlerCtx->dir = dir;
    ErrorCodeT result = SyncDirToDisk(dir, Sto, SPDKContext, CreateDirectorySyncDirToDiskCallback, HandlerCtx);

    if (result != DDS_ERROR_CODE_SUCCESS) {
        *(HandlerCtx->Result) = DDS_ERROR_CODE_IO_FAILURE;
        return result;
        // return RespondWithResult(HandlerCtx, CTRL_MSG_B2F_ACK_CREATE_DIR, DDS_ERROR_CODE_IO_FAILURE);
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
        // at this point, we've finished all work, so we just repsond with success
        //RespondWithResult(HandlerCtx, CTRL_MSG_B2F_ACK_REMOVE_DIR, DDS_ERROR_CODE_SUCCESS);
    }
    else {
        SPDK_ERRLOG("RemoveDirectoryCallback2() failed\n");
        *(HandlerCtx->Result) = DDS_ERROR_CODE_IO_FAILURE;
        //RespondWithResult(HandlerCtx, CTRL_MSG_B2F_ACK_REMOVE_DIR, DDS_ERROR_CODE_IO_WRONG);
    }
}

void RemoveDirectoryCallback1(struct spdk_bdev_io *bdev_io, bool Success, ContextT Context) {
    ControlPlaneHandlerCtx *HandlerCtx = Context;
    spdk_bdev_free_io(bdev_io);
    if (Success) {  
        // continue to do work
        free(Sto->AllDirs[HandlerCtx->DirId]);
        Sto->AllDirs[HandlerCtx->DirId] = NULL;

        pthread_mutex_lock(&Sto->SectorModificationMutex);

        Sto->TotalDirs--;
        ErrorCodeT result = SyncReservedInformationToDisk(Sto, HandlerCtx->SPDKContext,
            RemoveDirectoryCallback2, HandlerCtx);
        if (result != 0) {  // fatal, can't continue, and the callback won't run
            SPDK_ERRLOG("RemoveDirectoryCallback1() returned %hu\n", result);
            pthread_mutex_unlock(&Sto->SectorModificationMutex);
            *(HandlerCtx->Result) = DDS_ERROR_CODE_IO_FAILURE;
        }  // else, the callback passed to it should be guaranteed to run
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

void CreateFileCallback3(struct spdk_bdev_io *bdev_io, bool Success, ContextT Context) {
    ControlPlaneHandlerCtx *HandlerCtx = Context;
    spdk_bdev_free_io(bdev_io);
    if (Success) {
        pthread_mutex_unlock(&Sto->SectorModificationMutex);
        *(HandlerCtx->Result) = DDS_ERROR_CODE_SUCCESS;
        free(HandlerCtx);
        // at this point, we've finished all work, so we just repsond with success
        //RespondWithResult(HandlerCtx, CTRL_MSG_B2F_ACK_CREATE_FILE, DDS_ERROR_CODE_SUCCESS);
    }
    else {
        SPDK_ERRLOG("CreateFileCallback3() failed\n");
        *(HandlerCtx->Result) = DDS_ERROR_CODE_IO_FAILURE;
        //RespondWithResult(HandlerCtx, CTRL_MSG_B2F_ACK_CREATE_FILE, DDS_ERROR_CODE_IO_WRONG);
    }
}

void CreateFileCallback2(struct spdk_bdev_io *bdev_io, bool Success, ContextT Context) {
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
        if (result != 0) {  // fatal, can't continue, and the callback won't run
            SPDK_ERRLOG("CreateFileCallback2() returned %hu\n", result);
            pthread_mutex_unlock(&Sto->SectorModificationMutex);
            *(HandlerCtx->Result) =  DDS_ERROR_CODE_IO_FAILURE;
        }  // else, the callback passed to it should be guaranteed to run
    }
    else {
        SPDK_ERRLOG("CreateFileCallback2() failed\n");
        *(HandlerCtx->Result) =  DDS_ERROR_CODE_IO_FAILURE;
    }
}

void CreateFileCallback1(struct spdk_bdev_io *bdev_io, bool Success, ContextT Context) {
    ControlPlaneHandlerCtx *HandlerCtx = Context;
    spdk_bdev_free_io(bdev_io);
    if (Success) {  // continue to do work
        //
        // Add this file to its directory and make the update persistent
        //
        //
        Lock(HandlerCtx->dir);
    
        AddFile(HandlerCtx->FileId, HandlerCtx->dir);
        ErrorCodeT result = SyncDirToDisk(HandlerCtx->dir, Sto, HandlerCtx->SPDKContext,
            CreateFileCallback2, HandlerCtx);
        if (result != 0) {  // fatal, can't continue, and the callback won't run
            SPDK_ERRLOG("CreateFileCallback1() returned %hu\n", result);
            Unlock(HandlerCtx->dir);
            *(HandlerCtx->Result) =  DDS_ERROR_CODE_IO_FAILURE;
        }  // else, the callback passed to it should be guaranteed to run
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
        return DDS_ERROR_CODE_DIR_NOT_FOUND;
    }

    struct DPUFile* file = BackEndFileI(FileId, FileName, FileAttributes);
    if (!file) {
        *(HandlerCtx->Result) = DDS_ERROR_CODE_OUT_OF_MEMORY;
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
        // at this point, we've finished all work, so we just repsond with success
        //RespondWithResult(HandlerCtx, CTRL_MSG_B2F_ACK_DELETE_FILE, DDS_ERROR_CODE_SUCCESS);
    }
    else {
        SPDK_ERRLOG("DeleteFileCallback3() failed\n");
        *(HandlerCtx->Result) = DDS_ERROR_CODE_IO_FAILURE;
        //RespondWithResult(HandlerCtx, CTRL_MSG_B2F_ACK_DELETE_FILE, DDS_ERROR_CODE_IO_WRONG);
    }
}

void DeleteFileCallback2(struct spdk_bdev_io *bdev_io, bool Success, ContextT Context) {
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
        if (result != 0) {  // fatal, can't continue, and the callback won't run
            SPDK_ERRLOG("DeleteFileCallback2() returned %hu\n", result);
            pthread_mutex_unlock(&Sto->SectorModificationMutex);
            *(HandlerCtx->Result) = DDS_ERROR_CODE_IO_FAILURE;
        }  // else, the callback passed to it should be guaranteed to run
    }
    else {
        SPDK_ERRLOG("DeleteFileCallback2() failed\n");
        *(HandlerCtx->Result) = DDS_ERROR_CODE_IO_FAILURE;
    }
}

void DeleteFileCallback1(struct spdk_bdev_io *bdev_io, bool Success, ContextT Context) {
    ControlPlaneHandlerCtx *HandlerCtx = Context;
    spdk_bdev_free_io(bdev_io);
    if (Success) {  // continue to do work
        //
        // Delete this file from its directory and make the update persistent
        //
        //
        Lock(HandlerCtx->dir);

        DeleteFile(HandlerCtx->FileId, HandlerCtx->dir);
        ErrorCodeT result = SyncDirToDisk(HandlerCtx->dir, Sto, HandlerCtx->SPDKContext,
            DeleteFileCallback2, HandlerCtx);
        if (result != 0) {  // fatal, can't continue, and the callback won't run
            SPDK_ERRLOG("DeleteFileCallback1() returned %hu\n", result);
            Unlock(HandlerCtx->dir);
            *(HandlerCtx->Result) = DDS_ERROR_CODE_IO_FAILURE;
        }  // else, the callback passed to it should be guaranteed to run
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
            //SegmentAllocationMutex.lock();
            pthread_mutex_lock(&Sto->SegmentAllocationMutex);

            if (numSegmentsToAllocate > Sto->AvailableSegments) {
                //SegmentAllocationMutex.unlock();
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

            //SegmentAllocationMutex.unlock();
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
            //SegmentAllocationMutex.lock();
            pthread_mutex_lock(&Sto->SegmentAllocationMutex);

            for (SegmentIdT s = 0; s != numSegmentsToDeallocate; s++) {
                DeallocateSegment(file);
            }

            //SegmentAllocationMutex.unlock();
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
    FileSizeT remainingBytes = GetSize(file) - Offset;

    FileIOSizeT bytesLeftToRead = DestBuffer->TotalSize;
    if (remainingBytes < DestBuffer->TotalSize) {
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
        FileIOSizeT firstSplitLeftToRead = DestBuffer->FirstSize - bytesRead;
        FileIOSizeT bytesLeftOnCurrSplit; // may be 1st or 2nd split, the no. of bytes left to read onto it
        FileIOSizeT bufferOffset;

        if (firstSplitLeftToRead > 0) {  // first addr
            SPDK_NOTICELOG("reading into first split, firstSplitLeftToRead: %d\n", firstSplitLeftToRead);
            DestAddr = DestBuffer->FirstAddr;
            bytesLeftOnCurrSplit = firstSplitLeftToRead;
            bufferOffset = bytesRead;
        }
        else {  // second addr
            SPDK_NOTICELOG("reading into second split, bytesLeftToRead: %d\n", bytesLeftToRead);
            DestAddr = DestBuffer->SecondAddr;
            bytesLeftOnCurrSplit = bytesLeftToRead;
            bufferOffset = bytesRead - DestBuffer->FirstSize;
        }

        bytesToIssue = min3(bytesLeftOnCurrSplit, bytesLeftToRead, remainingBytesOnCurSeg);
        SPDK_NOTICELOG("bytesToIssue: %d, bufferOffset: %d\n", bytesToIssue, bufferOffset);

        
        /* if (remainingBytesOnCurSeg >= bytesLeftToRead) {
            bytesToIssue = bytesLeftToRead;
        }
        else {
            bytesToIssue = remainingBytesOnCurSeg;
        } */

        result = ReadFromDiskAsync(
            DestAddr + bufferOffset, //(curOffset - Offset)
            curSegment,
            offsetOnSegment,
            bytesToIssue,
            Callback,
            Context,
            Sto,
            SPDKContext,
            true
        );

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

    SPDK_NOTICELOG("For RequestId %hu, # callbacks to run: %hu\n", SlotContext->Ctx->Response->RequestId, SlotContext->CallbacksToRun);
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
            //SegmentAllocationMutex.lock();
            pthread_mutex_lock(&Sto->SegmentAllocationMutex);

            if (numSegmentsToAllocate > Sto->AvailableSegments) {
                //SegmentAllocationMutex.unlock();
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

            //SegmentAllocationMutex.unlock();
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
        FileIOSizeT firstSplitLeftToWrite = SourceBuffer->FirstSize - bytesWritten;
        FileIOSizeT bytesLeftOnCurrSplit; // may be 1st or 2nd split, the no. of bytes left to write on it
        FileIOSizeT bufferOffset;
        
        if (firstSplitLeftToWrite > 0) {  // writing from first addr
            SPDK_NOTICELOG("writing from first split, firstSplitLeftToWrite: %d\n", firstSplitLeftToWrite);
            SourceAddr = SourceBuffer->FirstAddr;
            bytesLeftOnCurrSplit = firstSplitLeftToWrite;
            bufferOffset = bytesWritten;
        }
        else {  // writing from second addr
            SPDK_NOTICELOG("writing from second split, bytesLeftToWrite: %d\n", bytesLeftToWrite);
            SourceAddr = SourceBuffer->SecondAddr;
            bytesLeftOnCurrSplit = bytesLeftToWrite;
            bufferOffset = bytesWritten - SourceBuffer->FirstSize;
        }

        //
        // bytesToIssue will be the min of 3: bytesLeftOnCurrSplit, bytesLeftToWrite, remainingBytesOnCurSeg
        //
        //

        bytesToIssue = min3(bytesLeftOnCurrSplit, bytesLeftToWrite, remainingBytesOnCurSeg);
        SPDK_NOTICELOG("bytesToIssue: %d, bufferOffset: %d\n", bytesToIssue, bufferOffset);

        /* if (remainingBytesOnCurSeg >= bytesLeftToWrite) {  // everything can go onto curr seg
            bytesToIssue = bytesLeftToWrite;
            if (bytesToIssue > firstSplitLeftToWrite) {
                bytesToIssue = firstSplitLeftToWrite;
            }
        }
        else {  // write as much on curr seg now, some data will be on another seg later
            bytesToIssue = remainingBytesOnCurSeg;
        } */
        

        result = WriteToDiskAsync(
            SourceAddr + bufferOffset,//curOffset - Offset
            curSegment,
            offsetOnSegment,
            bytesToIssue,
            Callback,
            SlotContext,
            Sto,
            SPDKContext,
            true
        );

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

    SPDK_NOTICELOG("For RequestId %hu, # callbacks to run: %hu\n", SlotContext->Ctx->Response->RequestId, SlotContext->CallbacksToRun);
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
    //SegmentAllocationMutex.lock();
    pthread_mutex_lock(&Sto->SegmentAllocationMutex);

    *StorageFreeSpace = Sto->AvailableSegments * (FileSizeT)DDS_BACKEND_SEGMENT_SIZE;

    //SegmentAllocationMutex.unlock();
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
        // at this point, we've finished all work, so we just repsond with success
        //RespondWithResult(HandlerCtx, CTRL_MSG_B2F_ACK_MOVE_FILE, DDS_ERROR_CODE_SUCCESS);
    }
    else {
        SPDK_ERRLOG("RemoveDirectoryCallback2() failed\n");
        *(HandlerCtx->Result) = DDS_ERROR_CODE_IO_FAILURE;
        //RespondWithResult(HandlerCtx, CTRL_MSG_B2F_ACK_MOVE_FILE, DDS_ERROR_CODE_IO_WRONG);
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
        if (result != 0) {  // fatal, can't continue, and the callback won't run
            SPDK_ERRLOG("MoveFileCallback1() returned %hu\n", result);
            Unlock(HandlerCtx->NewDir);
            *(HandlerCtx->Result) = DDS_ERROR_CODE_IO_FAILURE;
        }  // else, the callback passed to it should be guaranteed to run
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