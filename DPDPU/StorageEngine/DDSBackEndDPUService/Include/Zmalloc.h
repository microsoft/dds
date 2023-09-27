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
    void *Ctx;//put the needed ctx pointer here
    //add more if you need more info
};

void AllocateSpace(void *arg);

void FreeSingleSpace(void *arg);

void FreeAllSpace(void *arg);

int FindFreeSpace(void *arg);