#include "DPUBackEnd.h"
#include "bdev.h"

void AllocateSpace(void *arg);

void AllocateSingleSpace(
    void *arg, 
    int Position
);

void FreeSingleSpace(
    void *arg, 
    int Position
);

void FreeAllSpace(void *arg);

int FindFreeSpace(void *arg);