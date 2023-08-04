#pragma once

#include "MsgType.h"
#include "Protocol.h"
#include "RDMC.h"

#define BACKEND_TYPE_IN_MEMORY 1
#define BACKEND_TYPE_DMA 2
#define BACKEND_TYPE BACKEND_TYPE_DMA

#define BACKEND_BRIDGE_VERBOSE

//
// Connector that fowards requests to and receives responses from the back end
//
//
class BackEndBridge {
public:
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

public:
    BackEndBridge();

    //
    // Connect to the backend
    //
    //
    bool
    Connect();

    bool
    ConnectTest();

    //
    // Disconnect from the backend
    //
    //
    void
    Disconnect();
};
