/*
 * Copyright (c) Microsoft Corporation.
 * Licensed under the MIT License
 */

#include "CacheTable.h"
#include "OffloadTypes.h"
#include <stdio.h>

struct MessageHeader {
	long long TimeSend;
	uint16_t BatchId;
	uint64_t Key;
	uint64_t Value;
	uint8_t Operation;
};

void OffloadFunc(
    void* Msg,
    RequestDescriptorT* Req,
    CacheTableT* CacheTable,
    OffloadWorkRequest* ReadOp
)  {
    struct MessageHeader* msg = (struct MessageHeader*)(((char*)Msg) + Req->Offset);
    CacheItemT* cacheItem = LookUpCacheTable(CacheTable, &msg->Key);
    ReadOp->FileId = cacheItem->FileId;
    ReadOp->Bytes = cacheItem->Size;
    ReadOp->Offset = cacheItem->Offset;
}
