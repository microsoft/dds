#pragma once

#include <BackEndInMemory.h>
#include "DDSFrontEndInterface.h"
#include "RDMC.h"
#include "MsgType.h"

#define BACKEND_ADDR "192.168.200.32"
#define BACKEND_PORT 4242

#define BACKEND_TYPE_IN_MEMORY 1
#define BACKEND_TYPE_DMA 2
#define BACKEND_TYPE BACKEND_TYPE_DMA

namespace DDS_FrontEnd {

//
// Callbacks for async operations
//
//
typedef void (*BackEndReadWriteCallback)(
    ErrorCodeT ErrorCode,
    FileIOSizeT BytesServiced,
    ContextT Context
);

//
// Connector that fowards requests to and receives responses from the back end
//
//
class BackEndBridge {
private:
#if BACKEND_TYPE == BACKEND_TYPE_IN_MEMORY
	DDS_BackEnd::BackEndInMemory* BackEnd;
#elif BACKEND_TYPE == BACKEND_TYPE_DMA
    //
    // Back end configuration
    //
    //
    char BackEndAddr[16];
    unsigned short BackEndPort;
    struct sockaddr_in BackEndSock;

    //
    // RNIC configuration
    //
    //
    IND2Adapter* Adapter;
    HANDLE AdapterFileHandle;
    ND2_ADAPTER_INFO AdapterInfo;
    OVERLAPPED Ov;
    size_t QueueDepth;
    size_t MaxSge;
    size_t InlineThreshold;
    struct sockaddr_in LocalSock;

    IND2Connector* CtrlConnector;
    IND2CompletionQueue* CtrlCompQ;
    IND2QueuePair* CtrlQPair;
    IND2MemoryRegion* CtrlMemRegion;
    ND2_SGE* CtrlSgl;
    char CtrlMsgBuf[CTRL_MSG_SIZE];

    int ClientId;
#endif

public:
    BackEndBridge();

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
        DirIdT ParentId
    );

    //
    // Delete a directory
    //
    //
    ErrorCodeT
    RemoveDirectory(
        DirIdT DirId
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
        DirIdT DirId
    );

    //
    // Delete a file
    // 
    //
    ErrorCodeT
    DeleteFile(
        FileIdT FileId,
        DirIdT DirId
    );

    //
    // Change the size of a file
    // 
    //
    ErrorCodeT
    ChangeFileSize(
        FileIdT FileId,
        FileSizeT NewSize
    );

    //
    // Get file size
    // 
    //
    ErrorCodeT
    GetFileSize(
        FileIdT FileId,
        FileSizeT* FileSize
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
        BackEndReadWriteCallback Callback,
        ContextT Context
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
        BackEndReadWriteCallback Callback,
        ContextT Context
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
        BackEndReadWriteCallback Callback,
        ContextT Context
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
        BackEndReadWriteCallback Callback,
        ContextT Context
    );

    //
    // Get file properties by file Id
    // 
    //
    ErrorCodeT
    GetFileInformationById(
        FileIdT FileId,
        FilePropertiesT* FileProperties
    );

    //
    // Get file attributes by file name
    // 
    //
    ErrorCodeT
    GetFileAttributes(
        FileIdT FileId,
        FileAttributesT* FileAttributes
    );

    //
    // Get the size of free space on the storage
    // 
    //
    ErrorCodeT
    GetStorageFreeSpace(
        FileSizeT* StorageFreeSpace
    );

    //
    // Move an existing file or a directory,
    // including its children
    // 
    //
    ErrorCodeT
    MoveFile(
        FileIdT FileId,
        const char* NewFileName
    );
};

}