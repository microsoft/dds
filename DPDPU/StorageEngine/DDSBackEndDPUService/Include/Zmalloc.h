#include "DPUBackEnd.h"
#include "bdev.h"
#include "DPUBackEndStorage.h"

void AllocateSpace(void *arg);

void FreeSingleSpace(
    void *arg, 
    int Position
);

void FreeAllSpace(void *arg);

int FindFreeSpace(void *arg);