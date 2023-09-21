#include <stdio.h>
#include <string.h>

#include "../Include/ControlPlaneHandlers.h"

struct DPUStorage *Sto;
SPDKContextT *SPDKContext;

//
// Handler for a CreateDirectory request
//
//
void CreateDirectoryHandler(
    CtrlMsgF2BReqCreateDirectory *Req,
    CtrlMsgB2FAckCreateDirectory *Resp,
    struct ControlPlaneHandlerCtx *CtrlMsgHandlerCtx
) {
    printf("Creating a directory: %s\n", Req->PathName);

    //
    // TODO: Create the directory
    //
    //
    CreateDirectory(Req->PathName, Req->DirId, Req->ParentDirId, Sto, SPDKContext, CtrlMsgHandlerCtx);

}

//
// Handler for a RemoveDirectory request
//
//
void RemoveDirectoryHandler(
    CtrlMsgF2BReqRemoveDirectory *Req,
    CtrlMsgB2FAckRemoveDirectory *Resp,
    struct ControlPlaneHandlerCtx *CtrlMsgHandlerCtx
) {
    printf("Removing a directory: %u\n", Req->DirId);

    //
    // TODO: Remove the directory
    //
    //
    RemoveDirectory(Req->DirId, Sto, SPDKContext, CtrlMsgHandlerCtx);

}

//
// Handler for a CreateFile request
//
//
void CreateFileHandler(
    CtrlMsgF2BReqCreateFile *Req,
    CtrlMsgB2FAckCreateFile *Resp,
    struct ControlPlaneHandlerCtx *CtrlMsgHandlerCtx
) {
    printf("Creating a file: %s\n", Req->FileName);

    //
    // TODO: Create the file
    //
    //
    CreateFile(Req->FileName, Req->FileAttributes, Req->FileId, 
    Req->DirId, Sto, SPDKContext, CtrlMsgHandlerCtx);

}

//
// Handler for a DeleteFile request
//
//
void DeleteFileHandler(
    CtrlMsgF2BReqDeleteFile *Req,
    CtrlMsgB2FAckDeleteFile *Resp,
    struct ControlPlaneHandlerCtx *CtrlMsgHandlerCtx
) {
    printf("Deleting a file: %u\n", Req->FileId);

    //
    // TODO: Delete the file
    //
    //

    DeleteFileOnSto(Req->FileId, Req->DirId, Sto, SPDKContext, CtrlMsgHandlerCtx);
}

//
// Handler for a ChangeFileSize request
//
//
void ChangeFileSizeHandler(
    CtrlMsgF2BReqChangeFileSize *Req,
    CtrlMsgB2FAckChangeFileSize *Resp
) {
    printf("Changing the size of a file: %u\n", Req->FileId);

    //
    // TODO: Change the file size
    //
    //

    ChangeFileSize(Req->FileId, Req->NewSize, Sto, Resp);
}

//
// Handler for a GetFileSize request
//
//
void GetFileSizeHandler(
    CtrlMsgF2BReqGetFileSize *Req,
    CtrlMsgB2FAckGetFileSize *Resp
) {
    printf("Getting the size of a file: %u\n", Req->FileId);

    //
    // TODO: Get the file size
    //
    //
    GetFileSize(Req->FileId, &Resp->FileSize, Sto, Resp);
}

//
// Handler for a GetFileInformationById request
//
//
void GetFileInformationByIdHandler(
    CtrlMsgF2BReqGetFileInfo *Req,
    CtrlMsgB2FAckGetFileInfo *Resp
) {
    printf("Getting the properties of a file: %u\n", Req->FileId);

    //
    // TODO: Get the file info
    //
    //
    GetFileInformationById(Req->FileId, &Resp->FileInfo, Sto, Resp); 
}

//
// Handler for a GetFileAttributes request
//
//
void GetFileAttributesHandler(
    CtrlMsgF2BReqGetFileAttr *Req,
    CtrlMsgB2FAckGetFileAttr *Resp
) {
    printf("Getting the attributes of a file: %u\n", Req->FileId);

    //
    // TODO: Get the file attributes
    //
    //
    GetFileAttributes(Req->FileId, &Resp->FileAttr, Sto, Resp);
}

//
// Handler for a GetStorageFreeSpace request
//
//
void GetStorageFreeSpaceHandler(
    CtrlMsgF2BReqGetFreeSpace *Req,
    CtrlMsgB2FAckGetFreeSpace *Resp
) {
    printf("Getting storage free space (%d)\n", Req->Dummy);

    //
    // TODO: Get the free storage space
    //
    //
    GetStorageFreeSpace(&Resp->FreeSpace, Sto, Resp);
}

//
// Handler for a MoveFile request
//
//
void MoveFileHandler(
    CtrlMsgF2BReqMoveFile *Req,
    CtrlMsgB2FAckMoveFile *Resp,
    struct ControlPlaneHandlerCtx *CtrlMsgHandlerCtx
) {
    printf("Moving the file %u to %s\n", Req->FileId, Req->NewFileName);

    //
    // TODO: Move the file
    //
    //
    
    // NewDirID and OldDirID are required here.
    MoveFile(Req->FileId, Req->OldDirId, Req->NewDirId, Req->NewFileName, Sto, SPDKContext, CtrlMsgHandlerCtx);
}
