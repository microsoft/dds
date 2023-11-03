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

    SlotContext->CallbacksRan = 0;  // incremented in callbacks
    SlotContext->CallbacksToRun = 0;  // incremented in ReadFile when async writes are issued successfully

    ErrorCodeT ret;
#ifdef FILE_SERVICE_USE_ZERO_COPY
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
// Handler for a write request
//
//
void WriteHandler(
    struct PerSlotContext *SlotContext
) {
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
            // TODO: actually getting run when zc, what happened?
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