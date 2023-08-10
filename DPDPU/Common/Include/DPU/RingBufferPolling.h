#pragma once

#include <stdint.h>

#include "Protocol.h"

//
// The back end of the request ring buffer for polling
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
    uint64_t DataBaseAddr;
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

//
// Initialize a buffer that is not cache aligned
//
//
void
InitializeRequestRingBufferNotAlignedBackEnd(
    struct RequestRingBufferBackEnd* RingBuffer,
    uint64_t RemoteAddr,
    uint32_t AccessToken,
    uint32_t Capacity
);

//
// Initialize a buffer for FaRM-style ring buffer
//
//
void
InitializeRequestRingBufferFaRMStyleBackEnd(
    struct RequestRingBufferBackEnd* RingBuffer,
    uint64_t RemoteAddr,
    uint32_t AccessToken,
    uint32_t Capacity
);

//
// Initialize a buffer for lock-based ring buffer
//
//
void
InitializeRequestRingBufferLockBasedBackEnd(
    struct RequestRingBufferBackEnd* RingBuffer,
    uint64_t RemoteAddr,
    uint32_t AccessToken,
    uint32_t Capacity
);

//
// The back end of the response ring buffer for polling
//
//
struct ResponseRingBufferBackEnd{
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
    uint64_t DataBaseAddr;
    int Tail;
};

//
// Initialize a buffer
//
//
void
InitializeResponseRingBufferBackEnd(
    struct ResponseRingBufferBackEnd* RingBuffer,
    uint64_t RemoteAddr,
    uint32_t AccessToken,
    uint32_t Capacity
);