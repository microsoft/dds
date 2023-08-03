#pragma once

#include "RDMC.h"
#include "MsgType.h"

class DMABuffer {
private:
    size_t Capacity;
    int ClientId;
    int BufferId;

    //
    // RNIC configuration
    //
    //
    IND2Adapter* Adapter;
    HANDLE AdapterFileHandle;
    IND2Connector* Connector;
    OVERLAPPED Ov;
    IND2CompletionQueue* CompQ;
    IND2QueuePair* QPair;
    IND2MemoryRegion* MemRegion;
    IND2MemoryWindow* MemWindow;

    //
    // Variables for messages
    //
    //
    ND2_SGE* MsgSgl;
    char MsgBuf[BUFF_MSG_SIZE];
    IND2MemoryRegion* MsgMemRegion;

public:
    char* BufferAddress;

public:
    DMABuffer(
        const char* _BackEndAddr,
        const unsigned short _BackEndPort,
        const size_t _Capacity,
        const int _ClientId
    );

    //
    // Allocate the buffer with the specified capacity
    // and register it to the NIC;
    // Not thread-safe
    //
    //
    bool Allocate(
        struct sockaddr_in* LocalSock,
        struct sockaddr_in* BackEndSock,
        const size_t QueueDepth,
        const size_t MaxSge,
        const size_t InlineThreshold
    );

    //
    // Release the allocated buffer;
    // Not thread-safe
    //
    //
    void Release();
};