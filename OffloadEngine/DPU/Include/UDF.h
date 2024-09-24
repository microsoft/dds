/*
 * Copyright (c) Microsoft Corporation.
 * Licensed under the MIT License
 */

#pragma once

#include "CacheTable.h"
#include "OffloadTypes.h"

#define OFFLOAD_PRED_NAME "OffloadPred"
#define OFFLOAD_FUNC_NAME "OffloadFunc"

enum OffloadType {
    OFFLOAD_PRED,
    OFFLOAD_FUNC
};

//
// The type of offload predicate
// 
//
typedef void (*OffloadPred)(
    void* Msg,
    uint16_t Bytes,
    CacheTableT* CacheTable,
    RequestDescriptorT* ReqsForHost,
    RequestDescriptorT* ReqsForDPU,
    uint16_t* NumReqsForHost,
    uint16_t* NumReqsForDPU
);

//
// The type of offload function
// 
//
typedef void (*OffloadFunc)(
    void* Msg,
    RequestDescriptorT* Req,
    CacheTableT* CacheTable,
    OffloadWorkRequest* ReadOp
);

//
// Compile either the offload predicate or the offload function
//
//
bool
CompileUDF(
    const char *CodePath,
    enum OffloadType Type
);

//
// Load the library that implements a UDF
//
//
void*
LoadLibrary(
    const char *CodePath,
    enum OffloadType Type
);

//
// Get the offload predicate
//
//
OffloadPred
GetOffloadPred(
    void *Lib
);

//
// Get the offload function
//
//
OffloadFunc
GetOffloadFunc(
    void *Lib
);

//
// Unload a library
//
//
void
UnloadLibrary(
    void *Lib
);
