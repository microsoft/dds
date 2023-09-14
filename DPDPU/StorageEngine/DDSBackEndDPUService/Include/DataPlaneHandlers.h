#include "MsgType.h"

//
// Handler for a read request
//
//
void ReadHandler(
    BuffMsgF2BReqHeader* Req,
    BuffMsgB2FAckHeader* Resp,
    SplittableBufferT* DestBuffer
);

//
// Handler for a write request
//
//
void WriteHandler(
    BuffMsgF2BReqHeader* Req,
    BuffMsgB2FAckHeader* Resp,
    SplittableBufferT* SourceBuffer
);
