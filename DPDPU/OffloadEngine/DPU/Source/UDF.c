#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>

#include "UDF.h"

//
// Compile either the offload predicate or the offload function
//
//
bool
CompileUDF(
    const char *CodePath,
    enum OffloadType Type
) {
    int err_code = 0;

    char cmd[512] = "gcc -O3 -Wall -Werror -fPIC -I../OffloadEngine/DPU/Include -shared ";

    if (Type == OFFLOAD_PRED) {
        strcat(cmd, CodePath);
        strcat(cmd, OFFLOAD_PRED_NAME);
        strcat(cmd, ".c -o ");
        strcat(cmd, CodePath);
        strcat(cmd, OFFLOAD_PRED_NAME);
        strcat(cmd, ".so");
    
        //
        // Disable output
        //
        //
        strcat(cmd, " >> ");
        strcat(cmd, CodePath);
        strcat(cmd, "BuildLog_");
        strcat(cmd, OFFLOAD_PRED_NAME);
        strcat(cmd, " 2>> ");
        strcat(cmd, CodePath);
        strcat(cmd, "BuildLog_");
        strcat(cmd, OFFLOAD_PRED_NAME);
    } else {
        strcat(cmd, CodePath);
        strcat(cmd, OFFLOAD_FUNC_NAME);
        strcat(cmd, ".c -o ");
        strcat(cmd, CodePath);
        strcat(cmd, OFFLOAD_FUNC_NAME);
        strcat(cmd, ".so");

        //
        // Disable output
        //
        //
        strcat(cmd, " >> ");
        strcat(cmd, CodePath);
        strcat(cmd, "BuildLog_");
        strcat(cmd, OFFLOAD_FUNC_NAME);
        strcat(cmd, " 2>> ");
        strcat(cmd, CodePath);
        strcat(cmd, "BuildLog_");
        strcat(cmd, OFFLOAD_FUNC_NAME);
    }

    err_code = system(cmd);
    if (err_code) {
        return false;
    }

    return true;
}

//
// Load the library that implements a UDF
//
//
void* LoadLibrary(
    const char *CodePath,
    enum OffloadType Type
) {
    char LibPath[64];
    memset(LibPath, 0, 64);
    strcat(LibPath, CodePath);
    if (Type == OFFLOAD_PRED) {
        strcat(LibPath, OFFLOAD_PRED_NAME);
        strcat(LibPath, ".so");
    } else {
        strcat(LibPath, OFFLOAD_FUNC_NAME);
        strcat(LibPath, ".so");
    }
    void *lib = dlopen(LibPath, RTLD_NOW);
    return lib;
}

//
// Get the offload predicate
//
//
OffloadPred GetOffloadPred(
    void *Lib
) {
    OffloadPred pred = (OffloadPred)dlsym(Lib, OFFLOAD_PRED_NAME);
    return pred;
}

//
// Get the offload function
//
//
OffloadFunc GetOffloadFunc(
    void *Lib
) {
    OffloadFunc func = (OffloadFunc)dlsym(Lib, OFFLOAD_FUNC_NAME);
    return func;
}

//
// Unload a library
//
//
void UnloadLibrary(
    void *Lib
) {
    dlclose(Lib);
}
