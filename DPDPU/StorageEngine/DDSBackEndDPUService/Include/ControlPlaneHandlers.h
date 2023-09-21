#pragma once
#include "../../../Common/Include/MsgType.h"
// #include "DPUBackEndStorage.h"

struct CreateDirectoryHandlerCtx {
    CtrlMsgF2BReqCreateDirectory *Req;
    CtrlMsgB2FAckCreateDirectory *Resp;
    /* struct CtrlConnConfig *CtrlConn;
    struct ibv_send_wr *badSendWr;
    struct ibv_recv_wr *badRecvWr; */
    // DirIdT DirId;  // this is just from the req
    struct DPUDir* dir;  // this is the dir we created, will be used later in the callback
    // void *mystuff;  // TODO: if we need some extra data like `dir` above, just add more
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

struct RemoveDirectoryHandlerCtx {
    CtrlMsgF2BReqRemoveDirectory *Req;
    CtrlMsgB2FAckRemoveDirectory *Resp;
    // maybe more
};

//
// Handler for a RemoveDirectory request
//
//
void RemoveDirectoryHandler(
    CtrlMsgF2BReqRemoveDirectory *Req,
    CtrlMsgB2FAckRemoveDirectory *Resp,
    struct RemoveDirectoryHandlerCtx *CtrlMsgHandlerCtx
);

struct CreateFileHandlerCtx {
    CtrlMsgF2BReqRemoveDirectory *Req;
    CtrlMsgB2FAckRemoveDirectory *Resp;
    // maybe more
};

//
// Handler for a CreateFile request
//
//
void CreateFileHandler(
    CtrlMsgF2BReqCreateFile *Req,
    CtrlMsgB2FAckCreateFile *Resp,
    struct CreateDirectoryHandlerCtx *CtrlMsgHandlerCtx
);

struct DeleteFileHandlerCtx {
    CtrlMsgF2BReqRemoveDirectory *Req;
    CtrlMsgB2FAckRemoveDirectory *Resp;
    // maybe more
};

//
// Handler for a DeleteFile request
//
//
void DeleteFileHandler(
    CtrlMsgF2BReqDeleteFile *Req,
    CtrlMsgB2FAckDeleteFile *Resp,
    struct DeleteFileHandlerCtx *CtrlMsgHandlerCtx
);

struct ChangeFileHandlerCtx {
    CtrlMsgF2BReqRemoveDirectory *Req;
    CtrlMsgB2FAckRemoveDirectory *Resp;
    // maybe more
};

//
// Handler for a ChangeFileSize request
//
//
void ChangeFileSizeHandler(
    CtrlMsgF2BReqChangeFileSize *Req,
    CtrlMsgB2FAckChangeFileSize *Resp,
    struct ChangeFileHandlerCtx *CtrlMsgHandlerCtx
);

struct GetFileSizeHandlerCtx {
    CtrlMsgF2BReqRemoveDirectory *Req;
    CtrlMsgB2FAckRemoveDirectory *Resp;
    // maybe more
};

//
// Handler for a GetFileSize request
//
//
void GetFileSizeHandler(
    CtrlMsgF2BReqGetFileSize *Req,
    CtrlMsgB2FAckGetFileSize *Resp,
    struct GetFileSizeHandlerCtx *CtrlMsgHandlerCtx
);

struct GetFileInformationByIdHandlerCtx {
    CtrlMsgF2BReqRemoveDirectory *Req;
    CtrlMsgB2FAckRemoveDirectory *Resp;
    // maybe more
};

//
// Handler for a GetFileInformationById request
//
//
void GetFileInformationByIdHandler(
    CtrlMsgF2BReqGetFileInfo *Req,
    CtrlMsgB2FAckGetFileInfo *Resp,
    struct GetFileInformationByIdHandlerCtx *CtrlMsgHandlerCtx
);

struct GetFileAttributesHandlerCtx {
    CtrlMsgF2BReqRemoveDirectory *Req;
    CtrlMsgB2FAckRemoveDirectory *Resp;
    // maybe more
};

//
// Handler for a GetFileAttributes request
//
//
void GetFileAttributesHandler(
    CtrlMsgF2BReqGetFileAttr *Req,
    CtrlMsgB2FAckGetFileAttr *Resp,
    struct GetFileAttributesHandlerCtx *CtrlMsgHandlerCtx
);

struct GetStorageFreeSpaceHandlerCtx {
    CtrlMsgF2BReqRemoveDirectory *Req;
    CtrlMsgB2FAckRemoveDirectory *Resp;
    // maybe more
};

//
// Handler for a GetStorageFreeSpace request
//
//
void GetStorageFreeSpaceHandler(
    CtrlMsgF2BReqGetFreeSpace *Req,
    CtrlMsgB2FAckGetFreeSpace *Resp,
    struct GetStorageFreeSpaceHandlerCtx *CtrlMsgHandlerCtx
);

struct MoveFileHandlerCtx {
    CtrlMsgF2BReqRemoveDirectory *Req;
    CtrlMsgB2FAckRemoveDirectory *Resp;
    // maybe more
};


//
// Handler for a MoveFile request
//
//
void MoveFileHandler(
    CtrlMsgF2BReqMoveFile *Req,
    CtrlMsgB2FAckMoveFile *Resp,
    struct MoveFileHandlerCtx *CtrlMsgHandlerCtx
);
