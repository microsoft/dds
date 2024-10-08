/*
 * Copyright (c) Microsoft Corporation.
 * Licensed under the MIT License
 */

#include <cstdlib>
#include <winsock2.h>
#include <iostream>
#include <thread>
#include <string>
#include <string.h>
#include <windows.h>
#include <tchar.h>
#include <random>

#include "../Common/Include/Config.h"
#include "../Common/Include/LatencyHelpers.h"
#include "Client.h"

#pragma comment(lib,"ws2_32.lib")
#pragma warning(disable: 6385)
#pragma warning(disable: 6386)
#pragma warning(disable:4996) 

using std::srand;
using std::rand;
using std::cout;
using std::endl;
using std::thread;

const int RAND_SEED = 0;
int* FILEID;
int FILENUM;
uint64_t FILE_SIZE;

int RunClientForLatency(
    int MessageSize,
    int BatchSize,
    int QueueDepth,
    uint64_t ReadNum,
    int Port
) {
    //
    // Initialize Winsock
    //
    //
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        cout << "WSAStartup failed with error: " << iResult << endl;
        return 1;
    }

    //
    // Create a socket for the client
    //
    //
    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET) {
        cout << "Error creating socket: " << WSAGetLastError() << endl;
        WSACleanup();
        return 1;
    }

    //
    // set socket to no delay, sends out packet without coalescing
    //
    //
    int noDelay = 1;
    iResult = setsockopt(clientSocket, IPPROTO_TCP, TCP_NODELAY, (char*) &noDelay, sizeof(noDelay));
    if (iResult) {
        printf("set TCP_NODELAY failed: %d\n", WSAGetLastError());
    }

    //
    // Set up the address of the client
    //
    //
    sockaddr_in clientAddr;
    clientAddr.sin_family = AF_INET;
    clientAddr.sin_port = htons(Port);
    clientAddr.sin_addr.s_addr = inet_addr(CLIENT_IP);

    // Set up the address of the server
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);
    serverAddr.sin_addr.s_addr = inet_addr(SERVER_IP);

    cout << "Connecting to the server..." << endl;

    //
    // Connect to the server
    //
    //
    iResult = connect(clientSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr));
    if (iResult == SOCKET_ERROR) {
        cout << "Unable to connect to server: " << WSAGetLastError() << endl;
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    cout << "Preparing message buffers..." << endl;

    // Send message to the server and receive response
    const int HeaderSize = sizeof(MessageHeader);

    char* sendBuffer = new char[HeaderSize*BatchSize];
    memset(sendBuffer, 0, HeaderSize);
    MessageHeader* hdr;
    for (int i = 0; i != BatchSize; i++) {
        hdr = (MessageHeader*)(sendBuffer+i*HeaderSize);
        hdr->Length = MessageSize;
    }
    int TOTAL_MESSAGE_SIZE = (HeaderSize + MessageSize)*BatchSize;
    char* recvBuffer = new char[TOTAL_MESSAGE_SIZE];
    memset(recvBuffer, 0, TOTAL_MESSAGE_SIZE);

    double* latencies = new double[ReadNum];
    for (int l = 0; l != ReadNum; l++) {
        latencies[l] = 0;
    }

    //
    //Prepare FileId and offset for each request
    // 
    // 
    uint16_t* FileIds = new uint16_t[ReadNum];
    uint64_t* Offsets = new uint64_t[ReadNum];
    std::random_device rd;
    std::mt19937_64 eng(rd());
    std::uniform_int_distribution<uint64_t> distr;

    for (int l = 0; l != ReadNum; l++) {
        FileIds[l] = FILEID[rand() % FILENUM];
        uint64_t randomValue = distr(eng);
        randomValue %= (FILE_SIZE - MessageSize - 1);
        // ensure offset is sector size aligned
        randomValue = (randomValue / SECTOR_SIZE) * SECTOR_SIZE;
        Offsets[l] = randomValue;
    }

    cout << "Starting measurement..." << endl;

    int firstMsg = HeaderSize * BatchSize;
    iResult = send(clientSocket, (const char*)&firstMsg, sizeof(int), 0);
    if (iResult == SOCKET_ERROR || iResult != sizeof(int)) {
        cout << "Error sending the first message: " << WSAGetLastError() << endl;
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    uint64_t bytesCompleted = 0;
    uint64_t bytesSent = 0;

    auto startTime = high_resolution_clock::now().time_since_epoch().count(); // measure start time

    int oneSend = 0;
    int remainingBytesToSend = HeaderSize;
    int actualBytesToSend;

    uint64_t msgIndex = 0;

    for (uint64_t q = 0; q != QueueDepth; q++) {
        //batch size
        for (int i = 0; i != BatchSize; i++) {
            hdr = (MessageHeader*)(sendBuffer + i * HeaderSize);
            hdr->TimeSend = high_resolution_clock::now().time_since_epoch().count();
            hdr->BatchId = (uint16_t)q;
            hdr->FileId = FileIds[msgIndex * BatchSize + i];
            hdr->Offset = Offsets[msgIndex * BatchSize + i];

        }
        msgIndex++;
        while (oneSend < HeaderSize * BatchSize) {
            actualBytesToSend = remainingBytesToSend > SEND_PACKET_SIZE ? SEND_PACKET_SIZE : remainingBytesToSend;
            iResult = send(clientSocket, sendBuffer + oneSend, actualBytesToSend, 0);
            if (iResult == SOCKET_ERROR) {
                cout << "Error sending message: " << WSAGetLastError() << endl;
                closesocket(clientSocket);
                WSACleanup();
                return 1;
            }

            oneSend += iResult;
            remainingBytesToSend = HeaderSize * BatchSize - oneSend;
        }

        bytesSent += oneSend;

        oneSend = 0;
        remainingBytesToSend = HeaderSize * BatchSize;
    }

    int oneReceive = 0;
    int remainingBytesToReceive = TOTAL_MESSAGE_SIZE;
    uint64_t RespIndex = 0;

    while (true) {
        //
        // receive one message
        //
        //
        while (oneReceive < TOTAL_MESSAGE_SIZE) {
            iResult = recv(clientSocket, recvBuffer + oneReceive, remainingBytesToReceive, 0);

            if (iResult == SOCKET_ERROR) {
                cout << "Error receiving message: " << WSAGetLastError() << endl;
                closesocket(clientSocket);
                WSACleanup();
                return 1;
            }

            oneReceive += iResult;
            remainingBytesToReceive = TOTAL_MESSAGE_SIZE - oneReceive;
        }

        //
        // measure latency
        //
        //
        long long diff;
        uint16_t BatchID;
        for (int i = 0; i != BatchSize; i++) {
            //diff returns the time in nanosecond, here we divide it by 1000 to get time in microsecond
            hdr = (MessageHeader*)(recvBuffer + i * (HeaderSize+ MessageSize));
            diff = high_resolution_clock::now().time_since_epoch().count() - hdr->TimeSend;
            BatchID = hdr->BatchId;
            latencies[RespIndex * BatchSize + i] = ((double) diff) / 1000;
        }
        bytesCompleted += oneReceive;
        RespIndex++;

        if (RespIndex == ReadNum/BatchSize) { 
            break;
        }

        oneReceive = 0;
        remainingBytesToReceive = TOTAL_MESSAGE_SIZE;

        //
        // send another message
        //
        //
        if (msgIndex < ReadNum/BatchSize) {
            for (int i = 0; i != BatchSize; i++) {
                hdr = (MessageHeader*)(sendBuffer + i * HeaderSize);
                hdr->TimeSend = high_resolution_clock::now().time_since_epoch().count();
                hdr->BatchId = BatchID;
                hdr->FileId = FileIds[msgIndex * BatchSize + i];
                hdr->Offset = Offsets[msgIndex * BatchSize + i];

            }
            msgIndex++;
            while (oneSend < HeaderSize * BatchSize) {
                actualBytesToSend = remainingBytesToSend > SEND_PACKET_SIZE ? SEND_PACKET_SIZE : remainingBytesToSend;
                iResult = send(clientSocket, sendBuffer + oneSend, actualBytesToSend, 0);
                if (iResult == SOCKET_ERROR) {
                    std::cout << "Error sending message: " << WSAGetLastError() << endl;
                    closesocket(clientSocket);
                    WSACleanup();
                    return 1;
                }

                oneSend += iResult;
                remainingBytesToSend = HeaderSize * BatchSize - oneSend;
            }

            bytesSent += oneSend;

            oneSend = 0;
            remainingBytesToSend = HeaderSize * BatchSize;
        }
    }
    auto endTime = high_resolution_clock::now().time_since_epoch().count(); // measure end time

    cout << "Processing results..." << endl;

    //
    // Calculate throughput
    //
    //
    auto duration = (endTime - startTime)/1000; // duration in microseconds
    double throughput = (double)bytesCompleted / (double)duration * 1000000.0f / (1024.0 * 1024.0 * 1024.0); // in giga bytes per second

    //
    // Calculate latency
    //
    //
    Statistics LatencyStats;
    Percentiles PercentileStats;
    GetStatistics(latencies, ReadNum, &LatencyStats, &PercentileStats);
    printf(
        "Result for %zu requests of %d bytes (%.2lf seconds): %.2lf RPS, Min: %.2lf, Max: %.2lf, 50th: %.2lf, 90th: %.2lf, 99th: %.2lf, 99.9th: %.2lf, 99.99th: %.2lf, StdErr: %.2lf\n",
        ReadNum,
        MessageSize,
        (duration / 1000000.0),
        (ReadNum / (duration / 1000000.0)),
        LatencyStats.Min,
        LatencyStats.Max,
        PercentileStats.P50,
        PercentileStats.P90,
        PercentileStats.P99,
        PercentileStats.P99p9,
        PercentileStats.P99p99,
        LatencyStats.StandardError);

    cout << "Throughput: " << throughput << " GB/s" << endl;

    //
    // Release buffers
    //
    //
    delete[] latencies;
    // delete[] oneMessage;
    delete[] sendBuffer;
    delete[] recvBuffer;
    delete[] FileIds;
    delete[] Offsets;

    //
    // Close the socket and clean up
    //
    //
    int ret = closesocket(clientSocket);
    if (ret != 0) {
        cout << "closing socket err: " << WSAGetLastError() << endl;;
    }
    WSACleanup();
    cout << "closing socket completed" << endl;

    return 0;
}

void ThreadFunc(
    int ThreadNum,
    int MessageSize,
    int BatchSize,
    const uint64_t ReadNum,
    const int QueueDepth,
    SOCKET* ClientSocket,
    char* SendBuffer,
    char* RecvBuffer,
    uint16_t* FileIds,
    uint64_t* Offsets,
    double* latencies,
    std::chrono::steady_clock::time_point* StartTime,
    std::chrono::steady_clock::time_point* EndTime,
    uint64_t* BytesCompleted,
    char** content
) {
    int iResult;
    const int HeaderSize = sizeof(MessageHeader);
    //the following are used for read
    /*const int firstMsg = HeaderSize*BatchSize;
    const int TOTAL_MESSAGE_SIZE = BatchSize * (HeaderSize + MessageSize);*/
    //the following are used for write
    const int TOTAL_MESSAGE_SIZE = HeaderSize * BatchSize;
    const int firstMsg = BatchSize * (HeaderSize + MessageSize);

    iResult = send(*ClientSocket, (const char*)&firstMsg, sizeof(int), 0);
    if (iResult == SOCKET_ERROR || iResult != sizeof(int)) {
        cout << "Error sending the first message: " << WSAGetLastError() << endl;
        closesocket(*ClientSocket);
        WSACleanup();
        return;
    }

    uint64_t bytesCompleted = 0;
    uint64_t bytesSent = 0;

    int oneSend = 0;
    int remainingBytesToSend = firstMsg;
    int actualBytesToSend;
    uint64_t msgIndex = 0;

    MessageHeader* hdr;

    *StartTime = high_resolution_clock::now();

    for (uint64_t q = 0; q != QueueDepth; q++) {
        for (int i = 0; i != BatchSize; i++) {
            //read
            //hdr = (MessageHeader*)(SendBuffer + i * HeaderSize);
            //write
            hdr = (MessageHeader*)(SendBuffer + i * (HeaderSize+ MessageSize));
            hdr->TimeSend = high_resolution_clock::now().time_since_epoch().count();
            hdr->BatchId = (uint16_t)q;
            hdr->FileId = FileIds[msgIndex * BatchSize + i];
            hdr->Offset = Offsets[msgIndex * BatchSize + i];
            //only for write
            memcpy(SendBuffer + i * (HeaderSize + MessageSize) + HeaderSize, content[msgIndex * BatchSize + i], MessageSize);

        }
        msgIndex++;
        while (oneSend < firstMsg) {
            actualBytesToSend = remainingBytesToSend > SEND_PACKET_SIZE ? SEND_PACKET_SIZE : remainingBytesToSend;
            iResult = send(*ClientSocket, SendBuffer + oneSend, actualBytesToSend, 0);
            if (iResult == SOCKET_ERROR) {
                cout << "Error sending message: " << WSAGetLastError() << endl;
                closesocket(*ClientSocket);
                WSACleanup();
                return;
            }

            oneSend += iResult;
            remainingBytesToSend = firstMsg - oneSend;
        }

        bytesSent += oneSend;

        oneSend = 0;
        remainingBytesToSend = firstMsg;
    }

    int oneReceive = 0;
    int remainingBytesToReceive = TOTAL_MESSAGE_SIZE;
    uint64_t RespIndex = 0;

    while (true) {
        //
        // receive one message
        //
        //
        while (oneReceive < TOTAL_MESSAGE_SIZE) {
            iResult = recv(*ClientSocket, RecvBuffer + oneReceive, remainingBytesToReceive, 0);
            if (iResult == SOCKET_ERROR) {
                cout << "Error receiving message: " << WSAGetLastError() << endl;
                closesocket(*ClientSocket);
                WSACleanup();
                return;
            }

            oneReceive += iResult;
            remainingBytesToReceive = TOTAL_MESSAGE_SIZE - oneReceive;
        }
        //
        // measure latency
        //
        //
        long long diff;
        uint16_t BatchID;
        for (int i = 0; i != BatchSize; i++) {
            //diff returns the time in nanosecond, here we divide it by 1000 to get time in microsecond
            //used for read
            //hdr = (MessageHeader*)(RecvBuffer + i * (HeaderSize + MessageSize));
            //used for write
            hdr = (MessageHeader*)(RecvBuffer + i * HeaderSize);
            diff = high_resolution_clock::now().time_since_epoch().count() - hdr->TimeSend;
            BatchID = hdr->BatchId;
            if (BatchID > QueueDepth) {
                cerr << "ERROR: got wrong batchId from server: " << BatchID << endl;
            }
            latencies[RespIndex * BatchSize + i] = (double) diff / 1000;
        }
        bytesCompleted += oneReceive;
        RespIndex++;

        if (RespIndex == ReadNum/BatchSize) {
            break;
        }

        oneReceive = 0;
        remainingBytesToReceive = TOTAL_MESSAGE_SIZE;

        //
        // send another message
        //
        //
        if (msgIndex < ReadNum / BatchSize) {
            for (int i = 0; i != BatchSize; i++) {
                //read
                //hdr = (MessageHeader*)(SendBuffer + i * HeaderSize);
                //write
                hdr = (MessageHeader*)(SendBuffer + i * (HeaderSize + MessageSize));
                hdr->TimeSend = high_resolution_clock::now().time_since_epoch().count();
                hdr->BatchId = (uint16_t)BatchID;
                hdr->FileId = FileIds[msgIndex * BatchSize + i];
                hdr->Offset = Offsets[msgIndex * BatchSize + i];
                //only for write
                memcpy(SendBuffer + i * (HeaderSize + MessageSize) + HeaderSize, content[msgIndex * BatchSize + i], MessageSize);

            }
            msgIndex++;
            while (oneSend < firstMsg) {
                actualBytesToSend = remainingBytesToSend > SEND_PACKET_SIZE ? SEND_PACKET_SIZE : remainingBytesToSend;
                iResult = send(*ClientSocket, SendBuffer + oneSend, actualBytesToSend, 0);
                if (iResult == SOCKET_ERROR) {
                    cout << "Error sending message: " << WSAGetLastError() << endl;
                    closesocket(*ClientSocket);
                    WSACleanup();
                    return;
                }

                oneSend += iResult;
                remainingBytesToSend = firstMsg - oneSend;
            }

            bytesSent += oneSend;

            oneSend = 0;
            remainingBytesToSend = firstMsg;
        }
    }

    *EndTime = high_resolution_clock::now();
    *BytesCompleted = bytesCompleted;
}
void GenerateRandomData(char* buffer, size_t size) {
    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<int> distribution(0, 51); // Range for uppercase and lowercase English letters

    for (size_t i = 0; i < size; i++) {
        int randomValue = distribution(generator);
        if (randomValue < 26) {
            buffer[i] = static_cast<char>('A' + randomValue);
        }
        else {
            buffer[i] = static_cast<char>('a' + (randomValue - 26));
        }
    }
}

int RunClientForThroughput(
    int MessageSize,
    int BatchSize,
    int QueueDepth,
    uint64_t ReadNum,
    int PortBase,
    int NumConnections
) {
    //
    // Initialize Winsock
    //
    //
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        cout << "WSAStartup failed with error: " << iResult << endl;
        return 1;
    }

    //
    // Create sockets for the client
    //
    //
    SOCKET* clientSockets = new SOCKET[NumConnections];
    sockaddr_in* clientAddrs = new sockaddr_in[NumConnections];

    for (int i = 0; i != NumConnections; i++) {
        clientSockets[i] = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (clientSockets[i] == INVALID_SOCKET) {
            cout << "Error creating socket: " << WSAGetLastError() << endl;
            WSACleanup();
            return 1;
        }
        int noDelay = 1;
        iResult = setsockopt(clientSockets[i], IPPROTO_TCP, TCP_NODELAY, (char*) &noDelay, sizeof(noDelay));
        if (iResult) {
            printf("set socket %d TCP_NODELAY failed: %d\n", i, WSAGetLastError());
        }

        clientAddrs[i].sin_family = AF_INET;
        clientAddrs[i].sin_port = htons(PortBase + i);
        clientAddrs[i].sin_addr.s_addr = inet_addr(CLIENT_IP);
    }

    //
    // Set up the address of the server
    //
    //
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);
    serverAddr.sin_addr.s_addr = inet_addr(SERVER_IP);

    cout << "Connecting to the server..." << endl;

    //
    // Connect to the server
    //
    //
    for (int i = NumConnections - 1; i >= 0; i--) {
        cout << "trying connection " << i << endl;
        iResult = connect(clientSockets[i], (SOCKADDR*)&serverAddr, sizeof(serverAddr));
        if (iResult == SOCKET_ERROR) {
            cout << "Unable to connect to server: " << WSAGetLastError() << endl;
            closesocket(clientSockets[i]);
            WSACleanup();
            return 1;
        }
        cout << "Connection #" << i << " has been connected" << endl;
    }

    cout << "Preparing message buffers..." << endl;

    const int HeaderSize = sizeof(MessageHeader);


    char** sendBuffers = new char* [NumConnections];
    char** recvBuffers = new char* [NumConnections];
    //the following are used for read
    /*int TOTAL_MESSAGE_SIZE = (HeaderSize + MessageSize) * BatchSize;
    for (int i = 0; i != NumConnections; i++) {
        sendBuffers[i] = new char[HeaderSize*BatchSize];
        memset(sendBuffers[i], 0, HeaderSize * BatchSize);
        MessageHeader* hdr;
        for (int j = 0; j != BatchSize; j++) {
            hdr = (MessageHeader*)(sendBuffers[i] + j * HeaderSize);
            hdr->Length = MessageSize;
        }
        recvBuffers[i] = new char[TOTAL_MESSAGE_SIZE];
        memset(recvBuffers[i], 0, TOTAL_MESSAGE_SIZE);
    }*/
    //the following are used for write
    int TOTAL_MESSAGE_SIZE = (HeaderSize + MessageSize) * BatchSize;
    for (int i = 0; i != NumConnections; i++) {
        sendBuffers[i] = new char[TOTAL_MESSAGE_SIZE];
        memset(sendBuffers[i], 0, TOTAL_MESSAGE_SIZE);
        MessageHeader* hdr;
        for (int j = 0; j != BatchSize; j++) {
            hdr = (MessageHeader*)(sendBuffers[i] + j * (HeaderSize + MessageSize));
            hdr->Length = MessageSize;
        }
        recvBuffers[i] = new char[HeaderSize * BatchSize];
        memset(recvBuffers[i], 0, HeaderSize * BatchSize);
    }

    //
    // Prepare FileId and offset for each request
    //
    //
    std::random_device rd;
    std::mt19937_64 eng(rd());
    std::uniform_int_distribution<uint64_t> distr;

    uint16_t** FileIds = new uint16_t* [NumConnections];
    uint64_t** Offsets = new uint64_t* [NumConnections];
    double** latencies = new double* [NumConnections];
    char*** content = new char** [NumConnections];
    for (int i = 0; i != NumConnections; i++) {
        FileIds[i] = new uint16_t[ReadNum];
        Offsets[i] = new uint64_t[ReadNum];
        latencies[i] = new double[ReadNum];
        content[i] = new char* [ReadNum];
        for (int l = 0; l != ReadNum; l++) {
            FileIds[i][l] = FILEID[rand() % FILENUM];
            // Offsets[i][l] = rand() % (FILE_SIZE - MessageSize - 1);
            uint64_t randomValue = distr(eng);
            randomValue %= (FILE_SIZE - MessageSize - 1);
            // ensure offset is sector size aligned
            randomValue = (randomValue / SECTOR_SIZE) * SECTOR_SIZE;
            Offsets[i][l] = randomValue;
            if (Offsets[i][l] + 8192 > 1073741824) {
                cout << "wrong offset: " << Offsets[i][l] << endl;
            }
            latencies[i][l] = 0;
            content[i][l] = new char[MessageSize];
            GenerateRandomData(content[i][l], MessageSize);

        }
    }  

    cout << "Starting measurement..." << endl;
    std::chrono::steady_clock::time_point* startTimes = new std::chrono::steady_clock::time_point[NumConnections];
    std::chrono::steady_clock::time_point* endTimes = new std::chrono::steady_clock::time_point[NumConnections];
    uint64_t* bytesCompleted = new uint64_t[NumConnections];

    thread** ioThreads = new thread * [NumConnections];
    for (int i = 0; i != NumConnections; i++) {
        thread* worker = new thread([i, MessageSize, BatchSize, ReadNum, QueueDepth, clientSockets, sendBuffers, recvBuffers, FileIds, Offsets, latencies, startTimes, endTimes, bytesCompleted, content]
            {
                ThreadFunc(i, MessageSize, BatchSize, ReadNum, QueueDepth, &clientSockets[i], sendBuffers[i], recvBuffers[i], FileIds[i], Offsets[i], latencies[i], &startTimes[i], &endTimes[i], &bytesCompleted[i], content[i]);
            });
        ioThreads[i] = worker;
    }

    for (int i = 0; i != NumConnections; i++) {
        ioThreads[i]->join();
    }

    cout << "Processing results..." << endl;

    //
    // Calculate throughput
    //
    //
    std::chrono::steady_clock::time_point aggStartTime = startTimes[0], aggEndTime = endTimes[0];
    uint64_t aggBytesCompleted = 0;
    double P50Sum = 0;
    double P90Sum = 0;
    double P99Sum = 0;
    Statistics LatencyStats;
    Percentiles PercentileStats;
    for (int i = 0; i != NumConnections; i++) {
        if (aggStartTime > startTimes[i]) {
            aggStartTime = startTimes[i];
        }
        if (aggEndTime < endTimes[i]) {
            aggEndTime = endTimes[i];
        }

        aggBytesCompleted += bytesCompleted[i];
    }
    auto duration = duration_cast<microseconds>(aggEndTime - aggStartTime).count(); // duration in microseconds
    double throughput = (double)aggBytesCompleted / (double)duration * 1000000.0f / (1024.0 * 1024.0 * 1024.0); // in giga bytes per second
    for (int i = 0; i != NumConnections; i++) {
        GetStatistics(latencies[i], ReadNum, &LatencyStats, &PercentileStats);
        printf(
            "Thread %d: Result for %zu requests of %d bytes (%.2lf seconds): %.2lf RPS, Min: %.2lf, Max: %.2lf, 50th: %.2lf, 90th: %.2lf, 99th: %.2lf, 99.9th: %.2lf, 99.99th: %.2lf, StdErr: %.2lf\n",
            i,
            ReadNum,
            MessageSize,
            (duration / 1000000.0),
            (ReadNum / (duration / 1000000.0)),
            LatencyStats.Min,
            LatencyStats.Max,
            PercentileStats.P50,
            PercentileStats.P90,
            PercentileStats.P99,
            PercentileStats.P99p9,
            PercentileStats.P99p99,
            LatencyStats.StandardError);
        P50Sum += PercentileStats.P50;
        P90Sum += PercentileStats.P90;
        P99Sum += PercentileStats.P99;
    }


    cout << "Throughput: " << throughput << " GB/s" << endl;
    cout << "IOPS: " << ReadNum* NumConnections/((double)duration/ 1000000) << "RPS" << endl;
    cout << "Avg P50: " << P50Sum / NumConnections << endl;
    cout << "Avg P90: " << P90Sum / NumConnections << endl;
    cout << "Avg P99: " << P99Sum / NumConnections << endl;

    //
    // Release buffers
    //
    //
    for (int i = 0; i != NumConnections; i++) {
        delete[] sendBuffers[i];
        delete[] recvBuffers[i];
        delete[] FileIds[i];
        delete[] Offsets[i];
        delete[] latencies[i];
        for (int j = 0; j != ReadNum; j++) {
            delete[] content[i][j];
        }
        delete content[i];
    }
    delete[] sendBuffers;
    delete[] recvBuffers;
    delete[] FileIds;
    delete[] Offsets;
    delete[] latencies;
    delete[] FILEID;
    delete[] content;

    //
    // Close the socket and clean up
    //
    //
    for (int i = 0; i != NumConnections; i++) {
        closesocket(clientSockets[i]);
    }
    delete[] clientSockets;
    WSACleanup();

    return 0;
}
struct FileReadData {
    HANDLE hFile;
    char* buffer;
};


void CALLBACK FileReadCompletion(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped) {
    cout << "callback function started" << endl;
    FileReadData* readData = reinterpret_cast<FileReadData*>(lpOverlapped->hEvent);
    if (dwErrorCode == 0) {
        cout << "Read " << dwNumberOfBytesTransfered << " bytes from file." << endl;
        CloseHandle(readData->hFile);

    }
    else {
        std::cerr << "Error reading file. Error code: " << dwErrorCode << endl;
    }

    // Cleanup, if necessary
    if (lpOverlapped != nullptr) {
        delete lpOverlapped;
        delete[] readData->buffer;
        delete readData;
    }
}


int main(
    int argc,
    const char** args
)
{
    if (argc == 7) {
        int msgSize = stoi(args[1]);
        int batchSize = stoi(args[2]);
        int queueDepth = stoi(args[3]);
        uint64_t readNum = stoull(args[4]);
        int port = stoi(args[5]);
        uint64_t fileSize = stoull(args[6]);
        FILE_SIZE = fileSize * 1024 * 1024 * 1024;
        return RunClientForLatency(msgSize, batchSize, queueDepth, readNum, port);
    }
    else if (argc == 9) {
        int msgSize = stoi(args[1]);
        int batchSize = stoi(args[2]);
        int queueDepth = stoi(args[3]);
        uint64_t readNum = stoull(args[4]);
        int port = stoi(args[5]);
        int numConns = stoi(args[6]);
        uint64_t fileSize = stoull(args[7]);
        FILE_SIZE = fileSize * 1024 * 1024 * 1024;
        int fileNum = stoi(args[8]);
        FILEID = new int[fileNum];
        for (int i = 0; i != fileNum; i++) {
            FILEID[i] = i;
        }
        FILENUM = fileNum;
        return RunClientForThroughput(msgSize, batchSize, queueDepth, readNum, port, numConns);
    }
    else {
        cout << "Client (latency) usage: " << args[0] << " [Read Size] [Batch Size] [Queue Depth] [ReadNum] [Port] [File Size (in gigabyte)]" << endl;
        cout << "Client (bandwidth) usage: " << args[0] << " [Read Size] [Batch Size] [Queue Depth] [ReadNum] [Port Base] [Num Connections] [File Size (in gigabyte)] [File Num]" << endl;
    }

    return 0;
}
