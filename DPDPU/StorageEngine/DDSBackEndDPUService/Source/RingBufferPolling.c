#include "RingBufferPolling.h"

//
// Initialize a buffer
//
//
void
InitializeRequestRingBufferBackEnd(
    struct RequestRingBufferBackEnd* RingBuffer,
    uint64_t RemoteAddr,
    uint32_t AccessToken,
    uint32_t Capacity
) {
    memset(RingBuffer, 0, sizeof(struct RequestRingBufferBackEnd));
    RingBuffer->RemoteAddr = RemoteAddr;
    RingBuffer->AccessToken = AccessToken;
    RingBuffer->Capacity = Capacity;
    RingBuffer->Head = 0;

    //
    // Align the buffer by cache line size
    //
    //
    uint64_t ringBufferAddress = (uint64_t)RingBuffer->RemoteAddr;
    while (ringBufferAddress % DDS_CACHE_LINE_SIZE != 0) {
        ringBufferAddress++;
    }

    //
    // Read the tail and the progress in the same request to minimize latency/overhead
    //
    //
    RingBuffer->ReadMetaAddr = ringBufferAddress;
    RingBuffer->ReadMetaSize = DDS_CACHE_LINE_SIZE * 2;

    //
    // Only write the head int
    //
    //
    RingBuffer->WriteMetaAddr = ringBufferAddress + RingBuffer->ReadMetaSize;
    RingBuffer->WriteMetaSize = sizeof(int);

    //
    // The data is after the meta data
    //
    //
    RingBuffer->ReadDataBaseAddr = ringBufferAddress + DDS_CACHE_LINE_SIZE * 3;
}