#include "DataPlaneHandlers.h"

#undef DEBUG_DATAPLANE_HANDLERS
#ifdef DEBUG_DATAPLANE_HANDLERS
#include <stdio.h>
#define DebugPrint(Fmt, ...) fprintf(stderr, Fmt, __VA_ARGS__)
#else
static inline void DebugPrint(const char* Fmt, ...) { }
#endif

struct DPUStorage *Sto;
SPDKContextT *SPDKContext;
//
// Handler for a read request
//
//
void ReadHandler(
    BuffMsgF2BReqHeader* Req,
    BuffMsgB2FAckHeader* Resp,
    SplittableBufferT* DestBuffer
) {
    DebugPrint("Executing a read request: %u@%lu#%u\n", Req->FileId, Req->Offset, Req->Bytes);

    //
    // TODO: Execute the read asynchronously
    //
    //
    BackEndIOContextT *IOContext = malloc(sizeof(*IOContext));
    IOContext->IsRead = false;
    IOContext->Resp = Resp;
    IOContext->Resp->RequestId = Req->RequestId;
    IOContext->CallbacksRan = 0;  // incremented in callbacks
    IOContext->CallbacksToRun = 0;  // incremented in ReadFile when async writes are issued successfully
    IOContext->SplittableBuffer = DestBuffer;

    ErrorCodeT ret = ReadFile(Req->FileId, Req->Offset, DestBuffer, ReadHandlerCallback, IOContext, Sto, SPDKContext);
    if (ret) {  // fatal, some callbacks won't be called
        IOContext->Resp->BytesServiced = 0;
        IOContext->Resp->Result = DDS_ERROR_CODE_IO_FAILURE;
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
// TODO: ReadHandler can share the same callback with WriteHandler
//
//
void ReadHandlerCallback(
    struct spdk_bdev_io *bdev_io,
    bool Success,
    ContextT Context
) {
    BackEndIOContextT *IOContext = Context;
    spdk_bdev_free_io(bdev_io);
    IOContext->CallbacksRan += 1;

    if (IOContext->Resp->Result != DDS_ERROR_CODE_IO_FAILURE) {  // did not previously fail, should actually == IO_PENDING here
        if (Success) {
            if (IOContext->CallbacksRan == IOContext->CallbacksToRun) {  // all callbacks done and successful, mark resp success
                IOContext->Resp->Result = DDS_ERROR_CODE_SUCCESS;
                IOContext->Resp->BytesServiced = IOContext->BytesIssued;
                SPDK_NOTICELOG("ReadHandler for RequestId %hu successful, BytesServiced: %d with %hu writes\n",
                    IOContext->Resp->RequestId, IOContext->BytesIssued, IOContext->CallbacksRan);
            }
        }
        else {  // unsuccessful, mark resp with failure
            SPDK_WARNLOG("failed bdev read (callback ran with fail param) encountered for RequestId %hu\n",
                IOContext->Resp->RequestId);
            IOContext->Resp->Result = DDS_ERROR_CODE_IO_FAILURE;
            IOContext->Resp->BytesServiced = 0;
        }
    }  // else previously failed, no useful work to do

    // finally if we are the last callback, free the context and buffer, no matter if it failed or not
    if (IOContext->CallbacksRan == IOContext->CallbacksToRun) {
        SPDK_NOTICELOG("last read handler callback run, total: %hu\n", IOContext->CallbacksToRun);
        free(IOContext->SplittableBuffer);
        free(IOContext);
    }
}

//
// Handler for a write request
//
//
void WriteHandler(
    BuffMsgF2BReqHeader* Req,
    BuffMsgB2FAckHeader* Resp,
    SplittableBufferT* SourceBuffer
) {
    printf("Executing a write request: %u@%lu#%u, splittable buffer total size: %d\n", Req->FileId, Req->Offset,
        Req->Bytes, SourceBuffer->TotalSize);

    BackEndIOContextT *IOContext = malloc(sizeof(*IOContext));
    IOContext->IsRead = false;
    IOContext->Resp = Resp;
    IOContext->Resp->RequestId = Req->RequestId;
    IOContext->CallbacksRan = 0;  // incremented in callbacks
    IOContext->CallbacksToRun = 0;  // incremented in WriteFile when async writes are issued successfully
    IOContext->SplittableBuffer = SourceBuffer;

    ErrorCodeT ret = WriteFile(Req->FileId, Req->Offset, SourceBuffer, WriteHandlerCallback, IOContext, Sto, SPDKContext);
    if (ret) {  // fatal, some callbacks won't be called
        IOContext->Resp->BytesServiced = 0;
        IOContext->Resp->Result = DDS_ERROR_CODE_IO_FAILURE;
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
    BackEndIOContextT *IOContext = Context;
    spdk_bdev_free_io(bdev_io);
    IOContext->CallbacksRan += 1;

    if (IOContext->Resp->Result != DDS_ERROR_CODE_IO_FAILURE) {  // did not previously fail, should actually == IO_PENDING here
        if (Success) {
            if (IOContext->CallbacksRan == IOContext->CallbacksToRun) {  // all callbacks done and successful, mark resp success
                IOContext->Resp->Result = DDS_ERROR_CODE_SUCCESS;
                IOContext->Resp->BytesServiced = IOContext->BytesIssued;
                SPDK_NOTICELOG("WriteHandler for RequestId %hu successful, BytesServiced: %d with %hu writes\n",
                    IOContext->Resp->RequestId, IOContext->BytesIssued, IOContext->CallbacksRan);
            }
        }
        else {  // unsuccessful, mark resp with failure
            SPDK_WARNLOG("failed bdev write (callback ran with fail param) encountered for RequestId %hu\n",
                IOContext->Resp->RequestId);
            IOContext->Resp->Result = DDS_ERROR_CODE_IO_FAILURE;
            IOContext->Resp->BytesServiced = 0;
        }
    }  // else previously failed, no useful work to do

    // finally if we are the last callback, free the context and buffer, no matter if it failed or not
    if (IOContext->CallbacksRan == IOContext->CallbacksToRun) {
        SPDK_NOTICELOG("last write handler callback run, total: %hu\n", IOContext->CallbacksToRun);
        free(IOContext->SplittableBuffer);
        free(IOContext);
    }
}
