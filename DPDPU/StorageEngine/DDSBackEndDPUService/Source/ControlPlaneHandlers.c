#include <stdio.h>
#include <string.h>

#include "ControlPlaneHandler.h"

// struct DPUStorage *Sto;
// FileService* FS;

//
// Handler for a control plane request
//
//
void ControlPlaneHandler(
    void *ReqContext
) {
    //
    // Upon completion, set Context->Response->Result as DDS_ERROR_CODE_SUCCESS
    //
    //
    ControlPlaneRequestContext *Context = (ControlPlaneRequestContext *) ReqContext;
    // SPDK_NOTICELOG("Executing a control plane request, id: %d, req: %p, resp: %p\n",
    //     Context->RequestId, Context->Request, Context->Response);

    switch (Context->RequestId) {
        case CTRL_MSG_F2B_REQ_CREATE_DIR: {
            CtrlMsgF2BReqCreateDirectory *Req = Context->Request;
            CtrlMsgB2FAckCreateDirectory *Resp = Context->Response;

            // we should free handler ctx in the last callback for every case
            ControlPlaneHandlerCtx *HandlerCtx = malloc(sizeof(*HandlerCtx));
            HandlerCtx->Result = &Resp->Result;
            CreateDirectory(Req->PathName, Req->DirId, Req->ParentDirId, Sto, Context->SPDKContext, HandlerCtx);
        }
            break;
        case CTRL_MSG_F2B_REQ_REMOVE_DIR: {
            CtrlMsgF2BReqRemoveDirectory *Req = Context->Request;
            CtrlMsgB2FAckRemoveDirectory *Resp = Context->Response;
            ControlPlaneHandlerCtx *HandlerCtx = malloc(sizeof(*HandlerCtx));
            HandlerCtx->Result = &Resp->Result;
            RemoveDirectory(Req->DirId, Sto, Context->SPDKContext, HandlerCtx);

        }
            break;
        case CTRL_MSG_F2B_REQ_CREATE_FILE: {
            CtrlMsgF2BReqCreateFile *Req = Context->Request;
            CtrlMsgB2FAckCreateFile *Resp = Context->Response;
            ControlPlaneHandlerCtx *HandlerCtx = malloc(sizeof(*HandlerCtx));
            HandlerCtx->Result = &Resp->Result;
            CreateFile(Req->FileName, Req->FileAttributes, Req->FileId, 
            Req->DirId, Sto, Context->SPDKContext, HandlerCtx);
        }
            break;
        case CTRL_MSG_F2B_REQ_DELETE_FILE: {
            CtrlMsgF2BReqDeleteFile *Req = Context->Request;
            CtrlMsgB2FAckDeleteFile *Resp = Context->Response;
            ControlPlaneHandlerCtx *HandlerCtx = malloc(sizeof(*HandlerCtx));
            HandlerCtx->Result = &Resp->Result;
            DeleteFileOnSto(Req->FileId, Req->DirId, Sto, Context->SPDKContext, HandlerCtx);
        }
            break;
        case CTRL_MSG_F2B_REQ_CHANGE_FILE_SIZE: {
            CtrlMsgF2BReqChangeFileSize *Req = Context->Request;
            CtrlMsgB2FAckChangeFileSize *Resp = Context->Response;
            ChangeFileSize(Req->FileId, Req->NewSize, Sto, Resp);
        }
            break;
        case CTRL_MSG_F2B_REQ_GET_FILE_SIZE: {
            CtrlMsgF2BReqGetFileSize *Req = Context->Request;
            CtrlMsgB2FAckGetFileSize *Resp = Context->Response;
            GetFileSize(Req->FileId, &Resp->FileSize, Sto, Resp);
        }
            break;
        case CTRL_MSG_F2B_REQ_GET_FILE_INFO: {
            CtrlMsgF2BReqGetFileInfo *Req = Context->Request;
             CtrlMsgB2FAckGetFileInfo *Resp = Context->Response;
             GetFileInformationById(Req->FileId, &Resp->FileInfo, Sto, Resp);
        }
            break;
        case CTRL_MSG_F2B_REQ_GET_FILE_ATTR: {
            CtrlMsgF2BReqGetFileAttr *Req = Context->Request;
            CtrlMsgB2FAckGetFileAttr *Resp = Context->Response;
            GetFileAttributes(Req->FileId, &Resp->FileAttr, Sto, Resp);
        }
            break;
        case CTRL_MSG_F2B_REQ_GET_FREE_SPACE: {
            CtrlMsgF2BReqGetFreeSpace *Req = Context->Request;
            CtrlMsgB2FAckGetFreeSpace *Resp = Context->Response;
            GetStorageFreeSpace(&Resp->FreeSpace, Sto, Resp);
        }
            break;
        case CTRL_MSG_F2B_REQ_MOVE_FILE: {
            CtrlMsgF2BReqMoveFile *Req = Context->Request;
            CtrlMsgB2FAckMoveFile *Resp = Context->Response;
            ControlPlaneHandlerCtx *HandlerCtx = malloc(sizeof(*HandlerCtx));
            HandlerCtx->Result = &Resp->Result;
            MoveFile(Req->FileId, Req->OldDirId, Req->NewDirId, Req->NewFileName, Sto, Context->SPDKContext, HandlerCtx);
        }
            break;
        default: {
            printf("UNKNOWN CONTROL PLANE REQUEST: %d\n", Context->RequestId);
        }
    }
}