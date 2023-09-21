#pragma once
#include "../../../Common/Include/MsgType.h"
#include "DPUBackEndStorage.h"


struct ControlPlaneHandlerCtx {
    // CtrlMsgF2BReqCreateDirectory *Req;
    // CtrlMsgB2FAckCreateDirectory *Resp;
    // struct CtrlConnConfig *CtrlConn;
    // struct ibv_send_wr *badSendWr;
    // struct ibv_recv_wr *badRecvWr;
    ErrorCodeT *Result;
    DirIdT DirId;  // because later we need to have this stmt: Sto->AllDirs[DirId] = dir;
    struct DPUDir* dir;
    FileIdT FileId;
    struct DPUFile* File;
    struct DPUDir* NewDir; // only used in MoveFile() as NewDir pointer
    char* NewFileName; // only used in MoveFile() as new file name
    // void *mystuff;  // TODO: if again we need some extra data, just add more
};

//
// Handler for a CreateDirectory request
//
//
void CreateDirectoryHandler(
    CtrlMsgF2BReqCreateDirectory *Req,
    CtrlMsgB2FAckCreateDirectory *Resp,
    struct ControlPlaneHandlerCtx *CtrlMsgHandlerCtx
);

// struct RemoveDirectoryHandlerCtx {
//     CtrlMsgF2BReqRemoveDirectory *Req;
//     CtrlMsgB2FAckRemoveDirectory *Resp;
//     // maybe more
// };

//
// Handler for a RemoveDirectory request
//
//
void RemoveDirectoryHandler(
    CtrlMsgF2BReqRemoveDirectory *Req,
    CtrlMsgB2FAckRemoveDirectory *Resp,
    struct ControlPlaneHandlerCtx *CtrlMsgHandlerCtx
);

// struct CreateFileHandlerCtx {
//     CtrlMsgF2BReqRemoveDirectory *Req;
//     CtrlMsgB2FAckRemoveDirectory *Resp;
//     // maybe more
// };

//
// Handler for a CreateFile request
//
//
void CreateFileHandler(
    CtrlMsgF2BReqCreateFile *Req,
    CtrlMsgB2FAckCreateFile *Resp,
    struct ControlPlaneHandlerCtx *CtrlMsgHandlerCtx
);

// struct DeleteFileHandlerCtx {
//     CtrlMsgF2BReqRemoveDirectory *Req;
//     CtrlMsgB2FAckRemoveDirectory *Resp;
//     // maybe more
// };

//
// Handler for a DeleteFile request
//
//
void DeleteFileHandler(
    CtrlMsgF2BReqDeleteFile *Req,
    CtrlMsgB2FAckDeleteFile *Resp,
    struct ControlPlaneHandlerCtx *CtrlMsgHandlerCtx
);

// struct ChangeFileHandlerCtx {
//     CtrlMsgF2BReqRemoveDirectory *Req;
//     CtrlMsgB2FAckRemoveDirectory *Resp;
//     // maybe more
// };

//
// Handler for a ChangeFileSize request
//
//
void ChangeFileSizeHandler(
    CtrlMsgF2BReqChangeFileSize *Req,
    CtrlMsgB2FAckChangeFileSize *Resp
);

// struct GetFileSizeHandlerCtx {
//     CtrlMsgF2BReqRemoveDirectory *Req;
//     CtrlMsgB2FAckRemoveDirectory *Resp;
//     // maybe more
// };

//
// Handler for a GetFileSize request
//
//
void GetFileSizeHandler(
    CtrlMsgF2BReqGetFileSize *Req,
    CtrlMsgB2FAckGetFileSize *Resp
);

// struct GetFileInformationByIdHandlerCtx {
//     CtrlMsgF2BReqRemoveDirectory *Req;
//     CtrlMsgB2FAckRemoveDirectory *Resp;
//     // maybe more
// };

//
// Handler for a GetFileInformationById request
//
//
void GetFileInformationByIdHandler(
    CtrlMsgF2BReqGetFileInfo *Req,
    CtrlMsgB2FAckGetFileInfo *Resp
);

// struct GetFileAttributesHandlerCtx {
//     CtrlMsgF2BReqRemoveDirectory *Req;
//     CtrlMsgB2FAckRemoveDirectory *Resp;
//     // maybe more
// };

//
// Handler for a GetFileAttributes request
//
//
void GetFileAttributesHandler(
    CtrlMsgF2BReqGetFileAttr *Req,
    CtrlMsgB2FAckGetFileAttr *Resp
);

// struct GetStorageFreeSpaceHandlerCtx {
//     CtrlMsgF2BReqRemoveDirectory *Req;
//     CtrlMsgB2FAckRemoveDirectory *Resp;
//     // maybe more
// };

//
// Handler for a GetStorageFreeSpace request
//
//
void GetStorageFreeSpaceHandler(
    CtrlMsgF2BReqGetFreeSpace *Req,
    CtrlMsgB2FAckGetFreeSpace *Resp
);

// struct MoveFileHandlerCtx {
//     CtrlMsgF2BReqRemoveDirectory *Req;
//     CtrlMsgB2FAckRemoveDirectory *Resp;
//     // maybe more
// };


//
// Handler for a MoveFile request
//
//
void MoveFileHandler(
    CtrlMsgF2BReqMoveFile *Req,
    CtrlMsgB2FAckMoveFile *Resp,
    struct ControlPlaneHandlerCtx *CtrlMsgHandlerCtx
);
