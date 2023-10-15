#pragma once

#include "MsgTypes.h"
#include "bdev.h"
#include "DPUBackEndStorage.h"
#include <stdbool.h>
#include "Zmalloc.h"

//
// Handler for a read request
//
//
void ReadHandler(
    struct PerSlotContext *SlotContext
);

//
// Callback for ReadHandler async read
//
//
void ReadHandlerZCCallback(
    struct spdk_bdev_io *bdev_io,
    bool Success,
    ContextT Context
);

void ReadHandlerNonZCCallback(
    struct spdk_bdev_io *bdev_io,
    bool Success,
    ContextT Context
);

//
// Handler for a write request
//
//
void WriteHandler(
    struct PerSlotContext *SlotContext
);

//
// Callback for WriteHandler async write
//
//
void WriteHandlerCallback(
    struct spdk_bdev_io *bdev_io,
    bool Success,
    ContextT Context
);
