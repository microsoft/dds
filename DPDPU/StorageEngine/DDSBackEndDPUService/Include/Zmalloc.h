#include "DPUBackEnd.h"
#include "bdev.h"
#include "DPUBackEndStorage.h"

//
// Used to manage the status of each slot in SPDK buffer
//
//
typedef struct PerSlotContext{
    int Position;
    bool Available;
    DataPlaneRequestContext *Ctx;
    atomic_ushort CallbacksToRun;
    atomic_ushort CallbacksRan;
    FileIOSizeT BytesIssued;
    bool IsRead;
};

void AllocateSpace(void *arg);

void FreeSingleSpace(struct PerSlotContext* Ctx);

void FreeAllSpace(void *arg);

struct PerSlotContext* FindFreeSpace(
    SPDKContextT *SPDKContext,
    DataPlaneRequestContext* Context
);