#include <stdio.h>

#include "ControlPlaneHandlers.h"

//
// Handler for a CreateDirectory request
//
//
void CreateDirectoryHandler(
    CtrlMsgF2BReqCreateDirectory *Req,
    CtrlMsgB2FAckCreateDirectory *Resp
) {
    printf("Creating a directory: %s\n", Req->PathName);

    //
    // TODO: Create the directory
    //
    //

    Resp->Result = DDS_ERROR_CODE_SUCCESS;
}

//
// Handler for a RemoveDirectory request
//
//
void CreateDirectoryHandler(
    CtrlMsgF2BReqRemoveDirectory *Req,
    CtrlMsgB2FAckRemoveDirectory *Resp
) {
    printf("Removing a directory: %u\n", Req->DirId);

    //
    // TODO: Remove the directory
    //
    //

    Resp->Result = DDS_ERROR_CODE_SUCCESS;
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

    Resp->Result = DDS_ERROR_CODE_SUCCESS;
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

    Resp->Result = DDS_ERROR_CODE_SUCCESS;
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

    Resp->Result = DDS_ERROR_CODE_SUCCESS;
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

    Resp->FileSize = 0;
    Resp->Result = DDS_ERROR_CODE_SUCCESS;
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
            
    memset(&Resp->FileInfo, 0, sizeof(Resp->FileInfo));
    Resp->Result = DDS_ERROR_CODE_SUCCESS;
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

    Resp->FileAttr = 0;
    Resp->Result = DDS_ERROR_CODE_SUCCESS;
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

    Resp->FreeSpace = 0;
    Resp->Result = DDS_ERROR_CODE_SUCCESS;
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

    Resp->Result = DDS_ERROR_CODE_SUCCESS;
}