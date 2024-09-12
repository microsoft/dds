/*
 * Copyright (c) Microsoft Corporation.
 * Licensed under the MIT License
 */

#pragma once

#include "MsgTypes.h"
#include "bdev.h"
#include "DPUBackEndStorage.h"
#include <stdbool.h>
#include "Zmalloc.h"

//
// To support batching request submission, instead of directly spdk send msg with R/W Handler, send msg with this,
// which will then call the corresponding RW Handler
//
//
void DataPlaneRequestHandler(
    void* Ctx
);

//
// Handler for a read request
//
//
void ReadHandler(
    void* Ctx
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
    void* Ctx
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
