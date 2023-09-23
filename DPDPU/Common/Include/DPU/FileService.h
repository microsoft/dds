#pragma once

#include "BackEndTypes.h"

//
// File service running on the DPU
//
//
typedef struct {
    //
    // TODO: necessary state of the SPDK file service
    //
    //
    int Dummy;
} FileService;

//
// Allocate the file service object
//
//
FileService*
AllocateFileService();

//
// Start the file service 
//
//
void
StartFileService(
    FileService* FS
);

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
    DataPlaneRequestContext* Context
);