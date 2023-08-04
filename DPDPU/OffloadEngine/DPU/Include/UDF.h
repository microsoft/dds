#ifndef DDS_OFFLOAD_UDF_H
#define DDS_OFFLOAD_UDF_H

#include "Types.h"

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
typedef bool (*OffloadPred)(
    void* Req,
    uint16_t Size,
    void* GMem
);

//
// The type of offload function
// 
//
typedef void (*OffloadFunc)(
    void* Req,
    uint16_t Size,
    void* GMem,
    void* DDSFiles,
    void* RespPtr,
    int* RespSize
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

#endif /* DDS_OFFLOAD_UDF_H */
