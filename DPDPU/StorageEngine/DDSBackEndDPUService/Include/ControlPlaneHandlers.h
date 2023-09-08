#include "../../../Common/Include/MsgType.h"
#include "DPUBackEndStorage.h"

//
// Handler for a CreateDirectory request
//
//
void CreateDirectoryHandler(
    CtrlMsgF2BReqCreateDirectory *Req,
    CtrlMsgB2FAckCreateDirectory *Resp
);

//
// Handler for a RemoveDirectory request
//
//
void RemoveDirectoryHandler(
    CtrlMsgF2BReqRemoveDirectory *Req,
    CtrlMsgB2FAckRemoveDirectory *Resp
);

//
// Handler for a CreateFile request
//
//
void CreateFileHandler(
    CtrlMsgF2BReqCreateFile *Req,
    CtrlMsgB2FAckCreateFile *Resp
);
//
// Handler for a DeleteFile request
//
//
void DeleteFileHandler(
    CtrlMsgF2BReqDeleteFile *Req,
    CtrlMsgB2FAckDeleteFile *Resp
);

//
// Handler for a ChangeFileSize request
//
//
void ChangeFileSizeHandler(
    CtrlMsgF2BReqChangeFileSize *Req,
    CtrlMsgB2FAckChangeFileSize *Resp
);

//
// Handler for a GetFileSize request
//
//
void GetFileSizeHandler(
    CtrlMsgF2BReqGetFileSize *Req,
    CtrlMsgB2FAckGetFileSize *Resp
);

//
// Handler for a GetFileInformationById request
//
//
void GetFileInformationByIdHandler(
    CtrlMsgF2BReqGetFileInfo *Req,
    CtrlMsgB2FAckGetFileInfo *Resp
);

//
// Handler for a GetFileAttributes request
//
//
void GetFileAttributesHandler(
    CtrlMsgF2BReqGetFileAttr *Req,
    CtrlMsgB2FAckGetFileAttr *Resp
);

//
// Handler for a GetStorageFreeSpace request
//
//
void GetStorageFreeSpaceHandler(
    CtrlMsgF2BReqGetFreeSpace *Req,
    CtrlMsgB2FAckGetFreeSpace *Resp
);

//
// Handler for a MoveFile request
//
//
void MoveFileHandler(
    CtrlMsgF2BReqMoveFile *Req,
    CtrlMsgB2FAckMoveFile *Resp
);
