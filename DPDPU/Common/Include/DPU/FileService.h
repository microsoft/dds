#pragma once

#include "BackEndTypes.h"
#include "Zmalloc.h"
#include "DataPlaneHandlers.h"

#define WORKER_THREAD_COUNT 1


//
// File service running on the DPU
//
//
typedef struct {
    //
    // Spawned worker threads, each will have its corresponding SPDKContext,
    // this should be initialized at startup and remain unchanged
    //
    //
    struct spdk_thread **WorkerThreads;  // array of thread ptrs
    SPDKContextT *WorkerSPDKContexts;  // array of ctxs per thread
    int WorkerThreadCount;

    struct spdk_thread *AppThread;
    SPDKContextT *MasterSPDKContext;
} FileService;

extern FileService* FS;


//
// Allocate the file service object
//
//
FileService*
AllocateFileService();


//
// the context/params to start the file service
//
//
struct StartFileServiceCtx {
    int Argc;
    char **Argv;
    FileService *FS;
};

//
// Start the file service 
//
//
void
StartFileService(
    int Argc,
    char **Argv,
    FileService *FS,
    pthread_t *AppPthread
);

struct WorkerThreadExitCtx {
    struct spdk_io_channel *Channel;
    int i;
};

//
// Stop the file service
//
//
void
StopFileService(
    FileService* FS
);

//
// Deallocate the file service object
//
//
void
DeallocateFileService(
    FileService* FS
);

//
// Send a control plane request to the file service 
//
//
void
SubmitControlPlaneRequest(
    FileService* FS,
    ControlPlaneRequestContext* Context
);

//
// Send a data plane request to the file service 
//
//
void
SubmitDataPlaneRequest(
    FileService* FS,
    DataPlaneRequestContext* Context,
    bool IsRead,
    RequestIdT Index
);


//
// Usage function for printing parameters that are specific to this application, needed for SPDK app start
//
//
static void
DDSCustomArgsUsage(void);

//
// This function is called to parse the parameters that are specific to this application
//
//
static int
DDSParseArg(int ch, char *arg);