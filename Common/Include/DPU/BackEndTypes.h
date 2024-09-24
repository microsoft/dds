/*
 * Copyright (c) Microsoft Corporation.
 * Licensed under the MIT License
 */

#pragma once

#include "DDSTypes.h"
#include "MsgTypes.h"

//
// Context for a pending data plane request from the host
//
//
typedef struct {
    int IsRead;
    BuffMsgF2BReqHeader* Request;
    BuffMsgB2FAckHeader* Response;
    SplittableBufferT DataBuffer;
} DataPlaneRequestContext;

//
// Context for a pending control plane request
//
//
typedef struct {
    RequestIdT RequestId;
    BufferT Request;
    BufferT Response;
    void* SPDKContext;  // thread specific SPDK ctx
} ControlPlaneRequestContext;

//
// Check a few parameters at the compile time
//
//
#define AssertStaticBackEndTypes(E, Num) \
    enum { AssertStaticBackEndTypes__##Num = 1/(E) }

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result" 
#else
#pragma warning(push)
#pragma warning (disable: 4804)
#endif
//
// Ring space is allocated at the |size of a response| + |header size|
// The alignment is enforced once a response is inserted into the ring
//
//
AssertStaticBackEndTypes(OFFLOAD_RESPONSE_RING_BYTES % (sizeof(OffloadWorkResponse) + sizeof(FileIOSizeT)) == 0, 0);
#ifdef __GNUC__
#pragma GCC diagnostic pop
#else
#pragma warning(pop)
#endif