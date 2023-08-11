#pragma once

#include <iostream>

#include "RingBufferProgressive.h"

namespace DDS_FrontEnd {

//
// **************************************************************
// Request buffer implementation
// **************************************************************
//

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

    //
    // Append request size to the beginning of the request
    // Check alignment
    //
    //
    FileIOSizeT requestBytes = sizeof(FileIOSizeT) + RequestSize;

    if (requestBytes % sizeof(FileIOSizeT) != 0) {
        requestBytes += (sizeof(FileIOSizeT) - (requestBytes % sizeof(FileIOSizeT)));
    }

    if (distance + requestBytes >= RING_BUFFER_REQUEST_MAXIMUM_TAIL_ADVANCEMENT) {
        return false;
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

        if (distance + requestBytes >= RING_BUFFER_REQUEST_MAXIMUM_TAIL_ADVANCEMENT) {
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
    if (tail + sizeof(FileIOSizeT) + RequestSize <= DDS_REQUEST_RING_BYTES) {
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
        RingSizeT remainingBytes = DDS_REQUEST_RING_BYTES - tail - sizeof(FileIOSizeT);
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
        if (remainingBytes > 0) {
            memcpy(requestAddress1 + sizeof(FileIOSizeT), CopyFrom, remainingBytes);
        }
        memcpy(requestAddress2, (const char*)CopyFrom + remainingBytes, RequestSize - remainingBytes);

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
    // In order to make this ring buffer safe, we must maintain the invariant below:
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

//
// Wait for completion
//
//
bool
CheckForRequestCompletionProgressive(
    RequestRingBufferProgressive* RingBuffer
) {
    int head = RingBuffer->Head[0];
    int tail = RingBuffer->Tail[0];

    return head == tail;
}

//
// **************************************************************
// Response buffer implementation
// **************************************************************
//

//
// Allocate a response buffer object
//
//
ResponseRingBufferProgressive*
AllocateResponseBufferProgressive(
    BufferT BufferAddress
) {
    ResponseRingBufferProgressive* ringBuffer = (ResponseRingBufferProgressive*)BufferAddress;

    //
    // Align the buffer by cache line size
    //
    //
    size_t ringBufferAddress = (size_t)ringBuffer;
    while (ringBufferAddress % DDS_CACHE_LINE_SIZE != 0) {
        ringBufferAddress++;
    }
    ringBuffer = (ResponseRingBufferProgressive*)ringBufferAddress;

    return ringBuffer;
}

//
// Deallocate a response buffer object
//
//
void
DeallocateResponseBufferProgressive(
    ResponseRingBufferProgressive* RingBuffer
) {
    memset(RingBuffer, 0, sizeof(ResponseRingBufferProgressive));
}

//
// Fetch a response into the request buffer
//
//
bool
FetchFromResponseBufferProgressive(
    ResponseRingBufferProgressive* RingBuffer,
    const BufferT CopyTo,
    FileIOSizeT* ResponseSize
) {
    //
    // Check if there is a response at the head
    //
    //
    int tail = RingBuffer->Tail[0];
    int head = RingBuffer->Head[0];

    if (tail == head) {
        return false;
    }

    FileIOSizeT responseSize = *(FileIOSizeT*)&RingBuffer->Buffer[head];

    if (responseSize == 0) {
        return false;
    }

    //
    // Grab the current head
    //
    //
    while(RingBuffer->Head[0].compare_exchange_weak(head, (head + responseSize) % DDS_RESPONSE_RING_BYTES) == false) {
        tail = RingBuffer->Tail[0];
        head = RingBuffer->Head[0];
        responseSize = *(FileIOSizeT*)&RingBuffer->Buffer[head];

        if (tail == head) {
            return false;
        }

        if (responseSize == 0) {
            return false;
        }
    }

    //
    // Now, it's safe to copy the response
    //
    //
    int rTail = (head + responseSize) % DDS_RESPONSE_RING_BYTES;
    RingSizeT availBytes = 0;
    char* sourceBuffer1 = nullptr;
    char* sourceBuffer2 = nullptr;

    if (rTail > head) {
        availBytes = responseSize;
        *ResponseSize = availBytes;
        sourceBuffer1 = &RingBuffer->Buffer[head];
    }
    else {
        availBytes = DDS_RESPONSE_RING_BYTES - head;
        *ResponseSize = availBytes + rTail;
        sourceBuffer1 = &RingBuffer->Buffer[head];
        sourceBuffer2 = &RingBuffer->Buffer[0];
    }

    memcpy(CopyTo, sourceBuffer1, availBytes);
    memset(sourceBuffer1, 0, availBytes);

    if (sourceBuffer2) {
        memcpy((char*)CopyTo + availBytes, sourceBuffer2, rTail);
        memset(sourceBuffer2, 0, rTail);
    }

    //
    // Increment the progress
    //
    //
    int progress = RingBuffer->Progress[0];
    while (RingBuffer->Progress[0].compare_exchange_weak(progress, (progress + responseSize) % DDS_RESPONSE_RING_BYTES) == false) {
        progress = RingBuffer->Progress[0];
    }

    head = RingBuffer->Head[0];
    progress = RingBuffer->Progress[0];

    return true;
}

//
// Insert responses into the request buffer
//
//
bool
InsertToResponseBufferProgressive(
    ResponseRingBufferProgressive* RingBuffer,
    const BufferT* CopyFromList,
    FileIOSizeT* ResponseSizeList,
    int NumResponses,
    int* NumResponsesInserted
) {
    //
    // In order to make this ring buffer safe, we must maintain the invariant below:
    // Each consumer moves the head before it increments the progress.
    // Every consumser maintains this invariant:
    // They (1) advance the head,
    //      (2) read the response, and
    //      (3) increment the progress.
    // However, the order of reading progress and head at the producer matters.
    // If the producer reads the head first, then it's possible that
    // before it reads the progress, a concurrent consumer performs all three steps above
    // and thus the progress is updated.
    //
    //
    int progress = RingBuffer->Progress[0];
    int head = RingBuffer->Head[0];
    int tail = RingBuffer->Tail[0];

    //
    // Check if responses are safe to be inserted
    //
    //
    if (head != progress) {
        return false;
    }

    RingSizeT distance = 0;
    
    if (tail >= head) {
        distance = head + DDS_RESPONSE_RING_BYTES - tail;
    }
    else {
        distance = head - tail;
    }

    //
    // Not enough space for batched responses
    //
    //
    if (distance < RING_BUFFER_RESPONSE_MINIMUM_TAIL_ADVANCEMENT) {
        return false;
    }

    //
    // Now, it's safe to insert responses
    //
    //
    int respIndex = 0;
    FileIOSizeT responseBytes = 0;
    FileIOSizeT totalResponseBytes = 0;
    FileIOSizeT nextBytes = 0;
#ifdef RING_BUFFER_RESPONSE_BATCH_ENABLED
    int oldTail = tail;
    totalResponseBytes = sizeof(FileIOSizeT) + sizeof(int);
    for (; respIndex != NumResponses; respIndex++) {
        responseBytes = ResponseSizeList[respIndex];
        
        nextBytes = totalResponseBytes + responseBytes;

        //
        // Align to the size of FileIOSizeT
        //
        //
        if (nextBytes % sizeof(FileIOSizeT) != 0) {
            nextBytes += (sizeof(FileIOSizeT) - (nextBytes % sizeof(FileIOSizeT)));
        }
        
        if (nextBytes > distance || nextBytes > RING_BUFFER_RESPONSE_MAXIMUM_TAIL_ADVANCEMENT) {
            //
            // No more space or reaching maximum batch size
            //
            //
            break;
        }

        if (tail + ResponseSizeList[respIndex] <= DDS_RESPONSE_RING_BYTES) {
            //
            // Write one response
            // On DPU, these responses should batched and there should be no extra memory copy
            //
            //
            memcpy(&RingBuffer->Buffer[tail], CopyFromList[respIndex], ResponseSizeList[respIndex]);
        }
        else {
            FileIOSizeT firstPartBytes = DDS_RESPONSE_RING_BYTES - tail;
            FileIOSizeT secondPartBytes = ResponseSizeList[respIndex] - firstPartBytes;

            //
            // Write one response to two locations
            // On DPU, these responses should batched and there should be no extra memory copy
            //
            //
            memcpy(&RingBuffer->Buffer[tail], CopyFromList[respIndex], firstPartBytes);
            memcpy(&RingBuffer->Buffer[0], (char*)CopyFromList[respIndex] + firstPartBytes, secondPartBytes);
        }
        
        totalResponseBytes += responseBytes;
        tail = (tail + responseBytes) % DDS_RESPONSE_RING_BYTES;
    }

    if (totalResponseBytes % sizeof(FileIOSizeT) != 0) {
        totalResponseBytes += (sizeof(FileIOSizeT) - (totalResponseBytes % sizeof(FileIOSizeT)));
    }

    *(FileIOSizeT*)&RingBuffer->Buffer[oldTail] = totalResponseBytes;
    *(int*)&RingBuffer->Buffer[oldTail + sizeof(FileIOSizeT)] = respIndex;
#else
    for (; respIndex != NumResponses; respIndex++) {
        responseBytes = ResponseSizeList[respIndex] + sizeof(FileIOSizeT);
        
        //
        // Align to the size of FileIOSizeT
        //
        //
        if (responseBytes % sizeof(FileIOSizeT) != 0) {
            responseBytes += (sizeof(FileIOSizeT) - (responseBytes % sizeof(FileIOSizeT)));
        }
        
        nextBytes = totalResponseBytes + responseBytes;
        if (nextBytes > distance || nextBytes > RING_BUFFER_RESPONSE_MAXIMUM_TAIL_ADVANCEMENT) {
            //
            // No more space or reaching maximum batch size
            //
            //
            break;
        }

        if (tail + sizeof(FileIOSizeT) + ResponseSizeList[respIndex] <= DDS_RESPONSE_RING_BYTES) {
            //
            // Write one response
            // On DPU, these responses should batched and there should be no extra memory copy
            //
            //
            memcpy(&RingBuffer->Buffer[(tail + sizeof(FileIOSizeT)) % DDS_RESPONSE_RING_BYTES], CopyFromList[respIndex], ResponseSizeList[respIndex]);
        }
        else {
            FileIOSizeT firstPartBytes = DDS_RESPONSE_RING_BYTES - tail - sizeof(FileIOSizeT);
            FileIOSizeT secondPartBytes = ResponseSizeList[respIndex] - firstPartBytes;

            //
            // Write one response to two locations
            // On DPU, these responses should batched and there should be no extra memory copy
            //
            //
            if (firstPartBytes > 0) {
                memcpy(&RingBuffer->Buffer[tail + sizeof(FileIOSizeT)], CopyFromList[respIndex], firstPartBytes);
            }
            memcpy(&RingBuffer->Buffer[0], (char*)CopyFromList[respIndex] + firstPartBytes, secondPartBytes);
        }
        
        *(FileIOSizeT*)&RingBuffer->Buffer[tail] = responseBytes;
        totalResponseBytes += responseBytes;
        tail = (tail + responseBytes) % DDS_RESPONSE_RING_BYTES;
    }
#endif

    *NumResponsesInserted = respIndex;
    RingBuffer->Tail[0] = tail;

    return true;
}

//
// Parse a response from copied data
// Note: ResponseSize is greater than the actual response size because of alignment
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
) {
    char* bufferAddress = (char*)CopyTo;
    FileIOSizeT totalBytes = *(FileIOSizeT*)bufferAddress;

    *ResponsePointer = (BufferT)(bufferAddress + sizeof(FileIOSizeT));
    *ResponseSize = totalBytes - sizeof(FileIOSizeT);
    *RemainingSize = TotalSize - totalBytes;

    if (*RemainingSize > 0) {
        *StartOfNext = (BufferT)(bufferAddress + totalBytes);
    }
    else {
        *StartOfNext = nullptr;
    }
}

//
// Wait for completion
//
//
bool
CheckForResponseCompletionProgressive(
    ResponseRingBufferProgressive* RingBuffer
) {
    int progress = RingBuffer->Progress[0];
    int head = RingBuffer->Head[0];
    int tail = RingBuffer->Tail[0];

    return progress == head && head == tail;
}

}