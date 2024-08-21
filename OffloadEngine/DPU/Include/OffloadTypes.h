#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "BackEndTypes.h"

#define MAX_OFFLOAD_READ_SIZE 16384

typedef struct {
    uint16_t Offset;
    uint16_t Bytes;
} RequestDescriptorT;

typedef struct {
    OffloadWorkRequest ReadReq;
    OffloadWorkResponse ReadResp;
#if OFFLOAD_ENGINE_ZERO_COPY == OFFLOAD_ENGINE_ZERO_COPY_NONE
    char RespBuf[MAX_OFFLOAD_READ_SIZE];
#ifdef OFFLOAD_ENGINE_RESPONSE_INCLUDES_REQUEST
    uint16_t ReqSize;
#endif
#else
    void* RespBuf;
#endif
#ifdef OFFLOAD_ENGINE_ALLOW_NONALIGNED_IO
    FileSizeT OffsetAlignment;
    FileIOSizeT BytesAlignment;
#endif
    ContextT StreamCtxt;
} ReadOpDescriptorT;
