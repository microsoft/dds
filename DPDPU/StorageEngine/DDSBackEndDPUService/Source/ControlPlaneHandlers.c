#include <stdio.h>
#include <string.h>

#include "ControlPlaneHandler.h"

struct DPUStorage *Sto;
SPDKContextT *SPDKContext;

//
// Handler for a control plane request
//
//
void ControlPlaneHandler(
    ControlPlaneRequestContext *Context
) {
    //
    // TODO: send this request to the file service
    // Upon completion, set Context->Response->Result as DDS_ERROR_CODE_SUCCESS
    //
    //
    printf("Executing a control plane request: %u\n", Context->RequestId);
}