#pragma once

#include <stdatomic.h>

#include "DDSBackEndTypes.h"
#include "Protocol.h"

//
// A ring buffer for exchanging requests between DPU threads
//
//
struct RequestRingBuffer{
    _Atomic int Progress[DDS_CACHE_LINE_SIZE_BY_INT];
    _Atomic int Tail[DDS_CACHE_LINE_SIZE_BY_INT];
    int Head[DDS_CACHE_LINE_SIZE_BY_INT];
    char Buffer[DDS_REQUEST_RING_BYTES];
};

//
// Allocate a request buffer object
//
//
RequestRingBuffer*
AllocateRequestBuffer(
    BufferT BufferAddress
);

//
// Deallocate a request buffer object
//
//
void
DeallocateRequestBuffer(
    RequestRingBuffer* RingBuffer
);

//
// Insert a data-plane request into the request buffer
// Data-plane requests only
//
//
uint8_t
InsertRequest(
    RequestRingBuffer* RingBuffer,
    BuffMsgF2BReqHeader* Request,
    BuffMsgB2FAckHeader* Response,
    SplittableBufferT* DataBuffer
);

//
// Fetch requests from the request buffer
//
//
uint8_t
FetchRequests(
    RequestRingBuffer* RingBuffer,
    SplittableBufferT* RequestBatch
);

//
// Increment the progress
//
//
void
IncrementProgress(
    RequestRingBuffer* RingBuffer,
    FileIOSizeT TotalBytes
);