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
// Create a diretory
// 
//
ErrorCodeT
DDSBackEndBridge::CreateDirectory(
    const char* PathName,
    DirIdT DirId,
    DirIdT ParentId
) {
    return DDS_ERROR_CODE_NOT_IMPLEMENTED;
}

//
// Delete a directory
// 
//
ErrorCodeT
DDSBackEndBridge::RemoveDirectory(
    DirIdT DirId
) {
    return DDS_ERROR_CODE_NOT_IMPLEMENTED;
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
    return DDS_ERROR_CODE_NOT_IMPLEMENTED;
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
    return DDS_ERROR_CODE_NOT_IMPLEMENTED;
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
    return DDS_ERROR_CODE_NOT_IMPLEMENTED;
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
    return DDS_ERROR_CODE_NOT_IMPLEMENTED;
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
    BackEndReadWriteCallback Callback,
    ContextT Context
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
    BackEndReadWriteCallback Callback,
    ContextT Context
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
    BackEndReadWriteCallback Callback,
    ContextT Context
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
    BackEndReadWriteCallback Callback,
    ContextT Context
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
    return DDS_ERROR_CODE_NOT_IMPLEMENTED;
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
    return DDS_ERROR_CODE_NOT_IMPLEMENTED;
}

//
// Get the size of free space on the storage
// 
//
ErrorCodeT
DDSBackEndBridge::GetStorageFreeSpace(
    FileSizeT* StorageFreeSpace
) {
    return DDS_ERROR_CODE_NOT_IMPLEMENTED;
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
    return DDS_ERROR_CODE_NOT_IMPLEMENTED;
}

}