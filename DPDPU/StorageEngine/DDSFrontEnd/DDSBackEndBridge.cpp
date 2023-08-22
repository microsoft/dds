#include <string.h>

#include "DDSBackEndBridge.h"

namespace DDS_FrontEnd {

DDSBackEndBridge::DDSBackEndBridge() {
    //
    // Record buffer capacity and back end address and port
    //
    //
    strcpy(BackEndAddr, DDS_BACKEND_ADDR);
    BackEndPort = DDS_BACKEND_PORT;
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
}

//
// Connect to the backend
//
//
ErrorCodeT
DDSBackEndBridge::Connect() {
    //
    // Set up RDMA with NDSPI
    //
    //
    WSADATA wsaData;
    int ret = ::WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (ret != 0) {
        printf("DDSBackEndBridge: failed to initialize Windows Sockets: %d\n", ret);
        return DDS_ERROR_CODE_FAILED_CONNECTION;
    }
#ifdef BACKEND_BRIDGE_VERBOSE
    else {
        printf("DDSBackEndBridge: succeeded to initialize Windows Sockets\n");
    }
#endif

    HRESULT hr = NdStartup();
    if (FAILED(hr)) {
        printf("NdStartup failed with %08x\n", hr);
        return DDS_ERROR_CODE_FAILED_CONNECTION;
    }
#ifdef BACKEND_BRIDGE_VERBOSE
    else {
        printf("NdStartup succeeded\n");
    }
#endif

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
        printf("DDSBackEndBridge: bad address for the backend\n");
        return DDS_ERROR_CODE_FAILED_CONNECTION;
    }
#ifdef BACKEND_BRIDGE_VERBOSE
    else {
        printf("DDSBackEndBridge: good address for the backend\n");
    }
#endif

    BackEndSock.sin_port = htons(BackEndPort);

    SIZE_T lenSrcSock = sizeof(LocalSock);
    hr = NdResolveAddress((const struct sockaddr*)&BackEndSock,
        sizeof(BackEndSock), (struct sockaddr*)&LocalSock, &lenSrcSock);
    if (FAILED(hr)) {
        printf("NdResolveAddress failed with %08x\n", hr);
        return DDS_ERROR_CODE_FAILED_CONNECTION;
    }
#ifdef BACKEND_BRIDGE_VERBOSE
    else {
        printf("NdResolveAddress succeeded\n");
    }
#endif

    hr = NdOpenAdapter(
        IID_IND2Adapter,
        reinterpret_cast<const struct sockaddr*>(&LocalSock),
        sizeof(LocalSock),
        reinterpret_cast<void**>(&Adapter)
    );
    if (FAILED(hr)) {
        printf("NdOpenAdapter failed with %08x\n", hr);
        return DDS_ERROR_CODE_FAILED_CONNECTION;
    }
#ifdef BACKEND_BRIDGE_VERBOSE
    else {
        printf("NdOpenAdapter succeeded\n");
    }
#endif

    Ov.hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (Ov.hEvent == nullptr) {
        printf("DDSBackEndBridge: failed to allocate event for overlapped operations\n");
        return DDS_ERROR_CODE_FAILED_CONNECTION;
    }
#ifdef BACKEND_BRIDGE_VERBOSE
    else {
        printf("DDSBackEndBridge: succeeded to allocate event for overlapped operations\n");
    }
#endif

    hr = Adapter->CreateOverlappedFile(&AdapterFileHandle);
    if (FAILED(hr))
    {
        printf("CreateOverlappedFile failed with %08x\n", hr);
        return DDS_ERROR_CODE_FAILED_CONNECTION;
    }
#ifdef BACKEND_BRIDGE_VERBOSE
    else {
        printf("CreateOverlappedFile succeeded\n");
    }
#endif

    AdapterInfo.InfoVersion = ND_VERSION_2;
    ULONG adapterInfoSize = sizeof(AdapterInfo);
    hr = Adapter->Query(&AdapterInfo, &adapterInfoSize);
    if (FAILED(hr)) {
        printf("IND2Adapter::GetAdapterInfo failed with %08x\n", hr);
        return DDS_ERROR_CODE_FAILED_CONNECTION;
    }
#ifdef BACKEND_BRIDGE_VERBOSE
    else {
        printf("IND2Adapter::GetAdapterInfo failed succeeded\n");
    }
#endif

    if ((AdapterInfo.AdapterFlags & ND_ADAPTER_FLAG_IN_ORDER_DMA_SUPPORTED) == 0) {
        printf("Adapter does not support in-order RDMA\n");
        return DDS_ERROR_CODE_FAILED_CONNECTION;
    }
#ifdef BACKEND_BRIDGE_VERBOSE
    else {
        printf("Adapter supports in-order RDMA\n");
    }
#endif

    QueueDepth = AdapterInfo.MaxCompletionQueueDepth;
    QueueDepth = min(QueueDepth, AdapterInfo.MaxInitiatorQueueDepth);
    QueueDepth = min(QueueDepth, AdapterInfo.MaxOutboundReadLimit);
    MaxSge = min(AdapterInfo.MaxInitiatorSge, AdapterInfo.MaxReadSge);
    InlineThreshold = AdapterInfo.InlineRequestThreshold;

    printf("DDSBackEndBridge: adapter information\n");
    printf("- Queue depth = %llu\n", QueueDepth);
    printf("- Max SGE = %llu\n", MaxSge);
    printf("- Inline threshold = %llu\n", InlineThreshold);

    //
    // Connect to the back end
    //
    //
    RDMC_OpenAdapter(&Adapter, &LocalSock, &AdapterFileHandle, &Ov);
#ifdef BACKEND_BRIDGE_VERBOSE
    printf("RDMC_OpenAdapter succeeded\n");
#endif

    RDMC_CreateConnector(Adapter, AdapterFileHandle, &CtrlConnector);
#ifdef BACKEND_BRIDGE_VERBOSE
    printf("RDMC_CreateConnector succeeded\n");
#endif

    RDMC_CreateCQ(Adapter, AdapterFileHandle, (DWORD)QueueDepth, &CtrlCompQ);
#ifdef BACKEND_BRIDGE_VERBOSE
    printf("RDMC_CreateCQ succeeded\n");
#endif

    RDMC_CreateQueuePair(Adapter, CtrlCompQ, (DWORD)QueueDepth, (DWORD)MaxSge, (DWORD)InlineThreshold, &CtrlQPair);
#ifdef BACKEND_BRIDGE_VERBOSE
    printf("RDMC_CreateQueuePair succeeded\n");
#endif

    RDMC_CreateMR(Adapter, AdapterFileHandle, &CtrlMemRegion);
#ifdef BACKEND_BRIDGE_VERBOSE
    printf("RDMC_CreateMR succeeded\n");
#endif

    unsigned long flags = ND_MR_FLAG_ALLOW_LOCAL_WRITE | ND_MR_FLAG_ALLOW_REMOTE_READ | ND_MR_FLAG_ALLOW_REMOTE_WRITE;
    RDMC_RegisterDataBuffer(CtrlMemRegion, CtrlMsgBuf, CTRL_MSG_SIZE, flags, &Ov);
#ifdef BACKEND_BRIDGE_VERBOSE
    printf("RDMC_RegisterDataBuffer succeeded\n");
#endif

    CtrlSgl = new ND2_SGE[1];
    CtrlSgl[0].Buffer = CtrlMsgBuf;
    CtrlSgl[0].BufferLength = CTRL_MSG_SIZE;
    CtrlSgl[0].MemoryRegionToken = CtrlMemRegion->GetLocalToken();

    uint8_t privData = CTRL_CONN_PRIV_DATA;
    RDMC_Connect(CtrlConnector, CtrlQPair, &Ov, LocalSock, BackEndSock, 0, (DWORD)QueueDepth, &privData, sizeof(privData));
#ifdef BACKEND_BRIDGE_VERBOSE
    printf("RDMC_Connect succeeded\n");
#endif

    RDMC_CompleteConnect(CtrlConnector, &Ov);
#ifdef BACKEND_BRIDGE_VERBOSE
    printf("RDMC_CompleteConnect succeeded\n");
#endif

    //
    // Request client id and wait for response
    //
    //
    ((MsgHeader*)CtrlMsgBuf)->MsgId = CTRL_MSG_F2B_REQUEST_ID;
    ((CtrlMsgF2BRequestId*)(CtrlMsgBuf + sizeof(MsgHeader)))->Dummy = 42;
    CtrlSgl->BufferLength = sizeof(MsgHeader) + sizeof(CtrlMsgF2BRequestId);
    RDMC_Send(CtrlQPair, CtrlSgl, 1, 0, MSG_CTXT);
#ifdef BACKEND_BRIDGE_VERBOSE
    printf("RDMC_Send succeeded\n");
#endif

    RDMC_WaitForCompletionAndCheckContext(CtrlCompQ, &Ov, MSG_CTXT, false);
#ifdef BACKEND_BRIDGE_VERBOSE
    printf("RDMC_WaitForCompletionAndCheckContext succeeded\n");
#endif

    CtrlSgl->BufferLength = CTRL_MSG_SIZE;
    RDMC_PostReceive(CtrlQPair, CtrlSgl, 1, MSG_CTXT);
#ifdef BACKEND_BRIDGE_VERBOSE
    printf("RDMC_PostReceive succeeded\n");
#endif

    RDMC_WaitForCompletionAndCheckContext(CtrlCompQ, &Ov, MSG_CTXT, false);
#ifdef BACKEND_BRIDGE_VERBOSE
    printf("RDMC_WaitForCompletionAndCheckContext succeeded\n");
#endif

    if (((MsgHeader*)CtrlMsgBuf)->MsgId == CTRL_MSG_B2F_RESPOND_ID) {
        ClientId = ((CtrlMsgB2FRespondId*)(CtrlMsgBuf + sizeof(MsgHeader)))->ClientId;
        printf("DDSBackEndBridge: connected to the back end with assigned Id (%d)\n", ClientId);
    }
    else {
        printf("DDSBackEndBridge: wrong message from the back end\n");
        return DDS_ERROR_CODE_UNEXPECTED_MSG;
    }

    return DDS_ERROR_CODE_SUCCESS;
}

//
// Disconnect from the backend
//
//
ErrorCodeT
DDSBackEndBridge::Disconnect() {
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
        printf("DDSBackEndBridge: disconnected from the back end\n");
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
}

//
// Send a control message and wait (w/ blocking) for response
//
//
static inline ErrorCodeT
SendCtrlMsgAndWait(
    DDSBackEndBridge* BackEnd,
    int ExpectedMsgId
) {
    ErrorCodeT result = DDS_ERROR_CODE_SUCCESS;

    RDMC_Send(BackEnd->CtrlQPair, BackEnd->CtrlSgl, 1, 0, MSG_CTXT);
    RDMC_WaitForCompletionAndCheckContext(BackEnd->CtrlCompQ, &BackEnd->Ov, MSG_CTXT, true);

    BackEnd->CtrlSgl->BufferLength = CTRL_MSG_SIZE;
    RDMC_PostReceive(BackEnd->CtrlQPair, BackEnd->CtrlSgl, 1, MSG_CTXT);
    RDMC_WaitForCompletionAndCheckContext(BackEnd->CtrlCompQ, &BackEnd->Ov, MSG_CTXT, true);

    if (((MsgHeader*)BackEnd->CtrlMsgBuf)->MsgId != ExpectedMsgId) {
        result = DDS_ERROR_CODE_UNEXPECTED_MSG;
    }

    return result;
}

//
// Create a diretory
// 
//
ErrorCodeT
DDSBackEndBridge::CreateDirectory(
    const char* PathName,
    DirIdT DirId,
    DirIdT ParentId
) {
    ErrorCodeT result = DDS_ERROR_CODE_SUCCESS;

    //
    // Send a create directory request to the back end
    //
    //
    ((MsgHeader*)CtrlMsgBuf)->MsgId = CTRL_MSG_F2B_REQ_CREATE_DIR;

    CtrlMsgF2BReqCreateDirectory* req = (CtrlMsgF2BReqCreateDirectory*)(CtrlMsgBuf + sizeof(MsgHeader));
    req->DirId = DirId;
    req->ParentDirId = ParentId;
    strcpy(req->PathName, PathName);
    
    CtrlSgl->BufferLength = sizeof(MsgHeader) + sizeof(CtrlMsgF2BReqCreateDirectory);
    
    result = SendCtrlMsgAndWait(this, CTRL_MSG_B2F_ACK_CREATE_DIR);
    if (result != DDS_ERROR_CODE_SUCCESS) {
        return result;
    }

    CtrlMsgB2FAckCreateDirectory* resp = (CtrlMsgB2FAckCreateDirectory*)(CtrlMsgBuf + sizeof(MsgHeader));
    return resp->Result;
}

//
// Delete a directory
// 
//
ErrorCodeT
DDSBackEndBridge::RemoveDirectory(
    DirIdT DirId
) {
    ErrorCodeT result = DDS_ERROR_CODE_SUCCESS;

    //
    // Send a remove directory request to the back end
    //
    //
    ((MsgHeader*)CtrlMsgBuf)->MsgId = CTRL_MSG_F2B_REQ_REMOVE_DIR;

    CtrlMsgF2BReqRemoveDirectory* req = (CtrlMsgF2BReqRemoveDirectory*)(CtrlMsgBuf + sizeof(MsgHeader));
    req->DirId = DirId;
    
    CtrlSgl->BufferLength = sizeof(MsgHeader) + sizeof(CtrlMsgF2BReqRemoveDirectory);
    
    result = SendCtrlMsgAndWait(this, CTRL_MSG_B2F_ACK_REMOVE_DIR);
    if (result != DDS_ERROR_CODE_SUCCESS) {
        return result;
    }

    CtrlMsgB2FAckRemoveDirectory* resp = (CtrlMsgB2FAckRemoveDirectory*)(CtrlMsgBuf + sizeof(MsgHeader));
    return resp->Result;
}

//
// Create a file
// 
//
ErrorCodeT
DDSBackEndBridge::CreateFile(
    const char* FileName,
    FileAttributesT FileAttributes,
    FileIdT FileId,
    DirIdT DirId
) {
    ErrorCodeT result = DDS_ERROR_CODE_SUCCESS;

    //
    // Send a create tile request to the back end
    //
    //
    ((MsgHeader*)CtrlMsgBuf)->MsgId = CTRL_MSG_F2B_REQ_CREATE_FILE;

    CtrlMsgF2BReqCreateFile* req = (CtrlMsgF2BReqCreateFile*)(CtrlMsgBuf + sizeof(MsgHeader));
    req->FileId = FileId;
    req->FileAttributes = FileAttributes;
    req->DirId = DirId;
    strcpy(req->FileName, FileName);
    
    CtrlSgl->BufferLength = sizeof(MsgHeader) + sizeof(CtrlMsgF2BReqCreateFile);
    
    result = SendCtrlMsgAndWait(this, CTRL_MSG_B2F_ACK_CREATE_FILE);
    if (result != DDS_ERROR_CODE_SUCCESS) {
        return result;
    }

    CtrlMsgB2FAckCreateFile* resp = (CtrlMsgB2FAckCreateFile*)(CtrlMsgBuf + sizeof(MsgHeader));
    return resp->Result;
}

//
// Delete a file
// 
//
ErrorCodeT
DDSBackEndBridge::DeleteFile(
    FileIdT FileId,
    DirIdT DirId
) {
    ErrorCodeT result = DDS_ERROR_CODE_SUCCESS;

    //
    // Send a delete file request to the back end
    //
    //
    ((MsgHeader*)CtrlMsgBuf)->MsgId = CTRL_MSG_F2B_REQ_DELETE_FILE;

    CtrlMsgF2BReqDeleteFile* req = (CtrlMsgF2BReqDeleteFile*)(CtrlMsgBuf + sizeof(MsgHeader));
    req->DirId = DirId;
    req->FileId = FileId;
    
    CtrlSgl->BufferLength = sizeof(MsgHeader) + sizeof(CtrlMsgF2BReqDeleteFile);
    
    result = SendCtrlMsgAndWait(this, CTRL_MSG_B2F_ACK_DELETE_FILE);
    if (result != DDS_ERROR_CODE_SUCCESS) {
        return result;
    }

    CtrlMsgB2FAckDeleteFile* resp = (CtrlMsgB2FAckDeleteFile*)(CtrlMsgBuf + sizeof(MsgHeader));
    return resp->Result;
}

//
// Change the size of a file
// 
//
ErrorCodeT
DDSBackEndBridge::ChangeFileSize(
    FileIdT FileId,
    FileSizeT NewSize
) {
    ErrorCodeT result = DDS_ERROR_CODE_SUCCESS;

    //
    // Send a change file size request to the back end
    //
    //
    ((MsgHeader*)CtrlMsgBuf)->MsgId = CTRL_MSG_F2B_REQ_CHANGE_FILE_SIZE;

    CtrlMsgF2BReqChangeFileSize* req = (CtrlMsgF2BReqChangeFileSize*)(CtrlMsgBuf + sizeof(MsgHeader));
    req->FileId = FileId;
    req->NewSize = NewSize;
    
    CtrlSgl->BufferLength = sizeof(MsgHeader) + sizeof(CtrlMsgF2BReqChangeFileSize);
    
    result = SendCtrlMsgAndWait(this, CTRL_MSG_B2F_ACK_CHANGE_FILE_SIZE);
    if (result != DDS_ERROR_CODE_SUCCESS) {
        return result;
    }

    CtrlMsgB2FAckChangeFileSize* resp = (CtrlMsgB2FAckChangeFileSize*)(CtrlMsgBuf + sizeof(MsgHeader));
    return resp->Result;
}

//
// Get file size
// 
//
ErrorCodeT
DDSBackEndBridge::GetFileSize(
    FileIdT FileId,
    FileSizeT* FileSize
) {
    ErrorCodeT result = DDS_ERROR_CODE_SUCCESS;

    //
    // Send a get file size request to the back end
    //
    //
    ((MsgHeader*)CtrlMsgBuf)->MsgId = CTRL_MSG_F2B_REQ_GET_FILE_SIZE;

    CtrlMsgF2BReqGetFileSize* req = (CtrlMsgF2BReqGetFileSize*)(CtrlMsgBuf + sizeof(MsgHeader));
    req->FileId = FileId;
    
    CtrlSgl->BufferLength = sizeof(MsgHeader) + sizeof(CtrlMsgF2BReqGetFileSize);
    
    result = SendCtrlMsgAndWait(this, CTRL_MSG_B2F_ACK_GET_FILE_SIZE);
    if (result != DDS_ERROR_CODE_SUCCESS) {
        return result;
    }

    CtrlMsgB2FAckGetFileSize* resp = (CtrlMsgB2FAckGetFileSize*)(CtrlMsgBuf + sizeof(MsgHeader));
    *FileSize = resp->FileSize;
    return resp->Result;
}

//
// Async read from a file
// 
//
ErrorCodeT
DDSBackEndBridge::ReadFile(
    FileIdT FileId,
    FileSizeT Offset,
    BufferT DestBuffer,
    FileIOSizeT BytesToRead,
    FileIOSizeT* BytesRead,
    ContextT Context,
    PollT* Poll
) {
    return DDS_ERROR_CODE_NOT_IMPLEMENTED;
}

//
// Async read from a file with scattering
// 
//
ErrorCodeT
DDSBackEndBridge::ReadFileScatter(
    FileIdT FileId,
    FileSizeT Offset,
    BufferT* DestBufferArray,
    FileIOSizeT BytesToRead,
    FileIOSizeT* BytesRead,
    ContextT Context,
    PollT* Poll
) {
    return DDS_ERROR_CODE_NOT_IMPLEMENTED;
}

//
// Async write to a file
// 
//
ErrorCodeT
DDSBackEndBridge::WriteFile(
    FileIdT FileId,
    FileSizeT Offset,
    BufferT SourceBuffer,
    FileIOSizeT BytesToWrite,
    FileIOSizeT* BytesWritten,
    ContextT Context,
    PollT* Poll
) {
    return DDS_ERROR_CODE_NOT_IMPLEMENTED;
}

//
// Async write to a file with gathering
// 
//
ErrorCodeT
DDSBackEndBridge::WriteFileGather(
    FileIdT FileId,
    FileSizeT Offset,
    BufferT* SourceBufferArray,
    FileIOSizeT BytesToWrite,
    FileIOSizeT* BytesWritten,
    ContextT Context,
    PollT* Poll
) {
    return DDS_ERROR_CODE_NOT_IMPLEMENTED;
}

//
// Get file properties by file Id
// 
//
ErrorCodeT
DDSBackEndBridge::GetFileInformationById(
    FileIdT FileId,
    FilePropertiesT* FileProperties
) {
    ErrorCodeT result = DDS_ERROR_CODE_SUCCESS;

    //
    // Send a get file information request to the back end
    //
    //
    ((MsgHeader*)CtrlMsgBuf)->MsgId = CTRL_MSG_F2B_REQ_GET_FILE_INFO;

    CtrlMsgF2BReqGetFileInfo* req = (CtrlMsgF2BReqGetFileInfo*)(CtrlMsgBuf + sizeof(MsgHeader));
    req->FileId = FileId;
    
    CtrlSgl->BufferLength = sizeof(MsgHeader) + sizeof(CtrlMsgF2BReqGetFileInfo);
    
    result = SendCtrlMsgAndWait(this, CTRL_MSG_B2F_ACK_GET_FILE_INFO);
    if (result != DDS_ERROR_CODE_SUCCESS) {
        return result;
    }

    CtrlMsgB2FAckGetFileInfo* resp = (CtrlMsgB2FAckGetFileInfo*)(CtrlMsgBuf + sizeof(MsgHeader));
    memcpy(FileProperties, &resp->FileInfo, sizeof(FilePropertiesT));
    return resp->Result;
}

//
// Get file attributes by file name
// 
//
ErrorCodeT
DDSBackEndBridge::GetFileAttributes(
    FileIdT FileId,
    FileAttributesT* FileAttributes
) {
    ErrorCodeT result = DDS_ERROR_CODE_SUCCESS;

    //
    // Send a get file attributes request to the back end
    //
    //
    ((MsgHeader*)CtrlMsgBuf)->MsgId = CTRL_MSG_F2B_REQ_GET_FILE_ATTR;

    CtrlMsgF2BReqGetFileAttr* req = (CtrlMsgF2BReqGetFileAttr*)(CtrlMsgBuf + sizeof(MsgHeader));
    req->FileId = FileId;
    
    CtrlSgl->BufferLength = sizeof(MsgHeader) + sizeof(CtrlMsgF2BReqGetFileAttr);
    
    result = SendCtrlMsgAndWait(this, CTRL_MSG_B2F_ACK_GET_FILE_ATTR);
    if (result != DDS_ERROR_CODE_SUCCESS) {
        return result;
    }

    CtrlMsgB2FAckGetFileAttr* resp = (CtrlMsgB2FAckGetFileAttr*)(CtrlMsgBuf + sizeof(MsgHeader));
    *FileAttributes = resp->FileAttr;
    return resp->Result;
}

//
// Get the size of free space on the storage
// 
//
ErrorCodeT
DDSBackEndBridge::GetStorageFreeSpace(
    FileSizeT* StorageFreeSpace
) {
    ErrorCodeT result = DDS_ERROR_CODE_SUCCESS;

    //
    // Send a get free space request to the back end
    //
    //
    ((MsgHeader*)CtrlMsgBuf)->MsgId = CTRL_MSG_F2B_REQ_GET_FREE_SPACE;

    CtrlMsgF2BReqGetFreeSpace* req = (CtrlMsgF2BReqGetFreeSpace*)(CtrlMsgBuf + sizeof(MsgHeader));
    req->Dummy = 42;
    
    CtrlSgl->BufferLength = sizeof(MsgHeader) + sizeof(CtrlMsgF2BReqGetFreeSpace);
    
    result = SendCtrlMsgAndWait(this, CTRL_MSG_B2F_ACK_GET_FREE_SPACE);
    if (result != DDS_ERROR_CODE_SUCCESS) {
        return result;
    }

    CtrlMsgB2FAckGetFreeSpace* resp = (CtrlMsgB2FAckGetFreeSpace*)(CtrlMsgBuf + sizeof(MsgHeader));
    *StorageFreeSpace = resp->FreeSpace;
    return resp->Result;
}

//
// Move an existing file or a directory,
// including its children
// 
//
ErrorCodeT
DDSBackEndBridge::MoveFile(
    FileIdT FileId,
    const char* NewFileName
) {
    ErrorCodeT result = DDS_ERROR_CODE_SUCCESS;

    //
    // Send a move file request to the back end
    //
    //
    ((MsgHeader*)CtrlMsgBuf)->MsgId = CTRL_MSG_F2B_REQ_MOVE_FILE;

    CtrlMsgF2BReqMoveFile* req = (CtrlMsgF2BReqMoveFile*)(CtrlMsgBuf + sizeof(MsgHeader));
    req->FileId = FileId;
    strcpy(req->NewFileName, NewFileName);
    
    CtrlSgl->BufferLength = sizeof(MsgHeader) + sizeof(CtrlMsgF2BReqMoveFile);
    
    result = SendCtrlMsgAndWait(this, CTRL_MSG_B2F_ACK_GET_FREE_SPACE);
    if (result != DDS_ERROR_CODE_SUCCESS) {
        return result;
    }

    CtrlMsgB2FAckMoveFile* resp = (CtrlMsgB2FAckMoveFile*)(CtrlMsgBuf + sizeof(MsgHeader));
    return resp->Result;
}

}