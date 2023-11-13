#include <winsock2.h>
#include <iostream>
#include <thread>
#include <vector>
#include <cstdlib>
#include <string>

using namespace std;

//#include <windows.h>

#include "../Common/Include/Config.h"
#include "Server.h"

#pragma comment(lib,"ws2_32.lib")

using std::cout;
using std::endl;


// 2 groups of thread, 1 for fileIO, 1 for tcp conn
//create files
//create completion ports
//fileIO group listen to ports
//register files in ports
//spawn mul threads
//for every thread, keeps receving request
//once recieved request issue file read
//wait for next connecttion
//read
//insert in to ports


// msgSize is HeaderSize * BatchSize
void HandleFirstMsg(SOCKET clientSocket, int *msgSize) {
    int iResult = 0;
    iResult = recv(clientSocket, (char*)msgSize, sizeof(int), 0);
    if (iResult == SOCKET_ERROR || iResult != sizeof(int)) {
        cout << "Error receiving the first message: " << WSAGetLastError() << endl;
        closesocket(clientSocket);
        WSACleanup();
        return;
    }

}


void ThreadFunc(SOCKET clientSocket, int payloadSize, int batchSize, int queueDepth, int clientIndex) {
    // Initialize context and preallocate memory
    // total no. of outstanding requests is batch size * queue depth
    int responseSize = sizeof(MessageHeader) + payloadSize;
    int batchRespSize = responseSize * batchSize;

    char *bufferMem = (char *) _aligned_malloc(batchSize * queueDepth * responseSize, 4096);
    ServerContext *serverContexts = (ServerContext *) malloc(batchSize * queueDepth * sizeof(ServerContext));
    atomic_uint16_t *batchCompletionCounts = new atomic_uint16_t[queueDepth];

    // send on the same socket mustn't be concurrent, per windows doc, so needs a lock
    mutex *socketMutex = new mutex();

    int iResult = -1;

    cout << "Thread for client " << clientIndex << " Waiting for the first message..." << endl;

    // Receive the first message
    int msgSize = 0;
    HandleFirstMsg(clientSocket, &msgSize);
    int mbatchSize = msgSize / sizeof(MessageHeader);
    /*iResult = recv(clientSocket, (char*)&msgSize, sizeof(int), 0);
    if (iResult == SOCKET_ERROR || iResult != sizeof(int)) {
        cout << "Error receiving the first message: " << WSAGetLastError() << endl;
        closesocket(clientSocket);
        WSACleanup();
        return;
    }*/

    // cout << "got payload size: " << msgSize << ", got batch size: " << mbatchSize << endl;
    // cout << "using batch size: " << batchSize << endl;
    if (mbatchSize != batchSize) {
        cerr << "ERROR: batch size MISMATCH on server and client" << endl;
    }
    else {
        cout << "got batch size from client: " << mbatchSize << endl;
    }

    char* recvBuffer = new char[msgSize];
    memset(recvBuffer, 0, msgSize);

    int oneReceive = 0;
    int remainingBytesToReceive = msgSize;

    int oneSend = 0;
    int remainingBytesToSend = batchRespSize;

    bool finished = false;

    while (!finished) {
        // Receive message from client and send response

        while (oneReceive < msgSize) {
            // cout << "receiving message..." << endl;
            iResult = recv(clientSocket, recvBuffer + oneReceive, remainingBytesToReceive, 0);

            if (iResult == SOCKET_ERROR) {
                if ((int64_t) msgProcessedCount[clientIndex] != readNum) {
                    cout << "Error receiving message: " << WSAGetLastError() << endl;
                    cout << "msgProcessedCount: " << (int64_t) msgProcessedCount[clientIndex] << endl;
                }
                else {
                    cout << "recv from client but client is finished and closed (conn rest)" << endl;
                }
                
                // int ret = closesocket(clientSocket); // close at the end
                // if (ret != 0) {
                //     int err = WSAGetLastError();
                //     // already done, but remote closed forcibly 10054: conn reset
                //     if (err == 10054 && (int64_t) msgProcessedCount[clientIndex] == readNum) {
                //         cout << "client " << clientIndex << " connection closed" << endl;
                //     }
                //     else {
                //         cout << "error closing socket for client " << clientIndex << endl;
                //     }
                // }
                // else {
                //     cout << "closing socket completed for client " << clientIndex << endl;
                // }
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
        MessageHeader *requestMsg = (MessageHeader *) recvBuffer;
        for (size_t i = 0; i < batchSize; i++) {
            // cout << "requestMsg batch id " << requestMsg->BatchId << endl;
            int index = requestMsg->BatchId * batchSize + i;
            char *respBuffer = bufferMem + index * responseSize;
            memcpy(respBuffer, requestMsg, sizeof(MessageHeader));
            respBuffer += sizeof(MessageHeader);
            ServerContext *respCtx = serverContexts + index;
            respCtx->clientIndex = clientIndex;
            respCtx->ReadStartTime = high_resolution_clock::now().time_since_epoch().count();
            respCtx->hSocket = clientSocket;
            respCtx->socketMutex = socketMutex;
            // respCtx->Index = index;
            respCtx->BatchId = requestMsg->BatchId;
            /* respCtx->Overlapped.Offset = requestMsg->Offset;
            respCtx->Overlapped.OffsetHigh = 0; */
            respCtx->Overlapped.Pointer = (PVOID) requestMsg->Offset;
            respCtx->Overlapped.hEvent = NULL;
            respCtx->bufferMem = bufferMem;
            respCtx->serverContexts = serverContexts;
            respCtx->batchCompletionCounts = batchCompletionCounts;
            iResult = ReadFile(fileHandles[requestMsg->FileId], respBuffer, payloadSize, NULL, &(respCtx->Overlapped));
            if (!iResult) {
                DWORD err = GetLastError();
                if (err != ERROR_IO_PENDING) {
                    cerr << "something wrong when ReadFile(), carry on regardless..., err: " << err << endl;
                    cerr << "offset: " << requestMsg->Offset << endl;
                    // cerr << "respBuffer: " << (void *)respBuffer << endl;
                }
                // else it's async
            }
            else {
                // completed sync, this really shouldn't happen when buffering is turned off
                cout << "ReadFile completed sync" << endl;
                // only send ALL responses if whole batch finished
                batchCompletionCounts[requestMsg->BatchId] += 1;
                if (batchCompletionCounts[requestMsg->BatchId] == batchSize) {
                    cout << "completed sync, and sending a batch back, " << requestMsg->BatchId << endl;
                    batchCompletionCounts[requestMsg->BatchId] = 0;
                    char *buf = bufferMem + (requestMsg->BatchId * batchRespSize);

                    socketMutex->lock();  // can't have concurrent sends, other pollers may also send on same socket
                    while (oneSend < batchRespSize) {
                        iResult = send(clientSocket, buf + oneSend, remainingBytesToSend, 0);
                        if (iResult == SOCKET_ERROR) {
                            cerr << "Error sending response: " << WSAGetLastError() << endl;
                            closesocket(clientSocket);
                            WSACleanup();
                            return;
                        }
                        oneSend += iResult;
                        remainingBytesToSend = batchRespSize - oneSend;
                    }
                    socketMutex->unlock();

                    oneSend = 0;
                    remainingBytesToSend = batchRespSize;
                }
            }
            requestMsg += 1;

        }

        /* while (oneSend < payloadSize) {
            iResult = send(clientSocket, recvBuffer + oneSend, remainingBytesToSend, 0);
            if (iResult == SOCKET_ERROR) {
                cout << "Error sending message: " << WSAGetLastError() << endl;
                closesocket(clientSocket);
                WSACleanup();
                return;
            }

            oneSend += iResult;
            remainingBytesToSend = payloadSize - oneSend;
        }

        oneSend = 0;
        remainingBytesToSend = payloadSize; */
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

    cout << "Measurement completes for thread " << clientIndex << endl;

    // Clear buffer
    delete[] recvBuffer;

    // Close the socket for this client
    int ret = closesocket(clientSocket);
    if (ret) {
        cout << "closing socket err: " << WSAGetLastError() << endl;;
    }
    else {
        cout << " Thread " << clientIndex << " closing client socket completed" << endl;
    }
}


// Function to generate random data of a given size
void GenerateRandomData(char* buffer, size_t size) {
    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<int> distribution(0, 51); // Range for uppercase and lowercase English letters

    for (size_t i = 0; i < size; i++) {
        int randomValue = distribution(generator);
        if (randomValue < 26) {
            // Uppercase letter (A-Z)
            buffer[i] = static_cast<char>('A' + randomValue);
        }
        else {
            // Lowercase letter (a-z)
            buffer[i] = static_cast<char>('a' + (randomValue - 26));
        }
    }
}

int GenerateFile(int GBSize, int nFile) {
    std::cout << "generating file, size: " << GBSize << std::endl;
    int fileSizeGB = GBSize;
    size_t fileSizeBytes = (size_t) fileSizeGB * 1024 * 1024 * 1024; // actual size
    string fileBaseName = FILE_BASE_NAME;

    for (int i = 0; i < nFile; i++) {
        string fileName = fileBaseName.append(to_string(i));
        wstring temp = wstring(fileName.begin(), fileName.end());
        LPCWSTR createFileName = temp.c_str();
        HANDLE hFile = CreateFile(createFileName, GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

        if (hFile == INVALID_HANDLE_VALUE) {
            std::cerr << "Error creating the file. Error code: " << GetLastError() << std::endl;
            return 1;
        }

        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            LARGE_INTEGER size;
            BOOL ret = GetFileSizeEx(hFile, &size);
            cout << "found file with size: " << size.QuadPart;

            if (ret && (size.QuadPart >= (int64_t) fileSizeBytes)) {
                // probably created earlier, let's skip writing
                std::cout << "file exists AND is of same size, not writing to it, file: " << fileName << std::endl;
                CloseHandle(hFile);
                continue;
            }
            // else still write the file
        }

        char buffer[8192]; // Buffer size for writing (adjust as needed)

        while (fileSizeBytes > 0) {
            size_t bytesToWrite = min(sizeof(buffer), fileSizeBytes);
            GenerateRandomData(buffer, bytesToWrite);

            DWORD bytesWritten;
            if (!WriteFile(hFile, buffer, static_cast<DWORD>(bytesToWrite), &bytesWritten, NULL)) {
                std::cerr << "Error writing to the file. Error code: " << GetLastError() << std::endl;
                CloseHandle(hFile);
                return 1;
            }

            fileSizeBytes -= bytesWritten;
        }

        CloseHandle(hFile);
        std::cout << "generated file: " << fileName << std::endl;
    }

    std::cout << "all files generated" << std::endl;
    return 0;
}


// pollers will run this
void HandleCompletions(int payloadSize, int batchSize, int readNum) {
    cout << "poller started, using payload size: " << payloadSize << endl;
    int iResult;
    int oneSend = 0;
    
    int batchRespSize = (sizeof(MessageHeader) + payloadSize) * batchSize;
    int remainingBytesToSend = batchRespSize;

    DWORD bytesTransferred = 0;
    uint64_t completionKey = NULL;

    // A pointer to a variable that receives the address of the OVERLAPPED structure
    // that was specified when the completed I/O operation was started.
    LPOVERLAPPED overlapped = NULL;
    while (true) {
        BOOL ret = GetQueuedCompletionStatus(ioCompletionPort, &bytesTransferred, &completionKey, &overlapped, INFINITE);
        if (!ret) {
            DWORD err = GetLastError();
            if (err == 735) { // completion port closed
                cout << "completion port closed, ending completion thread" << endl;
                break;
            }
            // this shouldn't happen
            cout << "Error, but ignoring, in GetQueuedCompletionStatus: " << err << endl;
        }
        ServerContext *ctx = (ServerContext *) overlapped;
        // cout << "one async read completed for batch id: " << ctx->BatchId << endl;

        // only send ALL responses if whole batch finished
        // ctx->batchCompletionCounts[ctx->BatchId] += 1; // TODO: racy??
        uint16_t completionCount = ctx->batchCompletionCounts[ctx->BatchId].fetch_add(1);
        atomic_uint64_t processedCount = msgProcessedCount[ctx->clientIndex].fetch_add(1);
        readLatencies[ctx->clientIndex][processedCount] = 
            (double) (high_resolution_clock::now().time_since_epoch().count() - ctx->ReadStartTime) / 1000;
        
        if (completionCount == batchSize - 1) {
            // cout << "one async completed and sending batch back, id: " << ctx->BatchId << endl;
            ctx->batchCompletionCounts[ctx->BatchId].store(0);  // reset
            char *buf = ctx->bufferMem + (ctx->BatchId * batchRespSize);

            auto sendStart = high_resolution_clock::now().time_since_epoch().count();
            ctx->socketMutex->lock();  // can't have concurrent sends, other pollers may also send on same socket
            while (oneSend < batchRespSize) {
                iResult = send(ctx->hSocket, buf + oneSend, remainingBytesToSend, 0);
                if (iResult == SOCKET_ERROR) {
                    cerr << "Error sending response: " << WSAGetLastError() << endl;
                    closesocket(ctx->hSocket);
                    WSACleanup();
                    return;
                }
                oneSend += iResult;
                remainingBytesToSend = batchRespSize - oneSend;
            }
            ctx->socketMutex->unlock();
            sendBackLatencies[ctx->clientIndex][processedCount / batchSize] =
                (double) (high_resolution_clock::now().time_since_epoch().count() - sendStart) / 1000;

            oneSend = 0;
            remainingBytesToSend = batchRespSize;
        }
        // msgProcessedCount[ctx->clientIndex]++; // this is racy!!!

        // TODO
        if (processedCount % 100000 == 0) {
            cout << "msgProcessedCount for client " << ctx->clientIndex << ": " << (int64_t) processedCount << endl;
        }
        // else nothing to do
    }
    // cout << "a completion thread ended..." << endl;
}

int RunServer(int payloadSize, int batchSize, int queueDepth, int readNum, int fileSize, int nFile, int nConnections, int nCompletionThreads) {
    cout << "server starting..." << endl;
    allThreads = new thread* [nConnections];
    // generate files
    GenerateFile(fileSize, nFile);

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


    // open files for read
    fileHandles = new HANDLE[nFile];
    string fileBaseName = FILE_BASE_NAME;
    for (int i = 0; i < nFile; i++) {
        string fileName = fileBaseName.append(to_string(i));
        wstring temp = wstring(fileName.begin(), fileName.end());
        LPCWSTR createFileName = temp.c_str();
        HANDLE hFile = CreateFile(createFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING, NULL);

        if (hFile == INVALID_HANDLE_VALUE) {
            std::cerr << "Error opening the file. Error code: " << GetLastError() << std::endl;
            return -1;
        }
        else {
            cout << "opened for async read, file " << i << endl;
            fileHandles[i] = hFile;
        }
    }


    // setup io completion port
    ioCompletionPort = CreateIoCompletionPort(fileHandles[0], NULL, NULL, nCompletionThreads);
    if (ioCompletionPort == NULL) {
        cout << "Error creating completion port: " << GetLastError() << endl;
        return -1;
    }
    cout << "IO completion port created" << endl;
    for (size_t i = 1; i < nFile; i++) {
        HANDLE ret = CreateIoCompletionPort(fileHandles[i], ioCompletionPort, NULL, nCompletionThreads);
        if (ret == NULL) {
            cout << "Error associating file to completion port: " << GetLastError() << endl;
            return -1;
        }
        else {
            cout << "file associated to completion port: " << i << endl;
        }
    }
    
    // setup threads for polling completions
    for (size_t i = 0; i < nCompletionThreads; i++) {
        thread *poller = new thread([payloadSize, batchSize, readNum]
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
    iResult = bind(listenSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr));
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
        cout << "accepted client: " << clientSocket << endl;

        thread* worker = new thread([clientSocket, payloadSize, batchSize, queueDepth, clientIndex]
            {
                ThreadFunc(clientSocket, payloadSize, batchSize, queueDepth, clientIndex);
            });
        allThreads[clientIndex] = worker;
        clientIndex++;
    }
    cout << "all clients connected, waiting for them to finish" << endl;
    int i = 0;
    for (i = 0; i < nConnections; i++) {
        allThreads[i]->join();
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


    // Close the listen socket and cleanup
    int ret = closesocket(listenSocket);
    if (ret) {
        cout << "closing listen socket err: " << WSAGetLastError() << endl;;
    }
    WSACleanup();
    cout << "closing listen socket completed" << endl;

    // close files opened
    for (int i = 0; i < nFile; i++) {
        ret = CloseHandle(fileHandles[i]);
        if (ret == 0) {
            cerr << "closing file " << i << " failed" << endl;
        }
    }
    cout << "closed files" << endl;

    // close completion port
    ret = CloseHandle(ioCompletionPort);
    if (ret == 0) {
        cerr << "closing completion port failed" << endl;
    }
    cout << "closed completion port" << endl;

    return 0;
}

int main(
    int argc,
    const char** args
)
{
    if (argc == 9) {
        //HANDLE hFile;
        //// create the file.
        //hFile = CreateFile(TEXT("large.TXT"), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        //if (hFile != INVALID_HANDLE_VALUE)
        //{
        //    DWORD dwByteCount;
        //    TCHAR szBuf[64] = TEXT("/0");
        //    // Write a simple string to hfile.
        //    WriteFile(hFile, "This is a simple message", 25, &dwByteCount, NULL);
        //    // Set the file pointer back to the beginning of the file.
        //    SetFilePointer(hFile, 0, 0, FILE_BEGIN);
        //    // Read the string back from the file.
        //    ReadFile(hFile, szBuf, 128, &dwByteCount, NULL);
        //    // Null terminate the string.
        //    szBuf[dwByteCount] = 0;
        //    // Close the file.
        //    CloseHandle(hFile);
        //    //output message with string if successful
        //    cout << "created large.txt" << endl;
        //}
        //else
        //{
        //    //output message if unsuccessful
        //    cout << "creation failed" << endl;
        //}
        //cout << "Server usage: msgSize queueDepth, shouldn't run with 1 arg" << endl;
        int readSize = stoi(args[1]);
        int batchSize = stoi(args[2]);
        int queueDepth = stoi(args[3]);
        readNum = stoi(args[4]);
        int fileSize = stoi(args[5]);
        int nFile = stoi(args[6]);
        int nConnections = stoi(args[7]);
        int nCompletionThreads = stoi(args[8]);
        cout << "queue depth: " << queueDepth << endl;
        return RunServer(readSize, batchSize, queueDepth, readNum, fileSize, nFile, nConnections, nCompletionThreads);
    }
    else {
        cout << "Server usage: readSize batchSize queueDepth readNum fileSize(GB) nFile nConnections nCompletionThreads" << endl;
    }

    return 0;
}