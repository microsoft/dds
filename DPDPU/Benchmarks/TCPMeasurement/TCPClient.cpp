#include <cstdlib>
#include <winsock2.h>
#include <iostream>
#include <thread>

#include "Config.h"
#include "LatencyHelpers.h"
#include "TCPClient.h"

#pragma comment(lib,"ws2_32.lib")
#pragma warning(disable: 6385)
#pragma warning(disable: 6386)

using std::srand;
using std::rand;
using std::cout;
using std::endl;
using std::thread;

const int RAND_SEED = 0;

int RunClientForLatency(
    int MessageSize,
    int QueueDepth,
    size_t TotalBytes,
    int Port,
    int OffloadPercent
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
    // Set up the address of the client
    //
    //
    sockaddr_in clientAddr;
    clientAddr.sin_family = AF_INET;
    clientAddr.sin_port = htons(Port);
    clientAddr.sin_addr.s_addr = inet_addr(CLIENT_IP);

    // Bind the socket to the local address
    iResult = bind(clientSocket, (SOCKADDR*)&clientAddr, sizeof(clientAddr));
    if (iResult == SOCKET_ERROR) {
        cout << "Error binding socket: " << WSAGetLastError() << endl;
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

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
    const int TOTAL_MESSAGE_SIZE = sizeof(MessageHeader) + MessageSize;

    char* oneMessage = new char[MessageSize];
    memset(oneMessage, 0, MessageSize);
    *((int*)oneMessage) = 42;

    char* sendBuffer = new char[TOTAL_MESSAGE_SIZE];
    memset(sendBuffer, 0, TOTAL_MESSAGE_SIZE);

    MessageHeader* hdr = (MessageHeader*)(sendBuffer);
    hdr->OffloadedToDPU = false;
    hdr->LastMessage = false;
    hdr->MessageSize = MessageSize;
    memcpy(sendBuffer + sizeof(MessageHeader), oneMessage, MessageSize);

    char* recvBuffer = new char[TOTAL_MESSAGE_SIZE];
    memset(recvBuffer, 0, TOTAL_MESSAGE_SIZE);

    const size_t TOTAL_MESSAGES = (TotalBytes / TOTAL_MESSAGE_SIZE) + (TotalBytes % TOTAL_MESSAGE_SIZE == 0 ? 0 : 1);
    double* latencies = new double[TOTAL_MESSAGES];
    for (size_t l = 0; l != TOTAL_MESSAGES; l++) {
        latencies[l] = 0;
    }

    //
    // Prepare offload bits
    //
    //
    srand(RAND_SEED);
    bool* isOffloaded = new bool[TOTAL_MESSAGES];
    for (size_t o = 0; o != TOTAL_MESSAGES; o++) {
        if (rand() % 100 <= OffloadPercent) {
            isOffloaded[o] = true;
        }
        else {
            isOffloaded[o] = false;
        }
    }

    cout << "Starting measurement..." << endl;

    int firstMsg = TOTAL_MESSAGE_SIZE;
    iResult = send(clientSocket, (const char*)&TOTAL_MESSAGE_SIZE, sizeof(int), 0);
    if (iResult == SOCKET_ERROR || iResult != sizeof(int)) {
        cout << "Error sending the first message: " << WSAGetLastError() << endl;
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    size_t bytesCompleted = 0;

    auto startTime = high_resolution_clock::now(); // measure start time

    int oneSend = 0;
    int remainingBytesToSend = TOTAL_MESSAGE_SIZE;
    size_t msgIndexForOffloading = 0;

    for (size_t q = 0; q != QueueDepth; q++) {
        hdr->OffloadedToDPU = isOffloaded[msgIndexForOffloading];
        msgIndexForOffloading++;
        hdr->TimeSend = high_resolution_clock::now();
        while (oneSend < TOTAL_MESSAGE_SIZE) {
            iResult = send(clientSocket, sendBuffer + oneSend, remainingBytesToSend, 0);
            if (iResult == SOCKET_ERROR) {
                cout << "Error sending message: " << WSAGetLastError() << endl;
                closesocket(clientSocket);
                WSACleanup();
                return 1;
            }

            oneSend += iResult;
            remainingBytesToSend = TOTAL_MESSAGE_SIZE - oneSend;
        }

        oneSend = 0;
        remainingBytesToSend = TOTAL_MESSAGE_SIZE;
    }

    int oneReceive = 0;
    int remainingBytesToReceive = TOTAL_MESSAGE_SIZE;
    size_t msgIndex = 0;

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
        std::chrono::duration<double> diff = high_resolution_clock::now() - ((MessageHeader*)recvBuffer)->TimeSend;
        latencies[msgIndex] = diff.count() * 1000000;

        bytesCompleted += oneReceive;
        msgIndex++;

        if (bytesCompleted >= TotalBytes) {
            break;
        }

        oneReceive = 0;
        remainingBytesToReceive = TOTAL_MESSAGE_SIZE;

        //
        // send another message
        //
        //
        hdr->OffloadedToDPU = isOffloaded[msgIndexForOffloading];
        msgIndexForOffloading++;
        hdr->TimeSend = high_resolution_clock::now();
        while (oneSend < TOTAL_MESSAGE_SIZE) {
            iResult = send(clientSocket, sendBuffer + oneSend, remainingBytesToSend, 0);
            if (iResult == SOCKET_ERROR) {
                std::cout << "Error sending message: " << WSAGetLastError() << endl;
                closesocket(clientSocket);
                WSACleanup();
                return 1;
            }

            oneSend += iResult;
            remainingBytesToSend = TOTAL_MESSAGE_SIZE - oneSend;
        }

        oneSend = 0;
        remainingBytesToSend = TOTAL_MESSAGE_SIZE;
    }

    auto endTime = high_resolution_clock::now(); // measure end time

    //
    // Signal the server to stop receiving
    //
    //
    // hdr->LastMessage = true;
    // send(clientSocket, (const char*)sendBuffer, sizeof(MessageHeader), 0);

    cout << "Processing results..." << endl;

    //
    // Calculate throughput
    //
    //
    auto duration = duration_cast<microseconds>(endTime - startTime).count(); // duration in microseconds
    double throughput = (double)bytesCompleted / (double)duration * 1000000.0f / (1024.0 * 1024.0 * 1024.0); // in giga bytes per second

    //
    // Calculate latency
    //
    //
    Statistics LatencyStats;
    Percentiles PercentileStats;
    GetStatistics(latencies, TOTAL_MESSAGES, &LatencyStats, &PercentileStats);
    printf(
        "Result for %llu requests of %d bytes (%.2lf seconds): %.2lf RPS, Min: %.2lf, Max: %.2lf, 50th: %.2lf, 90th: %.2lf, 99th: %.2lf, 99.9th: %.2lf, 99.99th: %.2lf, StdErr: %.2lf\n",
        TOTAL_MESSAGES,
        MessageSize,
        (duration / 1000000.0),
        (TOTAL_MESSAGES / (duration / 1000000.0)),
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
    delete[] oneMessage;
    delete[] sendBuffer;
    delete[] recvBuffer;

    //
    // Close the socket and clean up
    //
    //
    closesocket(clientSocket);
    WSACleanup();

    return 0;
}

void ThreadFunc(
    const int TOTAL_MESSAGE_SIZE,
    const int BytesPerConn,
    const int QueueDepth,
    SOCKET *ClientSocket,
    char* SendBuffer,
    char* RecvBuffer,
    bool* IsOffloaded,
    std::chrono::steady_clock::time_point* StartTime,
    std::chrono::steady_clock::time_point* EndTime,
    size_t* BytesCompleted
) {
    int iResult;
    int firstMsg = TOTAL_MESSAGE_SIZE;
    iResult = send(*ClientSocket, (const char*)&TOTAL_MESSAGE_SIZE, sizeof(int), 0);
    if (iResult == SOCKET_ERROR || iResult != sizeof(int)) {
        cout << "Error sending the first message: " << WSAGetLastError() << endl;
        closesocket(*ClientSocket);
        WSACleanup();
        return;
    }

    size_t bytesCompleted = 0;

    int oneSend = 0;
    int remainingBytesToSend = TOTAL_MESSAGE_SIZE;
    size_t msgIndexForOffloading = 0;

    MessageHeader* hdr = (MessageHeader*)(SendBuffer);

    *StartTime = high_resolution_clock::now();

    for (size_t q = 0; q != QueueDepth; q++) {
        hdr->OffloadedToDPU = IsOffloaded[msgIndexForOffloading];
        msgIndexForOffloading++;
        while (oneSend < TOTAL_MESSAGE_SIZE) {
            iResult = send(*ClientSocket, SendBuffer + oneSend, remainingBytesToSend, 0);
            if (iResult == SOCKET_ERROR) {
                cout << "Error sending message: " << WSAGetLastError() << endl;
                closesocket(*ClientSocket);
                WSACleanup();
                return;
            }

            oneSend += iResult;
            remainingBytesToSend = TOTAL_MESSAGE_SIZE - oneSend;
        }

        oneSend = 0;
        remainingBytesToSend = TOTAL_MESSAGE_SIZE;
    }

    int oneReceive = 0;
    int remainingBytesToReceive = TOTAL_MESSAGE_SIZE;
    size_t msgIndex = 0;

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

        bytesCompleted += oneReceive;
        msgIndex++;

        if (bytesCompleted >= BytesPerConn) {
            break;
        }

        oneReceive = 0;
        remainingBytesToReceive = TOTAL_MESSAGE_SIZE;

        //
        // send another message
        //
        //
        hdr->OffloadedToDPU = IsOffloaded[msgIndexForOffloading];
        msgIndexForOffloading++;
        while (oneSend < TOTAL_MESSAGE_SIZE) {
            iResult = send(*ClientSocket, SendBuffer + oneSend, remainingBytesToSend, 0);
            if (iResult == SOCKET_ERROR) {
                std::cout << "Error sending message: " << WSAGetLastError() << endl;
                closesocket(*ClientSocket);
                WSACleanup();
                return;
            }

            oneSend += iResult;
            remainingBytesToSend = TOTAL_MESSAGE_SIZE - oneSend;
        }

        oneSend = 0;
        remainingBytesToSend = TOTAL_MESSAGE_SIZE;
    }

    *EndTime = high_resolution_clock::now();
    *BytesCompleted = bytesCompleted;
}

int RunClientForThroughput(
    int MessageSize,
    int QueueDepth,
    size_t BytesPerConn,
    int PortBase,
    int OffloadPercent,
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

        clientAddrs[i].sin_family = AF_INET;
        clientAddrs[i].sin_port = htons(PortBase + i);
        clientAddrs[i].sin_addr.s_addr = inet_addr(CLIENT_IP);

        iResult = bind(clientSockets[i], (SOCKADDR*)&clientAddrs[i], sizeof(clientAddrs[i]));
        if (iResult == SOCKET_ERROR) {
            cout << "Error binding socket: " << WSAGetLastError() << endl;
            closesocket(clientSockets[i]);
            WSACleanup();
            return 1;
        }
    }

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
    for (int i = NumConnections - 1; i >= 0; i--) {
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

    // Send message to the server and receive response
    const int TOTAL_MESSAGE_SIZE = sizeof(MessageHeader) + MessageSize;

    char* oneMessage = new char[MessageSize];
    memset(oneMessage, 0, MessageSize);
    *((int*)oneMessage) = 42;

    char** sendBuffers = new char*[NumConnections];
    char** recvBuffers = new char* [NumConnections];
    for (int i = 0; i != NumConnections; i++) {
        sendBuffers[i] = new char[TOTAL_MESSAGE_SIZE];
        memset(sendBuffers[i], 0, TOTAL_MESSAGE_SIZE);

        MessageHeader* hdr = (MessageHeader*)(sendBuffers[i]);
        hdr->OffloadedToDPU = false;
        hdr->LastMessage = false;
        hdr->MessageSize = MessageSize;
        memcpy(sendBuffers[i] + sizeof(MessageHeader), oneMessage, MessageSize);

        recvBuffers[i] = new char[TOTAL_MESSAGE_SIZE];
        memset(recvBuffers[i], 0, TOTAL_MESSAGE_SIZE);
    }

    const size_t TOTAL_MESSAGES = (BytesPerConn / TOTAL_MESSAGE_SIZE) + (BytesPerConn % TOTAL_MESSAGE_SIZE == 0 ? 0 : 1);

    //
    // Prepare offload bits
    //
    //
    srand(RAND_SEED);

    bool** isOffloaded = new bool* [NumConnections];
    
    for (int i = 0; i != NumConnections; i++) {
        isOffloaded[i] = new bool[TOTAL_MESSAGES];
        for (size_t o = 0; o != TOTAL_MESSAGES; o++) {
            if (rand() % 100 <= OffloadPercent) {
                isOffloaded[i][o] = true;
            }
            else {
                isOffloaded[i][o] = false;
            }
        }
    }

    cout << "Starting measurement..." << endl;
    std::chrono::steady_clock::time_point* startTimes = new std::chrono::steady_clock::time_point[NumConnections];
    std::chrono::steady_clock::time_point* endTimes = new std::chrono::steady_clock::time_point[NumConnections];
    size_t* bytesCompleted = new size_t[NumConnections];

    thread** ioThreads = new thread*[NumConnections];
    for (int i = 0; i != NumConnections; i++) {
        thread* worker = new thread([i, TOTAL_MESSAGE_SIZE, BytesPerConn, QueueDepth, clientSockets, sendBuffers, recvBuffers, isOffloaded, startTimes, endTimes, bytesCompleted]
            {
                ThreadFunc(TOTAL_MESSAGE_SIZE, BytesPerConn, QueueDepth, &clientSockets[i], sendBuffers[i], recvBuffers[i], isOffloaded[i], &startTimes[i], &endTimes[i], &bytesCompleted[i]);
        });
        ioThreads[i] = worker;
    }

    for (int i = 0; i != NumConnections; i++) {
        ioThreads[i]->join();
    }

    //
    // Signal the server to stop receiving
    //
    //
    // for (int i = 0; i != NumConnections; i++) {
    //    MessageHeader* hdr = (MessageHeader*)(sendBuffers[i]);
    //    hdr->LastMessage = true;
    //    send(clientSockets[i], (const char*)sendBuffers[i], sizeof(MessageHeader), 0);
    // }

    cout << "Processing results..." << endl;

    //
    // Calculate throughput
    //
    //
    std::chrono::steady_clock::time_point aggStartTime = startTimes[0], aggEndTime = endTimes[0];
    size_t aggBytesCompleted = bytesCompleted[0];
    for (int i = 1; i != NumConnections; i++) {
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

    cout << "Throughput: " << throughput << " GB/s" << endl;

    //
    // Release buffers
    //
    //
    delete[] oneMessage;
    for (int i = 0; i != NumConnections; i++) {
        delete[] sendBuffers[i];
        delete[] recvBuffers[i];
    }
    delete[] sendBuffers;
    delete[] recvBuffers;

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