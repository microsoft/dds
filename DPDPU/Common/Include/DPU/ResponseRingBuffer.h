#pragma once

#include "DDSBackEndTypes.h"
#include "Protocol.h" 

//
// A ring buffer for storing read responses, owned by a network thread that performs offloading
// Tail[0] - TailA
// Tail[1] - TailB
// Tail[2] - TailC
//
//
struct ResponseRingBuffer{
    int Tail[DDS_CACHE_LINE_SIZE_BY_INT];
    char Buffer[DDS_INTRA_BACKEND_RESPONSE_RING_BYTES];
};

//
// Allocate a response buffer object
//
//
ResponseRingBuffer*
AllocateResponseBuffer(
    BufferT BufferAddress
);

//
// Deallocate a response buffer object
//
//
void
DeallocateResponseBuffer(
    ResponseRingBuffer* RingBuffer
);