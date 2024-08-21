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

void OffloadPred(
    void* Msg,
    uint16_t Bytes,
    CacheTableT* CacheTable,
    RequestDescriptorT* ReqsForHost,
    RequestDescriptorT* ReqsForDPU,
    uint16_t* NumReqsForHost,
    uint16_t* NumReqsForDPU
) {
    const uint16_t reqSize = sizeof(struct MessageHeader);

    if (Bytes < sizeof(struct MessageHeader)) {
        *NumReqsForHost = 1;
        *NumReqsForDPU = 0;
        ReqsForHost[0].Offset = 0;
        ReqsForHost[0].Bytes = Bytes;
        return;
    }

    const int numReqsTotal = Bytes / reqSize;
    int numReqsForHost = 0, numReqsForDPU = 0;

    for (int i = 0; i != numReqsTotal; i++) {
        struct MessageHeader* msg = (struct MessageHeader*)(((char*)Msg) + i * reqSize);
        CacheItemT* cacheItem = LookUpCacheTable(CacheTable, &msg->Key);
        if (cacheItem == NULL) {
            ReqsForHost[numReqsForHost].Offset = i * reqSize;
            ReqsForHost[numReqsForHost].Bytes = reqSize;
            numReqsForHost++;
        }
        else {
            ReqsForDPU[numReqsForDPU].Offset = i * reqSize;
            ReqsForDPU[numReqsForDPU].Bytes = reqSize;
            numReqsForDPU++;
        }
    }

    *NumReqsForHost = numReqsForHost;
    *NumReqsForDPU = numReqsForDPU;
}
