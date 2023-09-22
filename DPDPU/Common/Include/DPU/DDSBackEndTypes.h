#pragma once

#include "DDSTypes.h"
#include "MsgTypes.h"

//
// Context for a pending data plane request
//
//
typedef struct {
    BuffMsgF2BReqHeader* Request;
    BuffMsgB2FAckHeader* Response;
    SplittableBufferT* DataBuffer;
} DataPlaneRequestContext;

//
// Context for a pending control plane request
//
//
typedef struct {
    RequestIdT RequestId;
    BufferT Request;
    BufferT Response;
} ControlPlaneRequestContext;

typedef struct {
    RequestIdT RequestId;
    ErrorCodeT Result;
    FileIOSizeT BytesServiced;
} B2BAckHeader;

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
// Ring space is allocated at the |size of a request/response| + |header size|
// The alignment is enforced once a request/response is inserted into the ring
//
//
AssertStaticBackEndTypes(DDS_INTRA_BACKEND_REQUEST_RING_BYTES % sizeof(DataPlaneRequestContext) == 0, 0);
AssertStaticBackEndTypes(DDS_RESPONSE_RING_BYTES % (sizeof(B2BAckHeader) + sizeof(FileIOSizeT)) == 0, 1);
#ifdef __GNUC__
#pragma GCC diagnostic pop
#else
#pragma warning(pop)
#endif