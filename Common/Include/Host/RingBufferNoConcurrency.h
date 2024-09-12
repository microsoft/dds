/*
 * Copyright (c) Microsoft Corporation.
 * Licensed under the MIT License
 */

#pragma once

#include <atomic>

#include "DDSFrontEndInterface.h"

namespace DDS_FrontEnd {

template <class C>
using Atomic = std::atomic<C>;

//
// A ring buffer for message exchanges between front and back ends
// This object should be allocated from the DMA area
// All members are cache line aligned to avoid false sharing between threads
// The buffer fails to support concurrent producers
//
//
struct RequestRingBuffer{
    Atomic<int> Tail[DDS_CACHE_LINE_SIZE_BY_INT];
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
// Insert a request into the request buffer
//
//
bool
InsertToRequestBuffer(
    RequestRingBuffer* RingBuffer,
    const BufferT CopyFrom,
    FileIOSizeT RequestSize
);

//
// Fetch requests from the request buffer
//
//
bool
FetchFromRequestBuffer(
    RequestRingBuffer* RingBuffer,
    BufferT CopyTo,
    FileIOSizeT* RequestSize
);

//
// Parse a request from copied data
// Note: RequestSize is greater than the actual request size and is cache line aligned
//
//
void
ParseNextRequest(
    BufferT CopyTo,
    FileIOSizeT TotalSize,
    BufferT* RequestPointer,
    FileIOSizeT* RequestSize,
    BufferT* StartOfNext,
    FileIOSizeT* RemainingSize
);

}