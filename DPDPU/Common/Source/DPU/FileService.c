#include <stdlib.h>

#include "FileService.h"

#undef DEBUG_FILE_SERVICE
#ifdef DEBUG_FILE_SERVICE
#include <stdio.h>
#define DebugPrint(Fmt, ...) fprintf(stderr, Fmt, __VA_ARGS__)
#else
static inline void DebugPrint(const char* Fmt, ...) { }
#endif

//
// Allocate the file service object
//
//
FileService*
AllocateFileService() {
    DebugPrint("Allocating the file service object...\n");
    return (FileService*)malloc(sizeof(FileService));
}

//
// Start the file service 
//
//
void
StartFileService(
    FileService* FS
) {
    DebugPrint("File service started\n");
}

//
// Stop the file service
//
//
void
StopFileService(
    FileService* FS
) {
    DebugPrint("File service stopped\n");
}

//
// Deallocate the file service object
//
//
void
DeallocateFileService(
    FileService* FS
) {
    free(FS);
    DebugPrint("File service object deallocated\n");
}

//
// Send a control plane request to the file service 
//
//
void
SubmitControlPlaneRequest(
    FileService* FS,
    ControlPlaneRequestContext* Context
) {
    DebugPrint("Submitting a control plane request\n");
}

//
// Send a data plane request to the file service 
//
//
void
SubmitDataPlaneRequest(
    FileService* FS,
    DataPlaneRequestContext* Context
) {
    DebugPrint("Submitting a data plane request\n");
}