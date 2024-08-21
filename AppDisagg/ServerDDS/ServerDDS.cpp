#include "ServerDDS.h"

#pragma comment(lib,"ws2_32.lib")

using namespace std;

using std::cout;
using std::endl;
using std::thread;
using std::this_thread::yield;

// msgSize is HeaderSize * BatchSize
void HandleFirstMsg(SOCKET clientSocket, int* msgSize) {
    int iResult = 0;
    iResult = recv(clientSocket, (char*)msgSize, sizeof(int), 0);
    if (iResult == SOCKET_ERROR || iResult != sizeof(int)) {
        cout << "Error receiving the first message: " << WSAGetLastError() << endl;
        closesocket(clientSocket);
        WSACleanup();
        return;
    }

}


void ThreadFunc(SOCKET clientSocket, DDS_FrontEnd::DDSFrontEnd *Store, int payloadSize, int batchSize, int queueDepth, int clientIndex) {
    // Initialize context and preallocate memory
    // total no. of outstanding requests is batch size * queue depth
    
    ////for read
    // int responseSize = sizeof(MessageHeader) + payloadSize;
    ////for write
    int responseSize = sizeof(MessageHeader);

    int batchRespSize = responseSize * batchSize;

    char* bufferMem = (char*)_aligned_malloc(batchSize * queueDepth * responseSize, 4096);
    memset(bufferMem, 0, batchSize * queueDepth * responseSize);
    ServerContext* serverContexts = (ServerContext*)malloc(batchSize * queueDepth * sizeof(ServerContext));
    memset(serverContexts, 0, batchSize * queueDepth * sizeof(ServerContext));
    atomic_uint16_t* batchCompletionCounts = new atomic_uint16_t[queueDepth];

    // send on the same socket mustn't be concurrent, per windows doc, so needs a lock
    mutex* socketMutex = new mutex();

    int iResult = -1;
    ErrorCodeT ret = -1;

    // cout << "Thread for client " << clientIndex << " Waiting for the first message..." << endl;

    // Receive the first message
    int msgSize = 0;
    HandleFirstMsg(clientSocket, &msgSize);
    ////for read
    // int mbatchSize = msgSize / sizeof(MessageHeader);
    ////for write
    int mbatchSize = msgSize / (sizeof(MessageHeader) + payloadSize);

    if (mbatchSize != batchSize) {
        cerr << "ERROR: batch size MISMATCH on server and client" << endl;
    }
    else {
        // cout << "got batch size from client: " << mbatchSize << endl;
    }

    char* recvBuffer = new char[msgSize];
    memset(recvBuffer, 0, msgSize);

    int oneReceive = 0;
    int remainingBytesToReceive = msgSize;

    int oneSend = 0;
    int remainingBytesToSend = batchRespSize;

    bool finished = false;
    uint64_t recvCount = 0;
    while (!finished) {
        // Receive message from client and send response

        while (oneReceive < msgSize) {
            iResult = recv(clientSocket, recvBuffer + oneReceive, remainingBytesToReceive, 0);
            recvCount++;

            if (iResult == SOCKET_ERROR) {
                if ((int64_t)msgProcessedCount[clientIndex] != readNum) {
                    doWork.store(false);
                    cout << "Error receiving message: " << WSAGetLastError() << endl;
                    cout << "msgProcessedCount: " << (int64_t)msgProcessedCount[clientIndex] << endl;
                }
                else {
                    cout << "recv from client but client is finished and closed (conn rest)" << endl;
                }
                finished = true;
                break;
            }
            else if (iResult == 0) {
                finished = true;
                cout << "client closed socket" << endl;
                break;
            }

            oneReceive += iResult;
            remainingBytesToReceive = msgSize - oneReceive;
        }



        if (finished) {
            break;
        }

        oneReceive = 0;
        remainingBytesToReceive = msgSize;


        // issue file read
        MessageHeader* requestMsg;
        for (size_t i = 0; i < batchSize; i++) {
            requestMsg = (MessageHeader*)(recvBuffer + i * (sizeof(MessageHeader) + payloadSize));
            
            if (requestMsg->BatchId > batchSize) {
                cout << "got wrong batch id from clinet: " << requestMsg->BatchId << endl;
            }
            int index = (int)(requestMsg->BatchId * batchSize + i);
            char* respBuffer = bufferMem + index * responseSize;
            memcpy(respBuffer, requestMsg, sizeof(MessageHeader));
            
            ServerContext* respCtx = serverContexts + index;
            respCtx->clientIndex = clientIndex;
            respCtx->ReadStartTime = high_resolution_clock::now().time_since_epoch().count();
            respCtx->hSocket = clientSocket;
            respCtx->socketMutex = socketMutex;
            respCtx->BatchId = requestMsg->BatchId;
            respCtx->bufferMem = bufferMem;
            respCtx->serverContexts = serverContexts;
            respCtx->batchCompletionCounts = batchCompletionCounts;
            
            ////for read
            // ret = Store->ReadFile(requestMsg->FileId, respBuffer, requestMsg->Offset, payloadSize, &respCtx->BytesRead, NULL, respCtx);
            ////for write
            ret = Store->WriteFile(requestMsg->FileId, recvBuffer + i * (sizeof(MessageHeader) + payloadSize) + sizeof(MessageHeader),
                requestMsg->Offset, payloadSize, &respCtx->BytesWritten, NULL, respCtx);


            while (ret == DDS_ERROR_CODE_TOO_MANY_REQUESTS || ret == DDS_ERROR_CODE_REQUEST_RING_FAILURE) {
                // just retry issuing the write
                ret = Store->WriteFile(requestMsg->FileId, recvBuffer + i * (sizeof(MessageHeader) + payloadSize) + sizeof(MessageHeader),
                    requestMsg->Offset, payloadSize, &respCtx->BytesWritten, NULL, respCtx);
            }
        }
    }

    Statistics stats;
    Percentiles PercentileStats;
    GetStatistics(readLatencies[clientIndex], readNum, &stats, &PercentileStats);
    printf(
        "readLatencies, Thread %d:  Min: %.2lf, Max: %.2lf, 50th: %.2lf, 90th: %.2lf, 99th: %.2lf, 99.9th: %.2lf, 99.99th: %.2lf, StdErr: %.2lf\n",
        clientIndex,
        stats.Min,
        stats.Max,
        PercentileStats.P50,
        PercentileStats.P90,
        PercentileStats.P99,
        PercentileStats.P99p9,
        PercentileStats.P99p99,
        stats.StandardError);
    allClientsReadLatencyStats[clientIndex] = stats;
    allClientsReadLatencyPercentiles[clientIndex] = PercentileStats;

    GetStatistics(sendBackLatencies[clientIndex], readNum / batchSize, &stats, &PercentileStats);
    printf(
        "send back times, Thread %d:  Min: %.2lf, Max: %.2lf, 50th: %.2lf, 90th: %.2lf, 99th: %.2lf, 99.9th: %.2lf, 99.99th: %.2lf, StdErr: %.2lf\n",
        clientIndex,
        stats.Min,
        stats.Max,
        PercentileStats.P50,
        PercentileStats.P90,
        PercentileStats.P99,
        PercentileStats.P99p9,
        PercentileStats.P99p99,
        stats.StandardError);
    allClientsSendBackStats[clientIndex] = stats;
    allClientsSendBackPercentiles[clientIndex] = PercentileStats;

    // Clear buffer
    delete[] recvBuffer;

    // Close the socket for this client
    ret = closesocket(clientSocket);
    if (ret) {
        cout << "closing socket err: " << WSAGetLastError() << endl;;
    }
}

// 1 thread will run this routine to PollWait the DDS front end IOs
// the ctx from poll wait will be sent as the completion key (a ptr)
void PollWaitRoutine(DDS_FrontEnd::DDSFrontEnd* Store) {
    PollIdT pollId;
    ErrorCodeT ret = Store->GetDefaultPoll(&pollId);
    FileIOSizeT bytesServiced;
    ContextT ctx = nullptr;
    ContextT pollContext = nullptr;  // should not be used
    bool pollResult;

    cout << "Pollwait routine running..." << endl;
    while (doWork) {
        ret = Store->PollWait(pollId, &bytesServiced, &pollContext, &ctx, INFINITE, &pollResult);
       
        ret = PostQueuedCompletionStatus(ioCompletionPort, bytesServiced, (ULONG_PTR)ctx, NULL);
        if (!ret) {
            DWORD err = GetLastError();
            if (err == 6) {
                cout << "completion port closed" << err << endl;
                break;
            }
            else {
                cout << "Error posting to completion port: " << err << endl;
            }
        }
    }
}

// pollers will run this, the context to get is in the completion key, not overlapped
void HandleCompletions(int payloadSize, int batchSize, int readNum) {
    
    volatile ErrorCodeT iResult2 = 0;
    int oneSend = 0;

    ////for read
    // int batchRespSize = (sizeof(MessageHeader) + payloadSize) * batchSize;
    ////for write
    int batchRespSize = (sizeof(MessageHeader)) * batchSize;
    int remainingBytesToSend = batchRespSize;

    DWORD bytesTransferred = 0;
    LPOVERLAPPED overlapped = NULL;  // unused
    uint64_t completionKey = NULL;  // ptr to dds ctx

    while (doWork) {
        BOOL ret = GetQueuedCompletionStatus(ioCompletionPort, &bytesTransferred, &completionKey, &overlapped, INFINITE);
        if (!ret) {
            DWORD err = GetLastError();
            if (err == 735 || err == 6) { // completion port closed
                break;
            }
            // this shouldn't happen
            cout << "Error, but ignoring, in GetQueuedCompletionStatus: " << err << endl;
        }
        
        ServerContext* ctx = (ServerContext*) completionKey;

        // only send ALL responses if whole batch finished
        // ctx->batchCompletionCounts[ctx->BatchId] += 1; // racy!!
        uint16_t completionCount = ctx->batchCompletionCounts[ctx->BatchId].fetch_add(1);
        atomic_uint64_t processedCount = msgProcessedCount[ctx->clientIndex].fetch_add(1);
        readLatencies[ctx->clientIndex][processedCount] =
            (double)(high_resolution_clock::now().time_since_epoch().count() - ctx->ReadStartTime) / 1000;

        if (completionCount == batchSize - 1) {
            int bytesSent = 0;
            ctx->batchCompletionCounts[ctx->BatchId].store(0);  // reset
            char* buf = ctx->bufferMem + (ctx->BatchId * batchRespSize);

            auto sendStart = high_resolution_clock::now().time_since_epoch().count();
            ctx->socketMutex->lock();  // can't have concurrent sends, other completion threads may also send on same socket
            
            while (oneSend < batchRespSize) {
                bytesSent = send(ctx->hSocket, buf + oneSend, remainingBytesToSend, 0);
        
                if (bytesSent == SOCKET_ERROR) {
                    ctx->socketMutex->unlock();
                    cerr << "Error sending response: " << WSAGetLastError() << endl;
                    closesocket(ctx->hSocket);
                    WSACleanup();
                    return;
                }
                oneSend += bytesSent;
                remainingBytesToSend = batchRespSize - oneSend;
            }
            ctx->socketMutex->unlock();
            uint64_t sendC = sendCount.fetch_add(1);
            sendBackLatencies[ctx->clientIndex][processedCount / batchSize] =
                (double)(high_resolution_clock::now().time_since_epoch().count() - sendStart) / 1000;

            oneSend = 0;
            remainingBytesToSend = batchRespSize;
        }
    }
}

int RunServer(int payloadSize, int batchSize, int queueDepth, int readNum, int fileSize, int nFile, int nConnections, int nCompletionThreads) {
    cout << "server starting..." << endl;
    allClientThreads = new thread * [nConnections];

    // alloc for stats
    allClientsReadLatencyStats = new Statistics[nConnections];
    allClientsReadLatencyPercentiles = new Percentiles[nConnections];
    allClientsSendBackStats = new Statistics[nConnections];
    allClientsSendBackPercentiles = new Percentiles[nConnections];

    msgProcessedCount = new atomic_uint64_t[nConnections];
    readLatencies = new double* [nConnections];
    sendBackLatencies = new double* [nConnections];
    int batchNum = readNum / batchSize;
    for (int i = 0; i < nConnections; i++) {
        msgProcessedCount[i] = 0;
        readLatencies[i] = new double[readNum];
        memset(readLatencies[i], 0, sizeof(double) * readNum);
        sendBackLatencies[i] = new double[batchNum];
        memset(sendBackLatencies[i], 0, sizeof(double) * batchNum);
    }


    // open DDS files for read
    const char* storeName = "DDS-Store0";
    const char* rootDirName = "/data";
    const char* fileName = "/data/example";
    const FileAccessT fileAccess = 0;
    const FileShareModeT shareMode = 0;
    const FileAttributesT fileAttributes = 0;
    DirIdT rootDirId = DDS_DIR_INVALID;
    FileIdT fileId = DDS_FILE_INVALID;

    DDS_FrontEnd::DDSFrontEnd *store = new DDS_FrontEnd::DDSFrontEnd(storeName);
    ErrorCodeT result = store->Initialize();
    if (result != DDS_ERROR_CODE_SUCCESS) {
        cout << "Failed to initialize DDS front end" << std::endl;
    }
    else {
        cout << "DDS front end initialized " << std::endl;
    }

    result = store->CreateDirectory(rootDirName, &rootDirId);
    if (result != DDS_ERROR_CODE_SUCCESS) {
        cout << "Failed to create directory" << endl;
    }
    else {
        cout << "Directory created: " << rootDirId << endl;
    }
    result = store->CreateFile(
        fileName,
        fileAccess,
        shareMode,
        fileAttributes,
        &fileId
    );
    if (result != DDS_ERROR_CODE_SUCCESS) {
        cout << "Failed to create file" << endl;
    }
    else {
        cout << "File created: " << fileId << endl;
    }
    fileIds = new FileIdT[nFile];
    // only 1 file for now
    fileIds[0] = fileId;

    store->ChangeFileSize(fileId, (FileSizeT)fileSize * 1024 * 1024 * 1024);
    if (result != DDS_ERROR_CODE_SUCCESS) {
        cout << "Failed to change size of file" << endl;
    }
    else {
        cout << "File size changed to "<< fileSize << "G" << endl;
    }

    // setup io completion port
    ioCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, nCompletionThreads);
    if (ioCompletionPort == NULL) {
        cout << "Error creating completion port: " << GetLastError() << endl;
        return -1;
    }
    cout << "IO completion port created" << endl;


    // setup 1 thread for constantly PollWait-ing, then post to completion port
    pollWaitThread = new thread([store]
        {
            PollWaitRoutine(store);
        });

    // setup threads for polling completions (on completion port)
    for (size_t i = 0; i < nCompletionThreads; i++) {
        thread* poller = new thread([payloadSize, batchSize, readNum]
            {
                HandleCompletions(payloadSize, batchSize, readNum);
            });
    }

    // Initialize Winsock
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        cout << "WSAStartup failed with error: " << iResult << endl;
        return 1;
    }

    // Create a socket for the server
    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) {
        cout << "Error creating socket: " << WSAGetLastError() << endl;
        WSACleanup();
        return 1;
    }

    // Set up the address of the server
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    // Bind the socket to the address
    iResult = ::bind(listenSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr));
    if (iResult == SOCKET_ERROR) {
        cout << "Error binding socket: " << WSAGetLastError() << endl;
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    int clientIndex = 0;
    while (clientIndex != nConnections) {
        cout << "Waiting for clients..." << endl;

        // Listen for incoming connections
        iResult = listen(listenSocket, SOMAXCONN);
        if (iResult == SOCKET_ERROR) {
            cout << "Error listening for connections: " << WSAGetLastError() << endl;
            closesocket(listenSocket);
            WSACleanup();
            return 1;
        }

        // Accept incoming connections and handle them
        sockaddr_in clientAddr;
        int clientAddrSize = sizeof(clientAddr);

        // Accept connection
        SOCKET clientSocket = accept(listenSocket, (SOCKADDR*)&clientAddr, &clientAddrSize);

        if (clientSocket == INVALID_SOCKET) {
            cout << "Error accepting connection: " << WSAGetLastError() << endl;
            closesocket(listenSocket);
            WSACleanup();
            return 1;
        }
        cout << "accepted client " << clientIndex << endl;

        thread* worker = new thread([clientSocket, store, payloadSize, batchSize, queueDepth, clientIndex]
            {
                ThreadFunc(clientSocket, store, payloadSize, batchSize, queueDepth, clientIndex);
            });
        allClientThreads[clientIndex] = worker;
        clientIndex++;
    }
    cout << "all clients connected, waiting for them to finish" << endl;
    int i = 0;
    for (i = 0; i < nConnections; i++) {
        allClientThreads[i]->join();
    }
    cout << "all clients finished" << endl;

    double statsMean = 0, p50Mean = 0, p99p9Mean = 0;
    for (i = 0; i < nConnections; i++) {
        statsMean += allClientsReadLatencyStats[i].Mean;
        p50Mean += allClientsReadLatencyPercentiles[i].P50;
        p99p9Mean += allClientsReadLatencyPercentiles[i].P99p9;
    }
    statsMean /= nConnections;
    p50Mean /= nConnections;
    p99p9Mean /= nConnections;

    cout << "ReadLatency (avg): mean: " << statsMean << ", p50: " << p50Mean << ", p99.9: " << p99p9Mean << endl;
    statsMean = 0; p50Mean = 0; p99p9Mean = 0;

    for (i = 0; i < nConnections; i++) {
        statsMean += allClientsSendBackStats[i].Mean;
        p50Mean += allClientsSendBackPercentiles[i].P50;
        p99p9Mean += allClientsSendBackPercentiles[i].P99p9;
    }
    statsMean /= nConnections;
    p50Mean /= nConnections;
    p99p9Mean /= nConnections;

    cout << "SendBack time (avg): mean: " << statsMean << ", p50: " << p50Mean << ", p99.9: " << p99p9Mean << endl;

    // Disconnect from DPU
    delete store;

    // Close the listen socket and cleanup
    int ret = closesocket(listenSocket);
    if (ret) {
        cout << "closing listen socket err: " << WSAGetLastError() << endl;;
    }

    cout << "closing listen socket completed" << endl;

    // close completion port
    ret = CloseHandle(ioCompletionPort);
    if (ret == 0) {
        cerr << "closing completion port failed" << endl;
    }
    cout << "closed completion port" << endl;

    doWork.store(false);
    cout << "closed front end" << endl;

    WSACleanup();

    return 0;
}

int main(
    int argc,
    const char** args
)
{
    if (argc == 9) {
        int readSize = stoi(args[1]);
        int batchSize = stoi(args[2]);
        int queueDepth = stoi(args[3]);
        readNum = stoi(args[4]);
        int fileSize = stoi(args[5]);
        int nFile = stoi(args[6]);
        int nConnections = stoi(args[7]);
        int nCompletionThreads = stoi(args[8]);
        return RunServer(readSize, batchSize, queueDepth, readNum, fileSize, nFile, nConnections, nCompletionThreads);
    }
    else {
        cout << "Server usage: readSize batchSize queueDepth readNum fileSize(GB) nFile nConnections nCompletionThreads" << endl;
    }

    return 0;
}

