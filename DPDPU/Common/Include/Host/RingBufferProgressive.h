#pragma once

#include <atomic>

#include "DDSFrontEndInterface.h"

namespace DDS_FrontEnd {

template <class C>
using Atomic = std::atomic<C>;

//
// **************************************************************
// Request buffer
// **************************************************************
//

//
// A ring buffer for exchanging requests from the host to the DPU;
// This object should be allocated from the DMA area;
// All members are cache line aligned to avoid false sharing between threads
//
//
struct RequestRingBufferProgressive{
    Atomic<int> Progress[DDS_CACHE_LINE_SIZE_BY_INT];
    Atomic<int> Tail[DDS_CACHE_LINE_SIZE_BY_INT];
    int Head[DDS_CACHE_LINE_SIZE_BY_INT];
    char Buffer[DDS_REQUEST_RING_BYTES];
};

//
// Allocate a request buffer object
//
//
RequestRingBufferProgressive*
AllocateRequestBufferProgressive(
    BufferT BufferAddress
);

//
// Deallocate a request buffer object
//
//
void
DeallocateRequestBufferProgressive(
    RequestRingBufferProgressive* RingBuffer
);

//
// Insert a request into the request buffer
//
//
bool
InsertToRequestBufferProgressive(
    RequestRingBufferProgressive* RingBuffer,
    const BufferT CopyFrom,
    FileIOSizeT RequestSize
);

//
// Insert a WriteFile request into the request buffer
//
//
bool
InsertWriteFileRequest(
    RequestRingBufferProgressive* RingBuffer,
    RequestIdT RequestId,
    FileIdT FileId,
    FileSizeT Offset,
    FileIOSizeT Bytes,
    BufferT SourceBuffer
);

//
// Insert a WriteFileGather request into the request buffer
//
//
bool
InsertWriteFileGatherRequest(
    RequestRingBufferProgressive* RingBuffer,
    RequestIdT RequestId,
    FileIdT FileId,
    FileSizeT Offset,
    FileIOSizeT Bytes,
    BufferT* SourceBufferArray
);

//
// Insert a ReadFile or ReadFileScatter request into the request buffer
//
//
bool
InsertReadRequest(
    RequestRingBufferProgressive* RingBuffer,
    RequestIdT RequestId,
    FileIdT FileId,
    FileSizeT Offset,
    FileIOSizeT Bytes
);

//
// Fetch requests from the request buffer
//
//
bool
FetchFromRequestBufferProgressive(
    RequestRingBufferProgressive* RingBuffer,
    BufferT CopyTo,
    FileIOSizeT* RequestSize
);

//
// Parse a request from copied data
// Note: RequestSize is greater than the actual request size
//
//
void
ParseNextRequestProgressive(
    BufferT CopyTo,
    FileIOSizeT TotalSize,
    BufferT* RequestPointer,
    FileIOSizeT* RequestSize,
    BufferT* StartOfNext,
    FileIOSizeT* RemainingSize
);

//
// Wait for completion
//
//
bool
CheckForRequestCompletionProgressive(
    RequestRingBufferProgressive* RingBuffer
);

//
// **************************************************************
// Response buffer
// **************************************************************
//

//
// A ring buffer for exchanging responses from the DPU to the host;
// This object should be allocated from the DMA area;
// All members are cache line aligned to avoid false sharing between threads
//
//
struct ResponseRingBufferProgressive{
    Atomic<int> Progress[DDS_CACHE_LINE_SIZE_BY_INT];
    Atomic<int> Head[DDS_CACHE_LINE_SIZE_BY_INT];
    int Tail[DDS_CACHE_LINE_SIZE_BY_INT];
    char Buffer[DDS_RESPONSE_RING_BYTES];
};

//
// Allocate a response buffer object
//
//
ResponseRingBufferProgressive*
AllocateResponseBufferProgressive(
    BufferT BufferAddress
);

//
// Deallocate a response buffer object
//
//
void
DeallocateResponseBufferProgressive(
    ResponseRingBufferProgressive* RingBuffer
);

//
// Fetch responses from the response buffer
//
//
bool
FetchFromResponseBufferProgressive(
    ResponseRingBufferProgressive* RingBuffer,
    BufferT CopyTo,
    FileIOSizeT* ResponseSize
);

//
// Fetch a response from the response buffer
//
//
bool
FetchResponse(
    ResponseRingBufferProgressive* RingBuffer,
    BuffMsgB2FAckHeader** Response,
    SplittableBufferT* DataBuffer
);

#ifdef RING_BUFFER_RESPONSE_BATCH_ENABLED
//
// Fetch a batch of responses from the response buffer
//
//
bool
FetchResponseBatch(
    ResponseRingBufferProgressive* RingBuffer,
    SplittableBufferT* Responses
);
#endif

//
// Increment the progress
//
//
void
IncrementProgress(
    ResponseRingBufferProgressive* RingBuffer,
    FileIOSizeT ResponseSize
);

//
// Insert a response into the response buffer
//
//
bool
InsertToResponseBufferProgressive(
    ResponseRingBufferProgressive* RingBuffer,
    const BufferT* CopyFromList,
    FileIOSizeT* ResponseSizeList,
    int NumResponses,
    int* NumResponsesInserted
);

//
// Parse a response from copied data
// Note: ResponseSize is greater than the actual Response size
//
//
void
ParseNextResponseProgressive(
    BufferT CopyTo,
    FileIOSizeT TotalSize,
    BufferT* ResponsePointer,
    FileIOSizeT* ResponseSize,
    BufferT* StartOfNext,
    FileIOSizeT* RemainingSize
);

//
// Wait for completion
//
//
bool
CheckForResponseCompletionProgressive(
    ResponseRingBufferProgressive* RingBuffer
);

}