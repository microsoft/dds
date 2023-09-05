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
}
