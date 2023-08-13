#include <string.h>

#include "DDSBackEndBridgeForLocalMemory.h"

namespace DDS_FrontEnd {

DDSBackEndBridgeForLocalMemory::DDSBackEndBridgeForLocalMemory()
    : BackEnd(NULL) { }

//
// Connect to the backend
//
//
ErrorCodeT
DDSBackEndBridgeForLocalMemory::Connect() {
    if (!BackEnd) {
        BackEnd = new DDS_BackEnd::BackEndInMemory();
    }

    return DDS_ERROR_CODE_SUCCESS;
}

//
// Disconnect from the backend
//
//
ErrorCodeT
DDSBackEndBridgeForLocalMemory::Disconnect() {
    if (BackEnd) {
        delete BackEnd;
        BackEnd = nullptr;
    }

    return DDS_ERROR_CODE_SUCCESS;
}

//
// Create a diretory
// 
//
ErrorCodeT
DDSBackEndBridgeForLocalMemory::CreateDirectory(
    const char* PathName,
    DirIdT DirId,
    DirIdT ParentId
) {
    return BackEnd->CreateDirectory(
        PathName,
        (DDS_BackEnd::DirIdT)DirId,
        (DDS_BackEnd::DirIdT)ParentId
    );
}

//
// Delete a directory
// 
//
ErrorCodeT
DDSBackEndBridgeForLocalMemory::RemoveDirectory(
    DirIdT DirId
) {
    return BackEnd->RemoveDirectory(
        (DDS_BackEnd::DirIdT)DirId
    );
}

//
// Create a file
// 
//
ErrorCodeT
DDSBackEndBridgeForLocalMemory::CreateFile(
    const char* FileName,
    FileAttributesT FileAttributes,
    FileIdT FileId,
    DirIdT DirId
) {
    return BackEnd->CreateFile(
        FileName,
        (DDS_BackEnd::FileAttributesT)FileAttributes,
        (DDS_BackEnd::FileIdT)FileId,
        (DDS_BackEnd::DirIdT)DirId
    );
}

//
// Delete a file
// 
//
ErrorCodeT
DDSBackEndBridgeForLocalMemory::DeleteFile(
    FileIdT FileId,
    DirIdT DirId
) {
    return BackEnd->DeleteFile(
        (DDS_BackEnd::FileIdT)FileId,
        (DDS_BackEnd::DirIdT)DirId
    );
}

//
// Change the size of a file
// 
//
ErrorCodeT
DDSBackEndBridgeForLocalMemory::ChangeFileSize(
    FileIdT FileId,
    FileSizeT NewSize
) {
    return BackEnd->ChangeFileSize(
        (DDS_BackEnd::FileIdT)FileId,
        (DDS_BackEnd::FileSizeT)NewSize
    );
}

//
// Get file size
// 
//
ErrorCodeT
DDSBackEndBridgeForLocalMemory::GetFileSize(
    FileIdT FileId,
    FileSizeT* FileSize
) {
    return BackEnd->GetFileSize(
        (DDS_BackEnd::FileIdT)FileId,
        (DDS_BackEnd::FileSizeT*)FileSize
    );
}

//
// Async read from a file
// 
//
ErrorCodeT
DDSBackEndBridgeForLocalMemory::ReadFile(
    FileIdT FileId,
    FileSizeT Offset,
    BufferT DestBuffer,
    FileIOSizeT BytesToRead,
    FileIOSizeT* BytesRead,
    BackEndReadWriteCallback Callback,
    ContextT Context
) {
    return BackEnd->ReadFile(
        (DDS_BackEnd::FileIdT)FileId,
        (DDS_BackEnd::FileSizeT)Offset,
        (DDS_BackEnd::BufferT)DestBuffer,
        (DDS_BackEnd::FileIOSizeT)BytesToRead,
        (DDS_BackEnd::FileIOSizeT*)BytesRead,
        (DDS_BackEnd::ReadWriteCallback)Callback,
        (DDS_BackEnd::ContextT)Context
    );
}

//
// Async read from a file with scattering
// 
//
ErrorCodeT
DDSBackEndBridgeForLocalMemory::ReadFileScatter(
    FileIdT FileId,
    FileSizeT Offset,
    BufferT* DestBufferArray,
    FileIOSizeT BytesToRead,
    FileIOSizeT* BytesRead,
    BackEndReadWriteCallback Callback,
    ContextT Context
) {
    return BackEnd->ReadFileScatter(
        (DDS_BackEnd::FileIdT)FileId,
        (DDS_BackEnd::FileSizeT)Offset,
        (DDS_BackEnd::BufferT*)DestBufferArray,
        (DDS_BackEnd::FileIOSizeT)BytesToRead,
        (DDS_BackEnd::FileIOSizeT*)BytesRead,
        (DDS_BackEnd::ReadWriteCallback)Callback,
        (DDS_BackEnd::ContextT)Context
    );
}

//
// Async write to a file
// 
//
ErrorCodeT
DDSBackEndBridgeForLocalMemory::WriteFile(
    FileIdT FileId,
    FileSizeT Offset,
    BufferT SourceBuffer,
    FileIOSizeT BytesToWrite,
    FileIOSizeT* BytesWritten,
    BackEndReadWriteCallback Callback,
    ContextT Context
) {
    return BackEnd->WriteFile(
        (DDS_BackEnd::FileIdT)FileId,
        (DDS_BackEnd::FileSizeT)Offset,
        (DDS_BackEnd::BufferT)SourceBuffer,
        (DDS_BackEnd::FileIOSizeT)BytesToWrite,
        (DDS_BackEnd::FileIOSizeT*)BytesWritten,
        (DDS_BackEnd::ReadWriteCallback)Callback,
        (DDS_BackEnd::ContextT)Context
    );
}

//
// Async write to a file with gathering
// 
//
ErrorCodeT
DDSBackEndBridgeForLocalMemory::WriteFileGather(
    FileIdT FileId,
    FileSizeT Offset,
    BufferT* SourceBufferArray,
    FileIOSizeT BytesToWrite,
    FileIOSizeT* BytesWritten,
    BackEndReadWriteCallback Callback,
    ContextT Context
) {
    return BackEnd->WriteFileGather(
        (DDS_BackEnd::FileIdT)FileId,
        (DDS_BackEnd::FileSizeT)Offset,
        (DDS_BackEnd::BufferT*)SourceBufferArray,
        (DDS_BackEnd::FileIOSizeT)BytesToWrite,
        (DDS_BackEnd::FileIOSizeT*)BytesWritten,
        (DDS_BackEnd::ReadWriteCallback)Callback,
        (DDS_BackEnd::ContextT)Context
    );
}

//
// Get file properties by file Id
// 
//
ErrorCodeT
DDSBackEndBridgeForLocalMemory::GetFileInformationById(
    FileIdT FileId,
    FilePropertiesT* FileProperties
) {
    DDS_BackEnd::FilePropertiesT fileProperties;
    DDS_BackEnd::ErrorCodeT result = BackEnd->GetFileInformationById(
        (DDS_BackEnd::FileIdT)FileId,
        &fileProperties
    );
    strcpy(FileProperties->FileName, fileProperties.FileName);
    FileProperties->FileSize = fileProperties.FileSize;
    FileProperties->FileAttributes = fileProperties.FileAttributes;
    FileProperties->CreationTime = fileProperties.CreationTime;
    FileProperties->LastAccessTime = fileProperties.LastAccessTime;
    FileProperties->LastWriteTime = fileProperties.LastWriteTime;

    return result;
}

//
// Get file attributes by file name
// 
//
ErrorCodeT
DDSBackEndBridgeForLocalMemory::GetFileAttributes(
    FileIdT FileId,
    FileAttributesT* FileAttributes
) {
    return BackEnd->GetFileAttributes(
        FileId,
        (DDS_BackEnd::FileAttributesT*)FileAttributes
    );
}

//
// Get the size of free space on the storage
// 
//
ErrorCodeT
DDSBackEndBridgeForLocalMemory::GetStorageFreeSpace(
    FileSizeT* StorageFreeSpace
) {
    return BackEnd->GetStorageFreeSpace(
        (DDS_BackEnd::FileSizeT*)StorageFreeSpace
    );
}

//
// Move an existing file or a directory,
// including its children
// 
//
ErrorCodeT
DDSBackEndBridgeForLocalMemory::MoveFile(
    FileIdT FileId,
    const char* NewFileName
) {
    return BackEnd->MoveFile(FileId, NewFileName);
}

}