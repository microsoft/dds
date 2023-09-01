#include "MsgType.h"

//
// Describe an object that might be split on the ring buffer
//
//
typedef struct {
    RingSizeT TotalSize;
    RingSizeT FirstSize;
    BufferT FirstAddr;
    BufferT SecondAddr;
} SplittableBuffer;

//
// Handler for a read request
//
//
void ReadHandler(
    BuffMsgF2BReqHeader* Req,
    BuffMsgB2FAckHeader* Resp,
    SplittableBuffer& DestBuffer
);

//
// Handler for a write request
//
//
void WriteHandler(
    BuffMsgF2BReqHeader* Req,
    BuffMsgB2FAckHeader* Resp,
    SplittableBuffer& SourceBuffer
);