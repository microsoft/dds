#include "Types.h"

struct MessageHeader {
    bool OffloadedToDPU;
    bool LastMessage;
    int MessageSize;
    long long TimeSend;
};

bool
OffloadPred(
    void* Req,
    uint16_t Size,
    void* GMem
) {
    if (Size >= sizeof(struct MessageHeader)) {
        struct MessageHeader* hdr = (struct MessageHeader*)Req;
        return hdr->OffloadedToDPU;
    }
    else {
        return false;
    }
}
