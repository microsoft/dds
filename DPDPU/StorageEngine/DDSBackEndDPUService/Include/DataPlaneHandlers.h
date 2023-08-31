#include "../../../Common/Include/MsgType.h"
#include "DPUBackEndStorage.h"

//
// Handler for a read request
//
//
void ReadHandler(
    BuffMsgF2BReqHeader* Req,
    BuffMsgB2FAckHeader* Resp,
    BufferT DestBuffer,
    struct DPUStorage* Sto,
    void *arg
);

//
// Handler for a write request
//
//
void WriteHandler(
    BuffMsgF2BReqHeader* Req,
    BuffMsgB2FAckHeader* Resp,
    BufferT SourceBuffer,
    struct DPUStorage* Sto,
    void *arg
);