#pragma once
#include "../../../Common/Include/MsgType.h"
// #include "DPUBackEndStorage.h"

struct CreateDirectoryHandlerCtx {
    CtrlMsgF2BReqCreateDirectory *Req;
    CtrlMsgB2FAckCreateDirectory *Resp;
    struct CtrlConnConfig *CtrlConn;
    struct ibv_send_wr *badSendWr;
    struct ibv_recv_wr *badRecvWr;
    DirIdT DirId;  // because later we need to have this stmt: Sto->AllDirs[DirId] = dir;
    struct DPUDir* dir;
    void *mystuff;  // TODO: if again we need some extra data, just add more
};

//
// Handler for a CreateDirectory request
//
//
void CreateDirectoryHandler(
    CtrlMsgF2BReqCreateDirectory *Req,
    CtrlMsgB2FAckCreateDirectory *Resp,
    struct CreateDirectoryHandlerCtx *CtrlMsgHandlerCtx
);

//
// Handler for a RemoveDirectory request
//
//
void RemoveDirectoryHandler(
    CtrlMsgF2BReqRemoveDirectory *Req,
    CtrlMsgB2FAckRemoveDirectory *Resp,
    struct CreateDirectoryHandlerCtx *CtrlMsgHandlerCtx
);

//
// Handler for a CreateFile request
//
//
void CreateFileHandler(
    CtrlMsgF2BReqCreateFile *Req,
    CtrlMsgB2FAckCreateFile *Resp,
    struct CreateDirectoryHandlerCtx *CtrlMsgHandlerCtx
);
//
// Handler for a DeleteFile request
//
//
void DeleteFileHandler(
    CtrlMsgF2BReqDeleteFile *Req,
    CtrlMsgB2FAckDeleteFile *Resp,
    struct CreateDirectoryHandlerCtx *CtrlMsgHandlerCtx
);

//
// Handler for a ChangeFileSize request
//
//
void ChangeFileSizeHandler(
    CtrlMsgF2BReqChangeFileSize *Req,
    CtrlMsgB2FAckChangeFileSize *Resp,
    struct CreateDirectoryHandlerCtx *CtrlMsgHandlerCtx
);

//
// Handler for a GetFileSize request
//
//
void GetFileSizeHandler(
    CtrlMsgF2BReqGetFileSize *Req,
    CtrlMsgB2FAckGetFileSize *Resp,
    struct CreateDirectoryHandlerCtx *CtrlMsgHandlerCtx
);

//
// Handler for a GetFileInformationById request
//
//
void GetFileInformationByIdHandler(
    CtrlMsgF2BReqGetFileInfo *Req,
    CtrlMsgB2FAckGetFileInfo *Resp,
    struct CreateDirectoryHandlerCtx *CtrlMsgHandlerCtx
);

//
// Handler for a GetFileAttributes request
//
//
void GetFileAttributesHandler(
    CtrlMsgF2BReqGetFileAttr *Req,
    CtrlMsgB2FAckGetFileAttr *Resp,
    struct CreateDirectoryHandlerCtx *CtrlMsgHandlerCtx
);

//
// Handler for a GetStorageFreeSpace request
//
//
void GetStorageFreeSpaceHandler(
    CtrlMsgF2BReqGetFreeSpace *Req,
    CtrlMsgB2FAckGetFreeSpace *Resp,
    struct CreateDirectoryHandlerCtx *CtrlMsgHandlerCtx
);

//
// Handler for a MoveFile request
//
//
void MoveFileHandler(
    CtrlMsgF2BReqMoveFile *Req,
    CtrlMsgB2FAckMoveFile *Resp,
    struct CreateDirectoryHandlerCtx *CtrlMsgHandlerCtx
);
