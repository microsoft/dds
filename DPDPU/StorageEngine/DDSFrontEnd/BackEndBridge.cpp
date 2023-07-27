#include <string.h>

#include "BackEndBridge.h"

namespace DDS_FrontEnd {

BackEndBridge::BackEndBridge() {
#if BACKEND_TYPE == BACKEND_TYPE_DMA
    //
    // Record buffer capacity and back end address and port
    //
    //
    strcpy(BackEndAddr, BACKEND_ADDR);
    BackEndPort = BACKEND_PORT;
    memset(&BackEndSock, 0, sizeof(BackEndSock));

    //
    // Initialize NDSPI variables
    //
    //
    Adapter = NULL;
    AdapterFileHandle = NULL;
    memset(&AdapterInfo, 0, sizeof(ND2_ADAPTER_INFO));
    memset(&Ov, 0, sizeof(Ov));
    QueueDepth = 0;
    MaxSge = 0;
    InlineThreshold = 0;
    memset(&LocalSock, 0, sizeof(LocalSock));

    CtrlConnector = NULL;
    CtrlCompQ = NULL;
    CtrlQPair = NULL;
    CtrlMemRegion = NULL;
    CtrlSgl = NULL;
    memset(CtrlMsgBuf, 0, CTRL_MSG_SIZE);

    ClientId = -1;
#endif
}

//
// Connect to the backend
//
//
ErrorCodeT
BackEndBridge::Connect() {
#if BACKEND_TYPE == BACKEND_TYPE_IN_MEMORY
    if (!BackEnd) {
        BackEnd = new DDS_BackEnd::BackEndInMemory();
    }

    return DDS_ERROR_CODE_SUCCESS;
#elif BACKEND_TYPE == BACKEND_TYPE_DMA
    //
    // Set up RDMA with NDSPI
    //
    //
    WSADATA wsaData;
    int ret = ::WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (ret != 0) {
        printf("BackEndBridge: failed to initialize Windows Sockets: %d\n", ret);
        return false;
    }

    HRESULT hr = NdStartup();
    if (FAILED(hr)) {
        printf("NdStartup failed with %08x\n", hr);
        return false;
    }

    wchar_t wstrManagerAddr[32];
    mbstowcs(wstrManagerAddr, BackEndAddr, strlen(BackEndAddr) + 1);
    LPWSTR lpwManagerAddr = wstrManagerAddr;
    int lenSock = sizeof(BackEndSock);
    WSAStringToAddress(
        lpwManagerAddr,
        AF_INET,
        nullptr,
        reinterpret_cast<struct sockaddr*>(&(BackEndSock)),
        &lenSock
    );

    if (BackEndSock.sin_addr.s_addr == 0) {
        printf("BackEndBridge: bad address for the backend\n");
        return false;
    }

    BackEndSock.sin_port = htons(BackEndPort);

    SIZE_T lenSrcSock = sizeof(LocalSock);
    hr = NdResolveAddress((const struct sockaddr*)&BackEndSock,
        sizeof(BackEndSock), (struct sockaddr*)&LocalSock, &lenSrcSock);
    if (FAILED(hr)) {
        printf("NdResolveAddress failed with %08x\n", hr);
        return false;
    }

    hr = NdOpenAdapter(
        IID_IND2Adapter,
        reinterpret_cast<const struct sockaddr*>(&LocalSock),
        sizeof(LocalSock),
        reinterpret_cast<void**>(&Adapter)
    );
    if (FAILED(hr)) {
        printf("NdOpenAdapter failed with %08x\n", hr);
        return false;
    }

    Ov.hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (Ov.hEvent == nullptr) {
        printf("BackEndBridge: failed to allocate event for overlapped operations\n");
        return false;
    }

    hr = Adapter->CreateOverlappedFile(&AdapterFileHandle);
    if (FAILED(hr))
    {
        printf("CreateOverlappedFile failed with %08x\n", hr);
        return false;
    }

    AdapterInfo.InfoVersion = ND_VERSION_2;
    ULONG adapterInfoSize = sizeof(AdapterInfo);
    hr = Adapter->Query(&AdapterInfo, &adapterInfoSize);
    if (FAILED(hr)) {
        printf("IND2Adapter::GetAdapterInfo failed with %08x\n", hr);
        return false;
    }

    if ((AdapterInfo.AdapterFlags & ND_ADAPTER_FLAG_IN_ORDER_DMA_SUPPORTED) == 0) {
        printf("Adapter does not support in-order RDMA\n");
        return false;
    }

    QueueDepth = AdapterInfo.MaxCompletionQueueDepth;
    QueueDepth = min(QueueDepth, AdapterInfo.MaxInitiatorQueueDepth);
    QueueDepth = min(QueueDepth, AdapterInfo.MaxOutboundReadLimit);
    MaxSge = min(AdapterInfo.MaxInitiatorSge, AdapterInfo.MaxReadSge);
    InlineThreshold = AdapterInfo.InlineRequestThreshold;

    //
    // Connect to the back end
    //
    //
    RDMC_OpenAdapter(&Adapter, &BackEndSock, &AdapterFileHandle, &Ov);
    RDMC_CreateConnector(Adapter, AdapterFileHandle, &CtrlConnector);
    RDMC_CreateCQ(Adapter, AdapterFileHandle, QueueDepth, &CtrlCompQ);
    RDMC_CreateQueuePair(Adapter, CtrlCompQ, QueueDepth, MaxSge, InlineThreshold, &CtrlQPair);

    RDMC_CreateMR(Adapter, AdapterFileHandle, &CtrlMemRegion);
    uint64_t flags = ND_MR_FLAG_ALLOW_LOCAL_WRITE | ND_MR_FLAG_ALLOW_REMOTE_READ | ND_MR_FLAG_ALLOW_REMOTE_WRITE;
    RDMC_RegisterDataBuffer(CtrlMemRegion, CtrlMsgBuf, CTRL_MSG_SIZE, flags, &Ov);

    CtrlSgl = new ND2_SGE[1];
    CtrlSgl[0].Buffer = CtrlMsgBuf;
    CtrlSgl[0].BufferLength = CTRL_MSG_SIZE;
    CtrlSgl[0].MemoryRegionToken = CtrlMemRegion->GetLocalToken();

    RDMC_Connect(CtrlConnector, CtrlQPair, &Ov, LocalSock, BackEndSock, 0, QueueDepth);
    RDMC_CompleteConnect(CtrlConnector, &Ov);

    //
    // Request client id and wait for response
    //
    //
    ((MsgHeader*)CtrlMsgBuf)->MsgId = CTRL_MSG_F2B_REQUEST_ID;
    ((CtrlMsgF2BRequestId*)(CtrlMsgBuf + sizeof(MsgHeader)))->Dummy = 0;
    CtrlSgl->BufferLength = sizeof(MsgHeader) + sizeof(CtrlMsgF2BRequestId);
    RDMC_Send(CtrlQPair, CtrlSgl, 1, 0, MSG_CTXT);
    RDMC_WaitForCompletionAndCheckContext(CtrlCompQ, &Ov, MSG_CTXT, false);

    CtrlSgl->BufferLength = CTRL_MSG_SIZE;
    RDMC_PostReceive(CtrlQPair, CtrlSgl, 1, MSG_CTXT);
    RDMC_WaitForCompletionAndCheckContext(CtrlCompQ, &Ov, MSG_CTXT, false);

    if (((MsgHeader*)CtrlMsgBuf)->MsgId == CTRL_MSG_B2F_RESPOND_ID) {
        ClientId = ((CtrlMsgB2FRespondId*)(CtrlMsgBuf + sizeof(MsgHeader)))->ClientId;
        printf("BackEndBridge: connected to the back end with assigned Id (%d)\n", ClientId);
    }
    else {
        printf("BackEndBridge: wrong message from the back end\n");
        return DDS_ERROR_CODE_UNEXPECTED_MSG;
    }

    return DDS_ERROR_CODE_SUCCESS;
#else
    return DDS_ERROR_CODE_NOT_IMPLEMENTED;
#endif
}

//
// Disconnect from the backend
//
//
ErrorCodeT
BackEndBridge::Disconnect() {
#if BACKEND_TYPE == BACKEND_TYPE_IN_MEMORY
    if (BackEnd) {
        delete BackEnd;
        BackEnd = nullptr;
    }

    return DDS_ERROR_CODE_SUCCESS;
#elif BACKEND_TYPE == BACKEND_TYPE_DMA
    //
    // Send the exit message to the back end
    //
    //
    if (ClientId >= 0) {
        ((MsgHeader*)CtrlMsgBuf)->MsgId = CTRL_MSG_F2B_TERMINATE;
        ((CtrlMsgF2BTerminate*)(CtrlMsgBuf + sizeof(MsgHeader)))->ClientId = ClientId;
        CtrlSgl->BufferLength = sizeof(MsgHeader) + sizeof(CtrlMsgF2BTerminate);
        RDMC_Send(CtrlQPair, CtrlSgl, 1, 0, MSG_CTXT);
        RDMC_WaitForCompletionAndCheckContext(CtrlCompQ, &Ov, MSG_CTXT, false);
        printf("BackEndBridge: signaled the back end to exit\n");
    }

    //
    // Disconnect and release all resources
    //
    //
    if (CtrlConnector) {
        CtrlConnector->Disconnect(&Ov);
        CtrlConnector->Release();
    }
    
    if (CtrlMemRegion) {
        CtrlMemRegion->Deregister(&Ov);
        CtrlMemRegion->Release();
    }
    
    if (CtrlCompQ) {
        CtrlCompQ->Release();
    }
    
    if (CtrlQPair) {
        CtrlQPair->Release();
    }
    
    if (AdapterFileHandle) {
        CloseHandle(AdapterFileHandle);
    }
    
    if (Adapter) {
        Adapter->Release();
    }
    
    if (Ov.hEvent) {
        CloseHandle(Ov.hEvent);
    }

    //
    // Release memory buffer
    //
    //
    if (CtrlSgl) {
        delete[] CtrlSgl;
    }

    //
    // RDMA cleaning
    //
    //
    HRESULT hr = NdCleanup();
    if (FAILED(hr))
    {
        printf("NdCleanup failed with %08x\n", hr);
    }

    _fcloseall();
    WSACleanup();

    return DDS_ERROR_CODE_SUCCESS;
#endif
}

//
// Create a diretory
// 
//
ErrorCodeT
BackEndBridge::CreateDirectory(
    const char* PathName,
    DirIdT DirId,
    DirIdT ParentId
) {
#if BACKEND_TYPE == BACKEND_TYPE_IN_MEMORY
    return BackEnd->CreateDirectory(
        PathName,
        (DDS_BackEnd::DirIdT)DirId,
        (DDS_BackEnd::DirIdT)ParentId
    );
#else
    return DDS_ERROR_CODE_NOT_IMPLEMENTED;
#endif
}

//
// Delete a directory
// 
//
ErrorCodeT
BackEndBridge::RemoveDirectory(
    DirIdT DirId
) {
#if BACKEND_TYPE == BACKEND_TYPE_IN_MEMORY
    return BackEnd->RemoveDirectory(
        (DDS_BackEnd::DirIdT)DirId
    );
#else
    return DDS_ERROR_CODE_NOT_IMPLEMENTED;
#endif
}

//
// Create a file
// 
//
ErrorCodeT
BackEndBridge::CreateFile(
    const char* FileName,
    FileAttributesT FileAttributes,
    FileIdT FileId,
    DirIdT DirId
) {
#if BACKEND_TYPE == BACKEND_TYPE_IN_MEMORY
    return BackEnd->CreateFile(
        FileName,
        (DDS_BackEnd::FileAttributesT)FileAttributes,
        (DDS_BackEnd::FileIdT)FileId,
        (DDS_BackEnd::DirIdT)DirId
    );
#else
    return DDS_ERROR_CODE_NOT_IMPLEMENTED;
#endif
}

//
// Delete a file
// 
//
ErrorCodeT
BackEndBridge::DeleteFile(
    FileIdT FileId,
    DirIdT DirId
) {
#if BACKEND_TYPE == BACKEND_TYPE_IN_MEMORY
    return BackEnd->DeleteFile(
        (DDS_BackEnd::FileIdT)FileId,
        (DDS_BackEnd::DirIdT)DirId
    );
#else
    return DDS_ERROR_CODE_NOT_IMPLEMENTED;
#endif
}

//
// Change the size of a file
// 
//
ErrorCodeT
BackEndBridge::ChangeFileSize(
    FileIdT FileId,
    FileSizeT NewSize
) {
#if BACKEND_TYPE == BACKEND_TYPE_IN_MEMORY
    return BackEnd->ChangeFileSize(
        (DDS_BackEnd::FileIdT)FileId,
        (DDS_BackEnd::FileSizeT)NewSize
    );
#else
    return DDS_ERROR_CODE_NOT_IMPLEMENTED;
#endif
}

//
// Get file size
// 
//
ErrorCodeT
BackEndBridge::GetFileSize(
    FileIdT FileId,
    FileSizeT* FileSize
) {
#if BACKEND_TYPE == BACKEND_TYPE_IN_MEMORY
    return BackEnd->GetFileSize(
        (DDS_BackEnd::FileIdT)FileId,
        (DDS_BackEnd::FileSizeT*)FileSize
    );
#else
    return DDS_ERROR_CODE_NOT_IMPLEMENTED;
#endif
}

//
// Async read from a file
// 
//
ErrorCodeT
BackEndBridge::ReadFile(
    FileIdT FileId,
    FileSizeT Offset,
    BufferT DestBuffer,
    FileIOSizeT BytesToRead,
    FileIOSizeT* BytesRead,
    BackEndReadWriteCallback Callback,
    ContextT Context
) {
#if BACKEND_TYPE == BACKEND_TYPE_IN_MEMORY
    return BackEnd->ReadFile(
        (DDS_BackEnd::FileIdT)FileId,
        (DDS_BackEnd::FileSizeT)Offset,
        (DDS_BackEnd::BufferT)DestBuffer,
        (DDS_BackEnd::FileIOSizeT)BytesToRead,
        (DDS_BackEnd::FileIOSizeT*)BytesRead,
        (DDS_BackEnd::ReadWriteCallback)Callback,
        (DDS_BackEnd::ContextT)Context
    );
#else
    return DDS_ERROR_CODE_NOT_IMPLEMENTED;
#endif
}

//
// Async read from a file with scattering
// 
//
ErrorCodeT
BackEndBridge::ReadFileScatter(
    FileIdT FileId,
    FileSizeT Offset,
    BufferT* DestBufferArray,
    FileIOSizeT BytesToRead,
    FileIOSizeT* BytesRead,
    BackEndReadWriteCallback Callback,
    ContextT Context
) {
#if BACKEND_TYPE == BACKEND_TYPE_IN_MEMORY
    return BackEnd->ReadFileScatter(
        (DDS_BackEnd::FileIdT)FileId,
        (DDS_BackEnd::FileSizeT)Offset,
        (DDS_BackEnd::BufferT*)DestBufferArray,
        (DDS_BackEnd::FileIOSizeT)BytesToRead,
        (DDS_BackEnd::FileIOSizeT*)BytesRead,
        (DDS_BackEnd::ReadWriteCallback)Callback,
        (DDS_BackEnd::ContextT)Context
    );
#else
    return DDS_ERROR_CODE_NOT_IMPLEMENTED;
#endif
}

//
// Async write to a file
// 
//
ErrorCodeT
BackEndBridge::WriteFile(
    FileIdT FileId,
    FileSizeT Offset,
    BufferT SourceBuffer,
    FileIOSizeT BytesToWrite,
    FileIOSizeT* BytesWritten,
    BackEndReadWriteCallback Callback,
    ContextT Context
) {
#if BACKEND_TYPE == BACKEND_TYPE_IN_MEMORY
    return BackEnd->WriteFile(
        (DDS_BackEnd::FileIdT)FileId,
        (DDS_BackEnd::FileSizeT)Offset,
        (DDS_BackEnd::BufferT)SourceBuffer,
        (DDS_BackEnd::FileIOSizeT)BytesToWrite,
        (DDS_BackEnd::FileIOSizeT*)BytesWritten,
        (DDS_BackEnd::ReadWriteCallback)Callback,
        (DDS_BackEnd::ContextT)Context
    );
#else
    return DDS_ERROR_CODE_NOT_IMPLEMENTED;
#endif
}

//
// Async write to a file with gathering
// 
//
ErrorCodeT
BackEndBridge::WriteFileGather(
    FileIdT FileId,
    FileSizeT Offset,
    BufferT* SourceBufferArray,
    FileIOSizeT BytesToWrite,
    FileIOSizeT* BytesWritten,
    BackEndReadWriteCallback Callback,
    ContextT Context
) {
#if BACKEND_TYPE == BACKEND_TYPE_IN_MEMORY
    return BackEnd->WriteFileGather(
        (DDS_BackEnd::FileIdT)FileId,
        (DDS_BackEnd::FileSizeT)Offset,
        (DDS_BackEnd::BufferT*)SourceBufferArray,
        (DDS_BackEnd::FileIOSizeT)BytesToWrite,
        (DDS_BackEnd::FileIOSizeT*)BytesWritten,
        (DDS_BackEnd::ReadWriteCallback)Callback,
        (DDS_BackEnd::ContextT)Context
    );
#else
    return DDS_ERROR_CODE_NOT_IMPLEMENTED;
#endif
}

//
// Get file properties by file Id
// 
//
ErrorCodeT
BackEndBridge::GetFileInformationById(
    FileIdT FileId,
    FilePropertiesT* FileProperties
) {
#if BACKEND_TYPE == BACKEND_TYPE_IN_MEMORY
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
#else
    return DDS_ERROR_CODE_NOT_IMPLEMENTED;
#endif
}

//
// Get file attributes by file name
// 
//
ErrorCodeT
BackEndBridge::GetFileAttributes(
    FileIdT FileId,
    FileAttributesT* FileAttributes
) {
#if BACKEND_TYPE == BACKEND_TYPE_IN_MEMORY
    return BackEnd->GetFileAttributes(
        FileId,
        (DDS_BackEnd::FileAttributesT*)FileAttributes
    );
#else
    return DDS_ERROR_CODE_NOT_IMPLEMENTED;
#endif
}

//
// Get the size of free space on the storage
// 
//
ErrorCodeT
BackEndBridge::GetStorageFreeSpace(
    FileSizeT* StorageFreeSpace
) {
#if BACKEND_TYPE == BACKEND_TYPE_IN_MEMORY
    return BackEnd->GetStorageFreeSpace(
        (DDS_BackEnd::FileSizeT*)StorageFreeSpace
    );
#else
    return DDS_ERROR_CODE_NOT_IMPLEMENTED;
#endif
}

//
// Move an existing file or a directory,
// including its children
// 
//
ErrorCodeT
BackEndBridge::MoveFile(
    FileIdT FileId,
    const char* NewFileName
) {
#if BACKEND_TYPE == BACKEND_TYPE_IN_MEMORY
    return BackEnd->MoveFile(FileId, NewFileName);
#else
    return DDS_ERROR_CODE_NOT_IMPLEMENTED;
#endif
}

}