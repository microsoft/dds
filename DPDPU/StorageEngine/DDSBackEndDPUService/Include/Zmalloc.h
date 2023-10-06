#include "DPUBackEnd.h"
#include "bdev.h"
#include "DPUBackEndStorage.h"  // circular include within FileService.h

//
// Used to manage the status of each slot in SPDK buffer
//
//
typedef struct PerSlotContext{
    int Position;
    bool Available;  // should be unused now
    SPDKContextT *SPDKContext;  // thread specific SPDKContext
    DataPlaneRequestContext *Ctx;
    atomic_ushort CallbacksToRun;
    atomic_ushort CallbacksRan;
    FileIOSizeT BytesIssued;
    // bool IsRead;  // unused??
};

void AllocateSpace(void *arg);

void FreeSingleSpace(struct PerSlotContext* Ctx);

void FreeAllSpace(void *arg);

struct PerSlotContext* FindFreeSpace(
    SPDKContextT *SPDKContext,
    DataPlaneRequestContext* Context
);


struct PerSlotContext* GetFreeSpace(
    SPDKContextT *SPDKContext,
    DataPlaneRequestContext* Context,
    RequestIdT index
);