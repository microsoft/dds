/*
 * Copyright (c) Microsoft Corporation.
 * Licensed under the MIT License
 */

#include <pthread.h>

#include "DDSBOWPipeline.h"
#include "FileBackEnd.h"

typedef struct {
    int Argc;
    char** Argv;
} StorageEngineArg;

void
StorageEngine(
    void* Arg
) {
    StorageEngineArg* arg = (StorageEngineArg*)Arg;
    fprintf(stdout, "Starting storage engine...\n");
    RunFileBackEnd(
        DDS_BACKEND_ADDR,
        DDS_BACKEND_PORT,
        1,
        1,
        arg->Argc,
        arg->Argv
    );
    fprintf(stdout, "Storage engine exited\n");
}

int
main(
    int Argc,
    char** Argv
) {
    //
    // Run storage engine on a separte thread
    //
    //
    StorageEngineArg sEngArg;
    char* argvStorage[3];
    char* argvNetwork[3];
    int ret;

    if (Argc != 5) {
        fprintf(stderr, "Usage: %s --json [storage config] --json [network config] \n", Argv[0]);
        return -1;
    }

    argvStorage[0] = Argv[0];
    argvStorage[1] = Argv[1];
    argvStorage[2] = Argv[2];
    sEngArg.Argc = 3;
    sEngArg.Argv = argvStorage;

    //
    // Run network engine
    //
    //
    argvNetwork[0] = Argv[0];
    argvNetwork[1] = Argv[3];
    argvNetwork[2] = Argv[4];
    fprintf(stdout, "Starting network engine...\n");
    ret = RunBOW(3, argvNetwork, StorageEngine, &sEngArg);
    fprintf(stdout, "Network engine exited\n");

    return ret;
}
