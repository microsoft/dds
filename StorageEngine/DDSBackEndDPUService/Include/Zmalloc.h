/*
 * Copyright (c) Microsoft Corporation.
 * Licensed under the MIT License
 */

#include "DPUBackEnd.h"
#include "DPUBackEndStorage.h"

void AllocateSpace(void *arg);

void FreeSingleSpace(struct PerSlotContext* Ctx);

void FreeAllSpace(void *arg);

struct PerSlotContext*
FindFreeSpace(
    SPDKContextT *SPDKContext,
    DataPlaneRequestContext* Context
);

struct PerSlotContext*
GetFreeSpace(
    SPDKContextT *SPDKContext,
    DataPlaneRequestContext* Context,
    RequestIdT Index
);