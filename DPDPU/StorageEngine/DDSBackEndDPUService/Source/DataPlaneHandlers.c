#include "DataPlaneHandlers.h"

#undef DEBUG_DATAPLANE_HANDLERS
#ifdef DEBUG_DATAPLANE_HANDLERS
#include <stdio.h>
#define DebugPrint(Fmt, ...) fprintf(stderr, Fmt, __VA_ARGS__)
#else
static inline void DebugPrint(const char* Fmt, ...) { }
#endif

struct DPUStorage *Sto;
// SPDKContextT *SPDKContext;
//
// Handler for a read request
//
//
void ReadHandler(
    struct PerSlotContext *SlotContext
) {
    DataPlaneRequestContext* Context = SlotContext->Ctx;
    DebugPrint("Executing a read request: %u@%lu#%u\n", Context->Request->FileId, Context->Request->Offset, Context->Request->Bytes);

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

    ErrorCodeT ret = ReadFile(Context->Request->FileId, Context->Request->Offset, Context->DataBuffer,
        ReadHandlerCallback, SlotContext, Sto, SlotContext->SPDKContext);
    if (ret) {  // fatal, some callbacks won't be called
        Context->Response->BytesServiced = 0;
        Context->Response->Result = DDS_ERROR_CODE_IO_FAILURE;
        return;
    }

    ////old stuff below
    /* if (Req->Bytes > DestBuffer->FirstSize){
        ErrorCodeT ret;
        ret = ReadFile(Req->FileId, Req->Offset, DestBuffer->FirstAddr, DestBuffer->FirstSize,
            ReadHandlerCallback, Context, Sto, SPDKContext);
        if (ret != DDS_ERROR_CODE_SUCCESS){
            Resp->Result = ret;
            Resp->BytesServiced = 0;
        }
        else{
            RingSizeT SecondSize = DestBuffer->TotalSize - DestBuffer->FirstSize;
            Resp->Result = ReadFile(Req->FileId, Req->Offset+DestBuffer->FirstSize, DestBuffer->SecondAddr, SecondSize,
                ReadHandlerCallback, Context, Sto, SPDKContext);
            Resp->BytesServiced = Req->Bytes;
        }
    }
    else{
        Resp->Result = ReadFile(Req->FileId, Req->Offset, DestBuffer->FirstAddr, Req->Bytes,
            ReadHandlerCallback, Context, Sto, SPDKContext);
         Resp->BytesServiced = Req->Bytes;
    }
    // We need callback and context to pass to ReadFile()
    //Resp->Result = ReadFile(Req->FileId, Req->Offset, DestBuffer, Req->Bytes,?,?, Sto, arg);

    // We dont have any info for Resp->BytesServiced, Readfile() will return 
    // until bytesLeftToRead = 0
    Resp->Result = DDS_ERROR_CODE_SUCCESS;
    Resp->BytesServiced = Req->Bytes; */
}

//
// Similar to WriteHandler
//
//
void ReadHandlerCallback(
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
                SPDK_NOTICELOG("ReadHandler for RequestId %hu successful, BytesServiced: %d with %hu writes\n",
                    SlotContext->Ctx->Response->RequestId, SlotContext->BytesIssued, SlotContext->CallbacksRan);
            }  // else this isn't the last, nothing more to do
            else {
                SPDK_NOTICELOG("Intermediate ran, %hu / %hu\n", SlotContext->CallbacksRan, SlotContext->CallbacksToRun);
            }
        }
        else {  // unsuccessful, mark resp with failure
            SPDK_WARNLOG("failed bdev read (callback ran with fail param) encountered for RequestId %hu\n",
                SlotContext->Ctx->Response->RequestId);
            SlotContext->Ctx->Response->Result = DDS_ERROR_CODE_IO_FAILURE;
            SlotContext->Ctx->Response->BytesServiced = 0;
        }
    }  // else previously failed, no useful work to do

    /* // finally if we are the last callback, free the context and buffer, no matter if it failed or not
    if (SlotContext->CallbacksRan == SlotContext->CallbacksToRun) {
        SPDK_NOTICELOG("last read handler callback run, total: %hu\n", SlotContext->CallbacksToRun);
        // free(IOContext->SplittableBuffer);
        // free(IOContext);
        FreeSingleSpace(SlotContext);
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
    printf("Executing a write request: %u@%lu#%u, splittable buffer total size: %d\n", Context->Request->FileId,
    Context->Request->Offset, Context->Request->Bytes, Context->DataBuffer->TotalSize);

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

    // BackEndIOContextT *IOContext = malloc(sizeof(*IOContext));
    // SlotContext->IsRead = false;  // unused??

    SlotContext->CallbacksRan = 0;  // incremented in callbacks
    SlotContext->CallbacksToRun = 0;  // incremented in WriteFile when async writes are issued successfully

    ErrorCodeT ret = WriteFile(Context->Request->FileId, Context->Request->Offset, Context->DataBuffer,
        WriteHandlerCallback, SlotContext, Sto, SlotContext->SPDKContext);
    if (ret) {  // fatal, some callbacks won't be called
        Context->Response->BytesServiced = 0;
        Context->Response->Result = DDS_ERROR_CODE_IO_FAILURE;
        return;
    }


    //// old stuff below
    /* if (Req->Bytes > SourceBuffer->FirstSize){
        ErrorCodeT ret;
        
        ret = WriteFile(Req->FileId, Req->Offset, SourceBuffer->FirstAddr, SourceBuffer->FirstSize,
            WriteHandlerCallback, IOContext, Sto, SPDKContext);
        if (ret != DDS_ERROR_CODE_SUCCESS){
            Resp->Result = ret;
            Resp->BytesServiced = 0;
        }
        else{
            RingSizeT SecondSize = SourceBuffer->TotalSize - SourceBuffer->FirstSize;
            Resp->Result = WriteFile(Req->FileId, Req->Offset+SourceBuffer->FirstSize, SourceBuffer->SecondAddr, SecondSize,?,?, NULL, NULL);
            Resp->BytesServiced = Req->Bytes;
        }
    }
    else{
        Resp->Result = WriteFile(Req->FileId, Req->Offset, SourceBuffer->FirstAddr, Req->Bytes,?,?, NULL, NULL);
         Resp->BytesServiced = Req->Bytes;
    }

    // Again, we face the same issue in ReadHandler()
    //Resp->Result = WriteFile(Req->FileId, Req->Offset, SourceBuffer, Req->Bytes,?,?, Sto, arg);
    Resp->Result = DDS_ERROR_CODE_SUCCESS;
    Resp->BytesServiced = Req->Bytes; */
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
                SPDK_NOTICELOG("WriteHandler for RequestId %hu successful, BytesServiced: %d with %hu writes\n",
                    SlotContext->Ctx->Response->RequestId, SlotContext->BytesIssued, SlotContext->CallbacksRan);
            }  // else this isn't the last, nothing more to do
            else {
                SPDK_NOTICELOG("Intermediate ran, %hu / %hu\n", SlotContext->CallbacksRan, SlotContext->CallbacksToRun);
            }
        }
        else {  // unsuccessful, mark resp with failure
            SPDK_WARNLOG("failed bdev write (callback ran with fail param) encountered for RequestId %hu\n",
                SlotContext->Ctx->Response->RequestId);
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