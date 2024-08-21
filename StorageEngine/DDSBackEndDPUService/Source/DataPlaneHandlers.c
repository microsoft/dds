#include "DataPlaneHandlers.h"
#include "CacheTable.h"

#undef DEBUG_DATAPLANE_HANDLERS
#ifdef DEBUG_DATAPLANE_HANDLERS
#include <stdio.h>
#define DebugPrint(Fmt, ...) fprintf(stderr, Fmt, __VA_ARGS__)
#else
static inline void DebugPrint(const char* Fmt, ...) { }
#endif

//
// The global cache table
//
//
CacheTableT* CacheTable;

void DataPlaneRequestHandler(
    void* Ctx
) {
    struct PerSlotContext* HeadSlotContext = (struct PerSlotContext*) Ctx;  // head of batch
    RequestIdT batchSize = HeadSlotContext->BatchSize;
    RequestIdT ioSlotBase = HeadSlotContext->IndexBase;
            
    // iterate through all in the batch and call the corresponding RW handler, handles wrap around indices
    for (RequestIdT i = 0; i < batchSize; i++) {
        RequestIdT selfIndex = ioSlotBase + (HeadSlotContext->Position - ioSlotBase + i) % DDS_MAX_OUTSTANDING_IO;
        struct PerSlotContext* ThisSlotContext = &HeadSlotContext->SPDKContext->SPDKSpace[selfIndex];
        if (ThisSlotContext->Ctx->IsRead) {
            ReadHandler(ThisSlotContext);
        }
        else {
            WriteHandler(ThisSlotContext);
        }
    }
}

//
// Handler for a read request
//
//
void ReadHandler(
    void* Ctx
) {
    struct PerSlotContext* SlotContext = (struct PerSlotContext*)Ctx;
    DataPlaneRequestContext* Context = SlotContext->Ctx;

    SlotContext->CallbacksRan = 0;  // incremented in callbacks
    SlotContext->CallbacksToRun = 0;  // incremented in ReadFile when async writes are issued successfully

    ErrorCodeT ret;
#ifdef OPT_FILE_SERVICE_ZERO_COPY
    ret = ReadFile(Context->Request->FileId, Context->Request->Offset, &Context->DataBuffer,
        ReadHandlerZCCallback, SlotContext, Sto, SlotContext->SPDKContext);
#else
    ret = ReadFile(Context->Request->FileId, Context->Request->Offset, &Context->DataBuffer,
        ReadHandlerNonZCCallback, SlotContext, Sto, SlotContext->SPDKContext);
#endif

    if (ret) {
        //
        // Fatal, some callbacks won't be called
        //
        //
        SPDK_ERRLOG("ReadFile() Failed: %d\n", ret);
        Context->Response->BytesServiced = 0;
        Context->Response->Result = DDS_ERROR_CODE_IO_FAILURE;
        return;
    }
}

//
// Similar to WriteHandler
//
//
void ReadHandlerZCCallback(
    struct spdk_bdev_io *bdev_io,
    bool Success,
    ContextT Context
) {
    struct PerSlotContext* SlotContext = Context;

    spdk_bdev_free_io(bdev_io);
    SlotContext->CallbacksRan += 1;
    
    if (SlotContext->Ctx->Response->Result != DDS_ERROR_CODE_IO_FAILURE) {
        //
        // Did not previously fail, should actually == IO_PENDING here
        //
        //
        if (Success) {
            if (SlotContext->CallbacksRan == SlotContext->CallbacksToRun) {
                //
                // All callbacks done and successful, mark resp success
                //
                //
                SlotContext->Ctx->Response->Result = DDS_ERROR_CODE_SUCCESS;
                SlotContext->Ctx->Response->BytesServiced = SlotContext->BytesIssued;
            }
            //
            // else this isn't the last, nothing more to do
            //
            //
        }
        else {
            //
            // Unsuccessful, mark resp with failure
            //
            //
            SlotContext->Ctx->Response->Result = DDS_ERROR_CODE_IO_FAILURE;
            SlotContext->Ctx->Response->BytesServiced = 0;
        }
    }
    //
    // Else previously failed, no useful work to do
    //
    //
}

void ReadHandlerNonZCCallback(
    struct spdk_bdev_io *bdev_io,
    bool Success,
    ContextT Context
) {
    struct PerSlotContext* SlotContext = Context;
    spdk_bdev_free_io(bdev_io);
    SlotContext->CallbacksRan += 1;

    if (SlotContext->Ctx->Response->Result != DDS_ERROR_CODE_IO_FAILURE) {
        //
        // Did not previously fail, should actually == IO_PENDING here
        //
        //
        if (Success) {
            if (SlotContext->CallbacksRan == SlotContext->CallbacksToRun) {
                //
                // All callbacks done and successful, mark resp success
                // Non zc, we need to copy read data from our buff to the dest splittable buffer
                //
                //
                char *toCopy = &SlotContext->SPDKContext->buff[SlotContext->Position * DDS_BACKEND_SPDK_BUFF_BLOCK_SPACE];
                memcpy(SlotContext->DestBuffer->FirstAddr, toCopy, SlotContext->DestBuffer->FirstSize);
                toCopy += SlotContext->DestBuffer->FirstSize;
                memcpy(SlotContext->DestBuffer->SecondAddr, toCopy, SlotContext->DestBuffer->TotalSize - SlotContext->DestBuffer->FirstSize);
                SlotContext->Ctx->Response->Result = DDS_ERROR_CODE_SUCCESS;
                SlotContext->Ctx->Response->BytesServiced = SlotContext->BytesIssued;
            }
            //
            // Else this isn't the last, nothing more to do
            //
            //
        }
        else {
            //
            // Unsuccessful, mark resp with failure
            //
            //
            SPDK_WARNLOG("failed bdev read (callback ran with fail param) encountered for RequestId %hu\n",
                SlotContext->Ctx->Response->RequestId);
            SlotContext->Ctx->Response->Result = DDS_ERROR_CODE_IO_FAILURE;
            SlotContext->Ctx->Response->BytesServiced = 0;
        }
    }
    //
    // Else previously failed, no useful work to do
    //
    //
}

//
// The cache on write function called in WriteHandler
//
//
/*
void CacheOnWrite(
    SplittableBufferT *WriteBuffer,
    FileSizeT Offset,
    FileIdT FileId
) {
    int ret;
    // the offset within the write, of a record
    RingSizeT offsetInBuffer = Offset == 0 ? 64 : 0;  // for some reason the very first 64 bytes are unused (should == 0)

    uint64_t recordKey;
    uint64_t recordValue;

    CacheItemT cacheItem;
    char* recordKeyPtr;
    char* recordValuePtr;
    RingSizeT recordKeyLocation;
    RingSizeT recordValueLocation;

    RingSizeT recordKeyOffset;
    RingSizeT recordValueOffset;

    while (offsetInBuffer + 24 < WriteBuffer->TotalSize) {
        recordKeyLocation = Offset + offsetInBuffer + sizeof(uint64_t);
        recordValueLocation = recordKeyLocation + sizeof(uint64_t);

        //
        // in case it's split buffer
        //
        //
        recordKeyOffset = offsetInBuffer + sizeof(uint64_t);  // skip the record header
        recordKeyPtr = recordKeyOffset < WriteBuffer->FirstSize ? WriteBuffer->FirstAddr + recordKeyOffset : 
            WriteBuffer->SecondAddr + (recordKeyOffset - WriteBuffer->FirstSize);
        
        recordValueOffset = recordKeyOffset + sizeof(uint64_t);
        recordValuePtr = recordValueOffset < WriteBuffer->FirstSize ? WriteBuffer->FirstAddr + recordValueOffset : 
            WriteBuffer->SecondAddr + (recordValueOffset - WriteBuffer->FirstSize);

        // if an uint64 is actually split in mem
        if (recordKeyOffset < WriteBuffer->FirstSize && recordKeyOffset + sizeof(uint64_t) > WriteBuffer->FirstSize) {
            printf("split buffer, key is split\n");
            RingSizeT firstPartLen = WriteBuffer->FirstSize - recordKeyOffset;
            memcpy(&recordKey, recordKeyPtr, firstPartLen);
            memcpy(&recordKey + firstPartLen, recordKeyPtr + firstPartLen, sizeof(uint64_t) - firstPartLen);
        }
        else {  // not split
            recordKey = *((uint64_t*) recordKeyPtr);
        }

        if (recordValueOffset < WriteBuffer->FirstSize && recordValueOffset + sizeof(uint64_t) > WriteBuffer->FirstSize) {
            printf("split buffer, value is split\n");
            RingSizeT firstPartLen = WriteBuffer->FirstSize - recordValueOffset;
            memcpy(&recordValue, recordValuePtr, firstPartLen);
            memcpy(&recordValue + firstPartLen, recordValuePtr + firstPartLen, sizeof(uint64_t) - firstPartLen);
        }
        else {
            recordValue = *((uint64_t*) recordValuePtr);
        }

        if (recordValue != 42) {
            printf("!!!!!!!! ERROR CacheOnWrite: recordValue %llu != 42, recordValueOffset: %u\n", recordValue, recordValueOffset);
        }

        cacheItem->Version = 0;
        cacheItem->Key = recordKey;
        cacheItem->FileId = FileId;
        cacheItem->Offset = recordValueLocation;
        cacheItem->Size = sizeof(uint64_t);

        if (AddToCacheTable(cacheItem)) {
            printf("cache on write AddToCacheTable() FAILED!!!\n");
        }

        offsetInBuffer += 24;  // a record consists of 3 uint64's
    }
}
*/


//
// Handler for a write request
//
//
void WriteHandler(
    void* Ctx
) {
    struct PerSlotContext* SlotContext = (struct PerSlotContext*)Ctx;
    DataPlaneRequestContext* Context = SlotContext->Ctx;

    SlotContext->CallbacksRan = 0;  // incremented in callbacks
    SlotContext->CallbacksToRun = 0;  // incremented in WriteFile when async writes are issued successfully

    ErrorCodeT ret = WriteFile(Context->Request->FileId, Context->Request->Offset, &Context->DataBuffer,
        WriteHandlerCallback, SlotContext, Sto, SlotContext->SPDKContext);
    if (ret) {
        //
        // Fatal, some callbacks won't be called
        //
        //
        SPDK_ERRLOG("WriteFile failed: %d\n", ret);
        Context->Response->BytesServiced = 0;
        Context->Response->Result = DDS_ERROR_CODE_IO_FAILURE;
        return;
    }
}

//
// To continue the work of Update response buffer tail
//
//
void WriteHandlerCallback(
    struct spdk_bdev_io *bdev_io,
    bool Success,
    ContextT Context
) {
    struct PerSlotContext* SlotContext = Context;
    spdk_bdev_free_io(bdev_io);
    SlotContext->CallbacksRan += 1;

    if (SlotContext->Ctx->Response->Result != DDS_ERROR_CODE_IO_FAILURE) {
        //
        // Did not previously fail, should actually == IO_PENDING here
        //
        //
        if (Success) {
            if (SlotContext->CallbacksRan == SlotContext->CallbacksToRun) {
                //
                // All callbacks done and successful, mark resp success
                //
                //
                SlotContext->Ctx->Response->Result = DDS_ERROR_CODE_SUCCESS;
                SlotContext->Ctx->Response->BytesServiced = SlotContext->BytesIssued;
            }
            //
            // Else this isn't the last, nothing more to do
            //
            //
        }
        else {
            //
            // Unsuccessful, mark resp with failure
            //
            //
            SlotContext->Ctx->Response->Result = DDS_ERROR_CODE_IO_FAILURE;
            SlotContext->Ctx->Response->BytesServiced = 0;
        }
    }
    //
    // Else previously failed, no useful work to do
    //
    //
}
