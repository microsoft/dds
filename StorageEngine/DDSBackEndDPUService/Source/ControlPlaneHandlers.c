#include <stdio.h>
#include <string.h>

#include "ControlPlaneHandler.h"
#include "DPUBackEnd.h"
#include "DPUBackEndStorage.h"

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
    ControlPlaneRequestContext *Context = (ControlPlaneRequestContext *)ReqContext;

    switch (Context->RequestId) {
        case CTRL_MSG_F2B_REQ_CREATE_DIR: {
            CtrlMsgF2BReqCreateDirectory *Req = (CtrlMsgF2BReqCreateDirectory *)Context->Request;
            CtrlMsgB2FAckCreateDirectory *Resp = (CtrlMsgB2FAckCreateDirectory *)Context->Response;

            //
            // We should free handler ctx in the last callback for every case
            //
            //
            ControlPlaneHandlerCtx *HandlerCtx = malloc(sizeof(*HandlerCtx));
            HandlerCtx->Result = &Resp->Result;
            CreateDirectory(
                Req->PathName,
                Req->DirId,
                Req->ParentDirId,
                Sto,
                Context->SPDKContext,
                HandlerCtx
            );
        }
            break;
        case CTRL_MSG_F2B_REQ_REMOVE_DIR: {
            CtrlMsgF2BReqRemoveDirectory *Req = (CtrlMsgF2BReqRemoveDirectory *)Context->Request;
            CtrlMsgB2FAckRemoveDirectory *Resp = (CtrlMsgB2FAckRemoveDirectory *)Context->Response;
            ControlPlaneHandlerCtx *HandlerCtx = malloc(sizeof(*HandlerCtx));
            HandlerCtx->Result = &Resp->Result;
            RemoveDirectory(
                Req->DirId,
                Sto,
                Context->SPDKContext,
                HandlerCtx
            );

        }
            break;
        case CTRL_MSG_F2B_REQ_CREATE_FILE: {
            CtrlMsgF2BReqCreateFile *Req = (CtrlMsgF2BReqCreateFile *)Context->Request;
            CtrlMsgB2FAckCreateFile *Resp = (CtrlMsgB2FAckCreateFile *)Context->Response;
            ControlPlaneHandlerCtx *HandlerCtx = malloc(sizeof(*HandlerCtx));
            HandlerCtx->Result = &Resp->Result;
            CreateFile(
                Req->FileName,
                Req->FileAttributes,
                Req->FileId,
                Req->DirId,
                Sto,
                Context->SPDKContext,
                HandlerCtx
            );
        }
            break;
        case CTRL_MSG_F2B_REQ_DELETE_FILE: {
            CtrlMsgF2BReqDeleteFile *Req = (CtrlMsgF2BReqDeleteFile *)Context->Request;
            CtrlMsgB2FAckDeleteFile *Resp = (CtrlMsgB2FAckDeleteFile *)Context->Response;
            ControlPlaneHandlerCtx *HandlerCtx = malloc(sizeof(*HandlerCtx));
            HandlerCtx->Result = &Resp->Result;
            DeleteFileOnSto(
                Req->FileId,
                Req->DirId,
                Sto,
                Context->SPDKContext,
                HandlerCtx
            );
        }
            break;
        case CTRL_MSG_F2B_REQ_CHANGE_FILE_SIZE: {
            CtrlMsgF2BReqChangeFileSize *Req = (CtrlMsgF2BReqChangeFileSize *)Context->Request;
            CtrlMsgB2FAckChangeFileSize *Resp = (CtrlMsgB2FAckChangeFileSize *)Context->Response;
            ChangeFileSize(
                Req->FileId,
                Req->NewSize,
                Sto,
                Resp
            );
        }
            break;
        case CTRL_MSG_F2B_REQ_GET_FILE_SIZE: {
            CtrlMsgF2BReqGetFileSize *Req = (CtrlMsgF2BReqGetFileSize *)Context->Request;
            CtrlMsgB2FAckGetFileSize *Resp = (CtrlMsgB2FAckGetFileSize *)Context->Response;
            GetFileSize(
                Req->FileId,
                &Resp->FileSize,
                Sto,
                Resp
            );
        }
            break;
        case CTRL_MSG_F2B_REQ_GET_FILE_INFO: {
            CtrlMsgF2BReqGetFileInfo *Req = (CtrlMsgF2BReqGetFileInfo *)Context->Request;
            CtrlMsgB2FAckGetFileInfo *Resp = (CtrlMsgB2FAckGetFileInfo *)Context->Response;
            GetFileInformationById(
                Req->FileId,
                &Resp->FileInfo,
                Sto,
                Resp
            );
        }
            break;
        case CTRL_MSG_F2B_REQ_GET_FILE_ATTR: {
            CtrlMsgF2BReqGetFileAttr *Req = (CtrlMsgF2BReqGetFileAttr *)Context->Request;
            CtrlMsgB2FAckGetFileAttr *Resp = (CtrlMsgB2FAckGetFileAttr *)Context->Response;
            GetFileAttributes(
                Req->FileId,
                &Resp->FileAttr,
                Sto,
                Resp
            );
        }
            break;
        case CTRL_MSG_F2B_REQ_GET_FREE_SPACE: {
            CtrlMsgB2FAckGetFreeSpace *Resp = (CtrlMsgB2FAckGetFreeSpace *)Context->Response;
            GetStorageFreeSpace(
                &Resp->FreeSpace,
                Sto,
                Resp
            );
        }
            break;
        case CTRL_MSG_F2B_REQ_MOVE_FILE: {
            CtrlMsgF2BReqMoveFile *Req = (CtrlMsgF2BReqMoveFile *)Context->Request;
            CtrlMsgB2FAckMoveFile *Resp = (CtrlMsgB2FAckMoveFile *)Context->Response;
            ControlPlaneHandlerCtx *HandlerCtx = malloc(sizeof(*HandlerCtx));
            HandlerCtx->Result = &Resp->Result;
            MoveFile(
                Req->FileId,
                Req->OldDirId,
                Req->NewDirId,
                Req->NewFileName,
                Sto,
                Context->SPDKContext,
                HandlerCtx
            );
        }
            break;
        default: {
            printf("UNKNOWN CONTROL PLANE REQUEST: %d\n", Context->RequestId);
        }
    }
}
