#pragma once

#include <stdint.h>

#include "Protocol.h"

//
// The back end of the ring buffer for polling
//
//
struct RequestRingBufferBackEnd{
    uint64_t RemoteAddr;
    uint32_t AccessToken;
    uint32_t Capacity;

    //
    // Cached data for high performance
    //
    //
    uint64_t ReadMetaAddr;
    uint32_t ReadMetaSize;
    uint64_t WriteMetaAddr;
    uint32_t WriteMetaSize;
    uint64_t ReadDataBaseAddr;
    int Head;
};

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
);