#include <stdio.h>

#include "DataPlaneHandlers.h"

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
    // We need callback and context to pass to ReadFile()
    //Resp->Result = ReadFile(Req->FileId, Req->Offset, DestBuffer, Req->Bytes,?,?, Sto, arg);

    // We dont have any info for Resp->BytesServiced, Readfile() will return 
    // until bytesLeftToRead = 0
    Resp->Result = DDS_ERROR_CODE_SUCCESS;
    Resp->BytesServiced = Req->Bytes;
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

    // Again, we face the same issue in ReadHandler()
    //Resp->Result = WriteFile(Req->FileId, Req->Offset, SourceBuffer, Req->Bytes,?,?, Sto, arg);
    Resp->Result = DDS_ERROR_CODE_SUCCESS;
    Resp->BytesServiced = Req->Bytes;
}
