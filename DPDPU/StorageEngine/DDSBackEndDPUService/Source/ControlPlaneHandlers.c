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
    switch (Context->RequestId) {
        case CTRL_MSG_F2B_REQ_CREATE_DIR: {
            CtrlMsgF2BReqCreateDirectory *Req = Context->Request;

            // we should free handler ctx in the last callback for every case
            ControlPlaneHandlerCtx *HandlerCtx = malloc(sizeof(*HandlerCtx));
            CreateDirectory(Req->PathName, Req->DirId, Req->ParentDirId, Sto, SPDKContext, HandlerCtx);
        }
            break;
        case CTRL_MSG_F2B_REQ_REMOVE_DIR: {

        }
            break;
        case CTRL_MSG_F2B_REQ_CREATE_FILE: {

        }
            break;
        case CTRL_MSG_F2B_REQ_DELETE_FILE: {

        }
            break;
        case CTRL_MSG_F2B_REQ_CHANGE_FILE_SIZE: {

        }
            break;
        case CTRL_MSG_F2B_REQ_GET_FILE_SIZE: {

        }
            break;
        case CTRL_MSG_F2B_REQ_GET_FILE_INFO: {

        }
            break;
        case CTRL_MSG_F2B_REQ_GET_FILE_ATTR: {

        }
            break;
        case CTRL_MSG_F2B_REQ_GET_FREE_SPACE: {

        }
            break;
        case CTRL_MSG_F2B_REQ_MOVE_FILE: {

        }
            break;
        default: {
            printf("UNKNOWN CONTROL PLANE REQUEST: %d\n", Context->RequestId);
        }
    }
}