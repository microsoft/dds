#pragma once

#include <atomic>

#include "DDSFrontEndConfig.h"
#include "DDSTypes.h"

#if BACKEND_TYPE == BACKEND_TYPE_DPU
#include "DMABuffer.h"
#include "RingBufferProgressive.h"
#endif

namespace DDS_FrontEnd {

template <typename T>
using Atomic = std::atomic<T>;

//
// File read/write operation
//
//
typedef struct FileIOT {
    bool IsRead = false;
    bool IsSegmented = false;
    ContextT FileReference = (ContextT)nullptr;
    FileIdT FileId = (FileIdT)DDS_FILE_INVALID;
    FileSizeT Offset = (FileSizeT)0;
    BufferT AppBuffer = (BufferT)nullptr;
    BufferT* AppBufferArray = (BufferT*)nullptr;
    FileIOSizeT BytesDesired = (FileIOSizeT)0;
    FileIOSizeT BytesServiced = (FileIOSizeT)0;
    ReadWriteCallback AppCallback = (ReadWriteCallback)nullptr;
    ContextT Context = (ContextT)nullptr;
    Atomic<bool> IsComplete = true;
    Atomic<bool> IsReadyForPoll = false;
} FileIOT;

//
// Poll structure
//
//
typedef struct PollT {
    FileIOT* OutstandingRequests[DDS_FRONTEND_MAX_OUTSTANDING];
    Atomic<size_t> NextRequestSlot;
#if BACKEND_TYPE == BACKEND_TYPE_DPU
    DMABuffer* MsgBuffer;
    struct RequestRingBufferProgressive* RequestRing;
    struct ResponseRingBufferProgressive* ResponseRing;
#endif

    PollT();

#if BACKEND_TYPE == BACKEND_TYPE_DPU
    ErrorCodeT SetUpDMABuffer(void* BackEndDPU);
    void DestroyDMABuffer();
    void InitializeRings();
#endif
} PollT;

}