#include <stdio.h>
#include <string.h>

#include "../Include/ControlPlaneHandlers.h"
#include "DPUBackEndStorage.h"

struct DPUStorage *Sto;
SPDKContextT *SPDKContext;

//
// Handler for a CreateDirectory request
//
//
void CreateDirectoryHandler(
    CtrlMsgF2BReqCreateDirectory *Req,
    CtrlMsgB2FAckCreateDirectory *Resp,
    struct CreateDirectoryHandlerCtx *CtrlMsgHandlerCtx
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
    CtrlMsgB2FAckRemoveDirectory *Resp
) {
    printf("Removing a directory: %u\n", Req->DirId);

    //
    // TODO: Remove the directory
    //
    //
    Resp->Result = RemoveDirectory(Req->DirId, NULL, NULL);

}

//
// Handler for a CreateFile request
//
//
void CreateFileHandler(
    CtrlMsgF2BReqCreateFile *Req,
    CtrlMsgB2FAckCreateFile *Resp
) {
    printf("Creating a file: %s\n", Req->FileName);

    //
    // TODO: Create the file
    //
    //
    Resp->Result = CreateFile(Req->FileName, Req->FileAttributes, Req->FileId, 
    Req->DirId, NULL, NULL);

}

//
// Handler for a DeleteFile request
//
//
void DeleteFileHandler(
    CtrlMsgF2BReqDeleteFile *Req,
    CtrlMsgB2FAckDeleteFile *Resp
) {
    printf("Deleting a file: %u\n", Req->FileId);

    //
    // TODO: Delete the file
    //
    //

    Resp->Result = DeleteFileOnSto(Req->FileId, Req->DirId, NULL, NULL);
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

    Resp->Result = ChangeFileSize(Req->FileId, Req->NewSize, NULL);
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
    Resp->Result = GetFileSize(Req->FileId, &Resp->FileSize, NULL);
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
    Resp->Result = GetFileInformationById(Req->FileId, &Resp->FileInfo, NULL); 
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
    Resp->Result = GetFileAttributes(Req->FileId, &Resp->FileAttr, NULL);
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
    Resp->Result = GetStorageFreeSpace(&Resp->FreeSpace, NULL);
}

//
// Handler for a MoveFile request
//
//
void MoveFileHandler(
    CtrlMsgF2BReqMoveFile *Req,
    CtrlMsgB2FAckMoveFile *Resp
) {
    printf("Moving the file %u to %s\n", Req->FileId, Req->NewFileName);

    //
    // TODO: Move the file
    //
    //
    
    // NewDirID and OldDirID are required here.
    //Resp->Result = MoveFile(Req->FileId, ?,?,Req->NewFileName, NULL, NULL);
    Resp->Result = DDS_ERROR_CODE_SUCCESS;
}
