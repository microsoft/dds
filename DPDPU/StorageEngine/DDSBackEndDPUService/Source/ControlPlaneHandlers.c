#include <stdio.h>
#include <string.h>

#include "../Include/ControlPlaneHandlers.h"

//
// Handler for a CreateDirectory request
//
//
void CreateDirectoryHandler(
    CtrlMsgF2BReqCreateDirectory *Req,
    CtrlMsgB2FAckCreateDirectory *Resp,
    struct DPUStorage* Sto,
    void *arg
) {
    printf("Creating a directory: %s\n", Req->PathName);

    //
    // TODO: Create the directory
    //
    //
    Resp->Result = CreateDirectory(Req->PathName, Req->DirId, Req->ParentDirId, Sto, arg);

}

//
// Handler for a RemoveDirectory request
//
//
void RemoveDirectoryHandler(
    CtrlMsgF2BReqRemoveDirectory *Req,
    CtrlMsgB2FAckRemoveDirectory *Resp,
    struct DPUStorage* Sto,
    void *arg
) {
    printf("Removing a directory: %u\n", Req->DirId);

    //
    // TODO: Remove the directory
    //
    //
    Resp->Result = RemoveDirectory(Req->DirId, Sto, arg);

}

//
// Handler for a CreateFile request
//
//
void CreateFileHandler(
    CtrlMsgF2BReqCreateFile *Req,
    CtrlMsgB2FAckCreateFile *Resp,
    struct DPUStorage* Sto,
    void *arg
) {
    printf("Creating a file: %s\n", Req->FileName);

    //
    // TODO: Create the file
    //
    //
    Resp->Result = CreateFile(Req->FileName, Req->FileAttributes, Req->FileId, 
    Req->DirId, Sto, arg);

}

//
// Handler for a DeleteFile request
//
//
void DeleteFileHandler(
    CtrlMsgF2BReqDeleteFile *Req,
    CtrlMsgB2FAckDeleteFile *Resp,
    struct DPUStorage* Sto,
    void *arg
) {
    printf("Deleting a file: %u\n", Req->FileId);

    //
    // TODO: Delete the file
    //
    //

    Resp->Result = DeleteFileOnSto(Req->FileId, Req->DirId, Sto, arg);
}

//
// Handler for a ChangeFileSize request
//
//
void ChangeFileSizeHandler(
    CtrlMsgF2BReqChangeFileSize *Req,
    CtrlMsgB2FAckChangeFileSize *Resp,
    struct DPUStorage* Sto
) {
    printf("Changing the size of a file: %u\n", Req->FileId);

    //
    // TODO: Change the file size
    //
    //

    Resp->Result = ChangeFileSize(Req->FileId, Req->NewSize, Sto);
}

//
// Handler for a GetFileSize request
//
//
void GetFileSizeHandler(
    CtrlMsgF2BReqGetFileSize *Req,
    CtrlMsgB2FAckGetFileSize *Resp,
    struct DPUStorage* Sto
) {
    printf("Getting the size of a file: %u\n", Req->FileId);

    //
    // TODO: Get the file size
    //
    //
    Resp->Result = GetFileSize(Req->FileId, &Resp->FileSize, Sto);
}

//
// Handler for a GetFileInformationById request
//
//
void GetFileInformationByIdHandler(
    CtrlMsgF2BReqGetFileInfo *Req,
    CtrlMsgB2FAckGetFileInfo *Resp,
    struct DPUStorage* Sto
) {
    printf("Getting the properties of a file: %u\n", Req->FileId);

    //
    // TODO: Get the file info
    //
    //
    Resp->Result = GetFileInformationById(Req->FileId, &Resp->FileInfo, Sto); 
}

//
// Handler for a GetFileAttributes request
//
//
void GetFileAttributesHandler(
    CtrlMsgF2BReqGetFileAttr *Req,
    CtrlMsgB2FAckGetFileAttr *Resp,
    struct DPUStorage* Sto
) {
    printf("Getting the attributes of a file: %u\n", Req->FileId);

    //
    // TODO: Get the file attributes
    //
    //
    Resp->Result = GetFileAttributes(Req->FileId, &Resp->FileAttr, Sto);
}

//
// Handler for a GetStorageFreeSpace request
//
//
void GetStorageFreeSpaceHandler(
    CtrlMsgF2BReqGetFreeSpace *Req,
    CtrlMsgB2FAckGetFreeSpace *Resp,
    struct DPUStorage* Sto
) {
    printf("Getting storage free space (%d)\n", Req->Dummy);

    //
    // TODO: Get the free storage space
    //
    //
    Resp->Result = GetStorageFreeSpace(&Resp->FreeSpace, Sto);
}

//
// Handler for a MoveFile request
//
//
void MoveFileHandler(
    CtrlMsgF2BReqMoveFile *Req,
    CtrlMsgB2FAckMoveFile *Resp,
    struct DPUStorage* Sto,
    void *arg
) {
    printf("Moving the file %u to %s\n", Req->FileId, Req->NewFileName);

    //
    // TODO: Move the file
    //
    //
    
    // NewDirID and OldDirID are required here.
    //Resp->Result = MoveFile(Req->FileId, ?,?,Req->NewFileName, Sto, arg);
    Resp->Result = DDS_ERROR_CODE_SUCCESS;
}
