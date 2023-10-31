#include "DataPlaneHandlers.h"

#undef DEBUG_DATAPLANE_HANDLERS
#ifdef DEBUG_DATAPLANE_HANDLERS
#include <stdio.h>
#define DebugPrint(Fmt, ...) fprintf(stderr, Fmt, __VA_ARGS__)
#else
static inline void DebugPrint(const char* Fmt, ...) { }
#endif

// struct DPUStorage *Sto;
// SPDKContextT *SPDKContext;
uint64_t write_count = 0;

//
// Handler for a read request
//
//
void ReadHandler(
    struct PerSlotContext *SlotContext
) {
    DataPlaneRequestContext* Context = SlotContext->Ctx;
    // DebugPrint("Executing a read request: %u@%lu#%u\n", Context->Request->FileId, Context->Request->Offset, Context->Request->Bytes);

    //
    // Execute the read asynchronously
    //
    //
    /* BackEndIOContextT *IOContext = malloc(sizeof(*IOContext)); */

    /* struct PerSlotContext* SlotContext= FindFreeSpace(Context->SPDKContext, Context);
    //
    // in case there is no available space, set result to failure
    //
    //
    if(!SlotContext) {
        Context->Response->BytesServiced = 0;
        Context->Response->Result = DDS_ERROR_CODE_IO_FAILURE;
        return;
    } */

    SlotContext->CallbacksRan = 0;  // incremented in callbacks
    SlotContext->CallbacksToRun = 0;  // incremented in ReadFile when async writes are issued successfully

    ErrorCodeT ret;
    if (USE_ZERO_COPY) {
        ret = ReadFile(Context->Request->FileId, Context->Request->Offset, &Context->DataBuffer,
            ReadHandlerZCCallback, SlotContext, Sto, SlotContext->SPDKContext);
    }
    else {
        ret = ReadFile(Context->Request->FileId, Context->Request->Offset, &Context->DataBuffer,
            ReadHandlerNonZCCallback, SlotContext, Sto, SlotContext->SPDKContext);
    }
    /* ErrorCodeT ret = ReadFile(Context->Request->FileId, Context->Request->Offset, &Context->DataBuffer,
        ReadHandlerCallback, SlotContext, Sto, SlotContext->SPDKContext); */
    if (ret) {  // fatal, some callbacks won't be called
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

    /* if (SlotContext->DestBuffer->FirstSize != SlotContext->DestBuffer->TotalSize) {
        SPDK_NOTICELOG("CALLBACK FOR SG READ RUNNING, RequestId: %llu\n", SlotContext->Ctx->Response->RequestId);
    }
    else {
        // SPDK_NOTICELOG("CALLBACK FOR NORMAL READ RUNNING\n");
    } */

    spdk_bdev_free_io(bdev_io);
    SlotContext->CallbacksRan += 1;
    // SPDK_NOTICELOG("ReadHandlerZCCallback() ran\n");
    // if (SlotContext->DestBuffer->FirstSize != SlotContext->DestBuffer->TotalSize) {
    //     SPDK_NOTICELOG("ReadHandlerZCCallback() running for readv, CallbacksRan: %d, CallbacksToRun: %d\n",
    //         SlotContext->CallbacksRan, SlotContext->CallbacksToRun);
    //     SPDK_NOTICELOG("base1: %p, base1 int: %d, len1: %u, base2: %p, len2: %u\n",
    //         SlotContext->iov[0].iov_base, SlotContext->iov[0].iov_base, SlotContext->iov[0].iov_len, SlotContext->iov[1].iov_base, SlotContext->iov[1].iov_len);
    // }

    // if (SlotContext->CallbacksToRun > 1) {
    //     SPDK_NOTICELOG("CallbacksToRun: %d\n", SlotContext->CallbacksToRun);
    // }

    if (SlotContext->Ctx->Response->Result != DDS_ERROR_CODE_IO_FAILURE) {  // did not previously fail, should actually == IO_PENDING here
        if (Success) {
            if (SlotContext->CallbacksRan == SlotContext->CallbacksToRun) {  // all callbacks done and successful, mark resp success
                SlotContext->Ctx->Response->Result = DDS_ERROR_CODE_SUCCESS;
                SlotContext->Ctx->Response->BytesServiced = SlotContext->BytesIssued;
                /* SPDK_NOTICELOG("ReadHandler for RequestId %hu successful, BytesServiced: %d with %hu reads\n",
                    SlotContext->Ctx->Response->RequestId, SlotContext->BytesIssued, SlotContext->CallbacksRan); */
            }  // else this isn't the last, nothing more to do
            // else {
            //     SPDK_NOTICELOG("Intermediate ran, %hu / %hu\n", SlotContext->CallbacksRan, SlotContext->CallbacksToRun);
            // }
        }
        else {  // unsuccessful, mark resp with failure
            // SPDK_WARNLOG("failed bdev read (callback ran with fail param) encountered for RequestId %hu\n",
            //     SlotContext->Ctx->Response->RequestId);
            SlotContext->Ctx->Response->Result = DDS_ERROR_CODE_IO_FAILURE;
            SlotContext->Ctx->Response->BytesServiced = 0;
        }
    }  // else previously failed, no useful work to do
    else {
        // SPDK_ERRLOG("callback running for already failed IO, not doing anything...\n");
    }

    /* // finally if we are the last callback, do copying
    if (SlotContext->CallbacksRan == SlotContext->CallbacksToRun) {
        SPDK_NOTICELOG("last read handler callback run, total: %hu\n", SlotContext->CallbacksToRun);
        
    } */
}

void ReadHandlerNonZCCallback(
    struct spdk_bdev_io *bdev_io,
    bool Success,
    ContextT Context
) {
    struct PerSlotContext* SlotContext = Context;
    spdk_bdev_free_io(bdev_io);
    SlotContext->CallbacksRan += 1;

    if (SlotContext->Ctx->Response->Result != DDS_ERROR_CODE_IO_FAILURE) {  // did not previously fail, should actually == IO_PENDING here
        if (Success) {
            if (SlotContext->CallbacksRan == SlotContext->CallbacksToRun) {  // all callbacks done and successful, mark resp success
                // SPDK_NOTICELOG("non zero copy, copying from buff...\n");
                // non zc, we need to copy read data from our buff to the dest splittable buffer
                char *toCopy = &SlotContext->SPDKContext->buff[SlotContext->Position * DDS_BACKEND_SPDK_BUFF_BLOCK_SPACE];
                memcpy(SlotContext->DestBuffer->FirstAddr, toCopy, SlotContext->DestBuffer->FirstSize);
                toCopy += SlotContext->DestBuffer->FirstSize;
                memcpy(SlotContext->DestBuffer->SecondAddr, toCopy, SlotContext->DestBuffer->TotalSize - SlotContext->DestBuffer->FirstSize);
                SlotContext->Ctx->Response->Result = DDS_ERROR_CODE_SUCCESS;
                SlotContext->Ctx->Response->BytesServiced = SlotContext->BytesIssued;
                /* SPDK_NOTICELOG("ReadHandler for RequestId %hu successful, BytesServiced: %d with %hu reads\n",
                    SlotContext->Ctx->Response->RequestId, SlotContext->BytesIssued, SlotContext->CallbacksRan); */
            }  // else this isn't the last, nothing more to do
            // else {
            //     SPDK_NOTICELOG("Intermediate ran, %hu / %hu\n", SlotContext->CallbacksRan, SlotContext->CallbacksToRun);
            // }
        }
        else {  // unsuccessful, mark resp with failure
            SPDK_WARNLOG("failed bdev read (callback ran with fail param) encountered for RequestId %hu\n",
                SlotContext->Ctx->Response->RequestId);
            SlotContext->Ctx->Response->Result = DDS_ERROR_CODE_IO_FAILURE;
            SlotContext->Ctx->Response->BytesServiced = 0;
        }
    }  // else previously failed, no useful work to do

    /* // finally if we are the last callback, do copying
    if (SlotContext->CallbacksRan == SlotContext->CallbacksToRun) {
        SPDK_NOTICELOG("last read handler callback run, total: %hu\n", SlotContext->CallbacksToRun);
        
    } */
}

//
// Handler for a write request
//
//
void WriteHandler(
    struct PerSlotContext *SlotContext
) {
    DataPlaneRequestContext* Context = SlotContext->Ctx;
    // SPDK_NOTICELOG("Executing a write request: %u@%lu#%u, splittable buffer total size: %d\n", Context->Request->FileId,
    //     Context->Request->Offset, Context->Request->Bytes, Context->DataBuffer.TotalSize);

    SlotContext->CallbacksRan = 0;  // incremented in callbacks
    SlotContext->CallbacksToRun = 0;  // incremented in WriteFile when async writes are issued successfully

    ErrorCodeT ret = WriteFile(Context->Request->FileId, Context->Request->Offset, &Context->DataBuffer,
        WriteHandlerCallback, SlotContext, Sto, SlotContext->SPDKContext);
    if (ret) {  // fatal, some callbacks won't be called
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

    if (SlotContext->Ctx->Response->Result != DDS_ERROR_CODE_IO_FAILURE) {  // did not previously fail, should actually == IO_PENDING here
        if (Success) {
            if (SlotContext->CallbacksRan == SlotContext->CallbacksToRun) {  // all callbacks done and successful, mark resp success
                SlotContext->Ctx->Response->Result = DDS_ERROR_CODE_SUCCESS;
                SlotContext->Ctx->Response->BytesServiced = SlotContext->BytesIssued;
                // SPDK_NOTICELOG("WriteHandler for RequestId %hu successful, BytesServiced: %d with %hu writes\n",
                    // SlotContext->Ctx->Response->RequestId, SlotContext->BytesIssued, SlotContext->CallbacksRan);
            }  // else this isn't the last, nothing more to do
            else {
                // SPDK_NOTICELOG("Intermediate ran, %hu / %hu\n", SlotContext->CallbacksRan, SlotContext->CallbacksToRun);
            }
        }
        else {  // unsuccessful, mark resp with failure
            // TODO: actually getting run when zc, what happened?
            // SPDK_WARNLOG("failed bdev write (callback ran with fail param) encountered for RequestId %hu\n",
            //     SlotContext->Ctx->Response->RequestId);
            SlotContext->Ctx->Response->Result = DDS_ERROR_CODE_IO_FAILURE;
            SlotContext->Ctx->Response->BytesServiced = 0;
        }
    }  // else previously failed, no useful work to do

    // finally if we are the last callback, free the context and buffer, no matter if it failed or not
    /* if (IOContext->CallbacksRan == IOContext->CallbacksToRun) {
        SPDK_NOTICELOG("last write handler callback run, total: %hu\n", IOContext->CallbacksToRun);
        free(IOContext->SplittableBuffer);
        free(IOContext);
    } */

    // no need to free anymore, since we are using the request index
    /* if (SlotContext->CallbacksRan == SlotContext->CallbacksToRun) {
        SPDK_NOTICELOG("last write handler callback run, total: %hu\n", SlotContext->CallbacksToRun);
        // free(IOContext->SplittableBuffer);
        // free(IOContext);
        FreeSingleSpace(SlotContext);
    } */
}