#pragma once

#include "BackEndInMemory.h"
#include "DDSBackEndBridgeBase.h"

#undef CreateDirectory
#undef RemoveDirectory
#undef CreateFile
#undef DeleteFile
#undef FindFirstFile
#undef FindNextFile
#undef GetFileAttributes
#undef GetCurrentDirectory
#undef MoveFile

namespace DDS_FrontEnd {

//
// A callback for back end
//
//
void
BackEndReadWriteCallbackHandler(
    ErrorCodeT ErrorCode,
    FileIOSizeT BytesServiced,
    ContextT Context
);

//
// Connector that fowards requests to and receives responses from the back end
//
//
class DDSBackEndBridgeForLocalMemory : public DDSBackEndBridgeBase {
private:
	DDS_BackEnd::BackEndInMemory* BackEnd;

public:
    DDSBackEndBridgeForLocalMemory();

    //
    // Connect to the backend
    //
    //
    ErrorCodeT
    Connect();

    //
    // Disconnect from the backend
    //
    //
    ErrorCodeT
    Disconnect();

	//
    // Create a diretory
    // 
    //
    ErrorCodeT
    CreateDirectory(
        const char* PathName,
        DirIdT DirId,
        DirIdT ParentId,
        PollT* Poll
    );

    //
    // Delete a directory
    //
    //
    ErrorCodeT
    RemoveDirectory(
        DirIdT DirId,
        PollT* Poll
    );

    //
    // Create a file
    // 
    //
    ErrorCodeT
    CreateFile(
        const char* FileName,
        FileAttributesT FileAttributes,
        FileIdT FileId,
        DirIdT DirId,
        PollT* Poll
    );

    //
    // Delete a file
    // 
    //
    ErrorCodeT
    DeleteFile(
        FileIdT FileId,
        DirIdT DirId,
        PollT* Poll
    );

    //
    // Change the size of a file
    // 
    //
    ErrorCodeT
    ChangeFileSize(
        FileIdT FileId,
        FileSizeT NewSize,
        PollT* Poll
    );

    //
    // Get file size
    // 
    //
    ErrorCodeT
    GetFileSize(
        FileIdT FileId,
        FileSizeT* FileSize,
        PollT* Poll
    );

    //
    // Async read from a file
    // 
    //
    ErrorCodeT
    ReadFile(
        FileIdT FileId,
        FileSizeT Offset,
        BufferT DestBuffer,
        FileIOSizeT BytesToRead,
        FileIOSizeT* BytesRead,
        ContextT Context,
        PollT* Poll
    );

    //
    // Async read from a file with scattering
    // 
    //
    ErrorCodeT
    ReadFileScatter(
        FileIdT FileId,
        FileSizeT Offset,
        BufferT* DestBufferArray,
        FileIOSizeT BytesToRead,
        FileIOSizeT* BytesRead,
        ContextT Context,
        PollT* Poll
    );

    //
    // Async write to a file
    // 
    //
    ErrorCodeT
    WriteFile(
        FileIdT FileId,
        FileSizeT Offset,
        BufferT SourceBuffer,
        FileIOSizeT BytesToWrite,
        FileIOSizeT* BytesWritten,
        ContextT Context,
        PollT* Poll
    );

    //
    // Async write to a file with gathering
    // 
    //
    ErrorCodeT
    WriteFileGather(
        FileIdT FileId,
        FileSizeT Offset,
        BufferT* SourceBufferArray,
        FileIOSizeT BytesToWrite,
        FileIOSizeT* BytesWritten,
        ContextT Context,
        PollT* Poll
    );

    //
    // Get file properties by file Id
    // 
    //
    ErrorCodeT
    GetFileInformationById(
        FileIdT FileId,
        FilePropertiesT* FileProperties,
        PollT* Poll
    );

    //
    // Get file attributes by file name
    // 
    //
    ErrorCodeT
    GetFileAttributes(
        FileIdT FileId,
        FileAttributesT* FileAttributes,
        PollT* Poll
    );

    //
    // Get the size of free space on the storage
    // 
    //
    ErrorCodeT
    GetStorageFreeSpace(
        FileSizeT* StorageFreeSpace,
        PollT* Poll
    );

    //
    // Move an existing file or a directory,
    // including its children
    // 
    //
    ErrorCodeT
    MoveFile(
        FileIdT FileId,
        const char* NewFileName,
        PollT* Poll
    );
};

}