#pragma once

#include <iostream>

#include "RingBufferProgressive.h"

namespace DDS_FrontEnd {

//
// Allocate a request buffer object
//
//
RequestRingBufferProgressive*
AllocateRequestBufferProgressive(
    BufferT BufferAddress
) {
    RequestRingBufferProgressive* ringBuffer = (RequestRingBufferProgressive*)BufferAddress;

    //
    // Align the buffer by cache line size
    //
    //
    size_t ringBufferAddress = (size_t)ringBuffer;
    while (ringBufferAddress % DDS_CACHE_LINE_SIZE != 0) {
        ringBufferAddress++;
    }
    ringBuffer = (RequestRingBufferProgressive*)ringBufferAddress;

    memset(ringBuffer, 0, sizeof(RequestRingBufferProgressive));

    return ringBuffer;
}

//
// Deallocate a request buffer object
//
//
void
DeallocateRequestBufferProgressive(
    RequestRingBufferProgressive* RingBuffer
) {
    memset(RingBuffer, 0, sizeof(RequestRingBufferProgressive));
}

//
// Insert a request into the request buffer
//
//
bool
InsertToRequestBufferProgressive(
    RequestRingBufferProgressive* RingBuffer,
    const BufferT CopyFrom,
    FileIOSizeT RequestSize
) {
    //
    // Check if the tail exceeds the allowable advance
    //
    //
    int tail = RingBuffer->Tail[0];
    int head = RingBuffer->Head[0];
    RingSizeT distance = 0;

    if (tail < head) {
        distance = tail + DDS_REQUEST_RING_BYTES - head;
    }
    else {
        distance = tail - head;
    }

    if (distance >= RING_BUFFER_ALLOWABLE_TAIL_ADVANCEMENT) {
        return false;
    }

    //
    // Align the request to cache line size to avoid false sharing;
    // Append request size to the beginning of the request;
    //
    //
    FileIOSizeT requestBytes = sizeof(FileIOSizeT) + RequestSize;
    while (requestBytes % DDS_CACHE_LINE_SIZE != 0) {
        requestBytes++;
    }

    if (requestBytes > DDS_REQUEST_RING_BYTES - distance) {
        return false;
    }

    while (RingBuffer->Tail[0].compare_exchange_weak(tail, (tail + requestBytes) % DDS_REQUEST_RING_BYTES) == false) {
        tail = RingBuffer->Tail[0];
        head = RingBuffer->Head[0];

        //
        // Check if the tail exceeds the allowable advance
        //
        //
        tail = RingBuffer->Tail[0];
        head = RingBuffer->Head[0];

        if (tail <= head) {
            distance = tail + DDS_REQUEST_RING_BYTES - head;
        }
        else {
            distance = tail - head;
        }

        if (distance >= RING_BUFFER_ALLOWABLE_TAIL_ADVANCEMENT) {
            return false;
        }

        //
        // Check space
        //
        //
        if (requestBytes > DDS_REQUEST_RING_BYTES - distance) {
            return false;
        }
    }

    //
    // Now, both tail and space are good
    //
    //
    if (tail + requestBytes <= DDS_REQUEST_RING_BYTES) {
        char* requestAddress = &RingBuffer->Buffer[tail];

        //
        // Write the number of bytes in this request
        //
        //
        *((FileIOSizeT*)requestAddress) = requestBytes;

        //
        // Write the request
        //
        //
        memcpy(requestAddress + sizeof(FileIOSizeT), CopyFrom, RequestSize);

        //
        // Increment the progress
        //
        //
        int progress = RingBuffer->Progress[0];
        while (RingBuffer->Progress[0].compare_exchange_weak(progress, (progress + requestBytes) % DDS_REQUEST_RING_BYTES) == false) {
            progress = RingBuffer->Progress[0];
        }
    }
    else {
        //
        // We need to wrap the buffer around
        //
        //
        RingSizeT remainingBytes = DDS_REQUEST_RING_BYTES - tail - sizeof(FileIOSizeT);;
        char* requestAddress1 = &RingBuffer->Buffer[tail];
        char* requestAddress2 = &RingBuffer->Buffer[0];

        //
        // Write the number of bytes in this request
        //
        //
        *((FileIOSizeT*)requestAddress1) = requestBytes;

        //
        // Write the request to two memory locations
        //
        //
        if (RequestSize <= remainingBytes) {
            memcpy(requestAddress1 + sizeof(FileIOSizeT), CopyFrom, RequestSize);
        }
        else {
            memcpy(requestAddress1 + sizeof(FileIOSizeT), CopyFrom, remainingBytes);
            memcpy(requestAddress2, (const char*)CopyFrom + remainingBytes, RequestSize - remainingBytes);
        }

        //
        // Increment the progress
        //
        //
        int progress = RingBuffer->Progress[0];
        while (RingBuffer->Progress[0].compare_exchange_weak(progress, (progress + requestBytes) % DDS_REQUEST_RING_BYTES) == false) {
            progress = RingBuffer->Progress[0];
        }
    }

    return true;
}

//
// Fetch requests from the request buffer
//
//
bool
FetchFromRequestBufferProgressive(
    RequestRingBufferProgressive* RingBuffer,
    BufferT CopyTo,
    FileIOSizeT* RequestSize
) {
    //
    // In order to make this ring buffer to be safe, we must maintain the invariant below:
    // Each producer moves tail before it increments the progress.
    // Every producer maintains this invariant:
    // They (1) advance the tail,
    //      (2) insert the request, and
    //      (3) increment the progress.
    // However, the order of reading progress and tail at the consumer matters.
    // If the consumer reads the tail first, then it's possible that
    // before it reads the progress, a producer performs all three steps above
    // and thus the progress is updated.
    //
    //
    int progress = RingBuffer->Progress[0];
    int tail = RingBuffer->Tail[0];
    int head = RingBuffer->Head[0];

    //
    // Check if there are requests
    //
    //
    if (tail == head) {
        return false;
    }

    //
    // Check if requests are safe to be copied
    //
    //
    if (tail != progress) {
        return false;
    }

    //
    // Now, it's safe to copy requests
    //
    //
    RingSizeT availBytes = 0;
    char* sourceBuffer1 = nullptr;
    char* sourceBuffer2 = nullptr;

    if (progress > head) {
        availBytes = progress - head;
        *RequestSize = availBytes;
        sourceBuffer1 = &RingBuffer->Buffer[head];
    }
    else {
        availBytes = DDS_REQUEST_RING_BYTES - head;
        *RequestSize = availBytes + progress;
        sourceBuffer1 = &RingBuffer->Buffer[head];
        sourceBuffer2 = &RingBuffer->Buffer[0];
    }

    memcpy(CopyTo, sourceBuffer1, availBytes);
    memset(sourceBuffer1, 0, availBytes);

    if (sourceBuffer2) {
        memcpy((char*)CopyTo + availBytes, sourceBuffer2, progress);
        memset(sourceBuffer2, 0, progress);
    }

    RingBuffer->Head[0] = progress;

    return true;
}

//
// Parse a request from copied data
// Note: RequestSize is greater than the actual request size and is cache line aligned
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
) {
    char* bufferAddress = (char*)CopyTo;
    FileIOSizeT totalBytes = *(FileIOSizeT*)bufferAddress;

    *RequestPointer = (BufferT)(bufferAddress + sizeof(FileIOSizeT));
    *RequestSize = totalBytes - sizeof(FileIOSizeT);
    *RemainingSize = TotalSize - totalBytes;

    if (*RemainingSize > 0) {
        *StartOfNext = (BufferT)(bufferAddress + totalBytes);
    }
    else {
        *StartOfNext = nullptr;
    }
}

}