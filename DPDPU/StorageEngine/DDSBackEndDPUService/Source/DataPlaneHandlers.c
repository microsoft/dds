#include <stdio.h>

#include "../Include/DataPlaneHandlers.h"
#include "../Include/DPUBackEndStorage.h"
#include "../Include/bdev.h"

struct DPUStorage *Sto;

//
// Handler for a read request
//
//
void ReadHandler(
    BuffMsgF2BReqHeader* Req,
    BuffMsgB2FAckHeader* Resp,
    SplittableBuffer* DestBuffer
) {
    printf("Executing a read request: %u@%lu#%u\n", Req->FileId, Req->Offset, Req->Bytes);

    //
    // TODO: Execute the read asynchronously
    //
    //
    // TODO: What about this Context (BackEndIOContextT)? Where do we get it? Do we create it here?
    ContextT Context;//// TODO
    SPDKContextT *SPDKContext;//// TODO
    if (Req->Bytes > DestBuffer->FirstSize){
        ErrorCodeT Tmp;
        Tmp = ReadFile(Req->FileId, Req->Offset, DestBuffer->FirstAddr, DestBuffer->FirstSize,
            ReadHandlerCallback, Context, Sto, SPDKContext);
        if (Tmp != DDS_ERROR_CODE_SUCCESS){
            Resp->Result = Tmp;
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
    Resp->BytesServiced = Req->Bytes;
}

//
// 
//
//
void ReadHandlerCallback(
    struct spdk_bdev_io *bdev_io,
    bool Success,
    ContextT Context
) {

}

//
// Handler for a write request
//
//
void WriteHandler(
    BuffMsgF2BReqHeader* Req,
    BuffMsgB2FAckHeader* Resp,
    SplittableBuffer* SourceBuffer
) {
    printf("Executing a write request: %u@%lu#%u\n", Req->FileId, Req->Offset, Req->Bytes);

    //
    // TODO: Execute the write asynchronously
    //
    //
    if (Req->Bytes > SourceBuffer->FirstSize){
        ErrorCodeT Tmp;
        // TODO: What about this Context (BackEndIOContextT)? Where do we get it? Do we create it here?
        ContextT Context;//// TODO
        SPDKContextT *SPDKContext;//// TODO
        Tmp = WriteFile(Req->FileId, Req->Offset, SourceBuffer->FirstAddr, SourceBuffer->FirstSize,
            WriteHandlerCallback, Context, Sto, SPDKContext);
        if (Tmp != DDS_ERROR_CODE_SUCCESS){
            Resp->Result = Tmp;
            Resp->BytesServiced = 0;
        }
        else{
            RingSizeT SecondSize = SourceBuffer->TotalSize - SourceBuffer->FirstSize;
            Resp->Result = WriteFile(Req->FileId, Req->Offset+SourceBuffer->FirstSize, SourceBuffer->SecondAddr, SecondSize,?,?, Sto, arg);
            Resp->BytesServiced = Req->Bytes;
        }
    }
    else{
        Resp->Result = WriteFile(Req->FileId, Req->Offset, SourceBuffer->FirstAddr, Req->Bytes,?,?, Sto, arg);
         Resp->BytesServiced = Req->Bytes;
    }

    // Again, we face the same issue in ReadHandler()
    //Resp->Result = WriteFile(Req->FileId, Req->Offset, SourceBuffer, Req->Bytes,?,?, Sto, arg);
    Resp->Result = DDS_ERROR_CODE_SUCCESS;
    Resp->BytesServiced = Req->Bytes;
}

void WriteHandlerCallback(
    struct spdk_bdev_io *bdev_io,
    bool Success,
    ContextT Context
) {

}
