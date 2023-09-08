#include "MsgType.h"
#include <stdbool.h>

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
    SplittableBuffer* DestBuffer
);

//
// Callback for ReadHandler async read
//
//
void ReadHandlerCallback(
    struct spdk_bdev_io *bdev_io,
    bool Success,
    ContextT Context
);

//
// Handler for a write request
//
//
void WriteHandler(
    BuffMsgF2BReqHeader* Req,
    BuffMsgB2FAckHeader* Resp,
    SplittableBuffer* SourceBuffer
);

//
// Callback for WriteHandler async write
//
//
void WriteHandlerCallback(
    struct spdk_bdev_io *bdev_io,
    bool Success,
    ContextT Context
);
