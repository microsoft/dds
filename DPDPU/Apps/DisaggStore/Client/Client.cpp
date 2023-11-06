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

int RunClientForLatency(
    int MessageSize,
    int QueueDepth,
    uint64_t TotalBytes,
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
    //const int TOTAL_MESSAGE_SIZE = sizeof(MessageHeader) + MessageSize;
    const int TOTAL_MESSAGE_SIZE = sizeof(MessageHeader);

    /*char* oneMessage = new char[MessageSize];
    memset(oneMessage, 0, MessageSize);
    *((int*)oneMessage) = 42;*/
    /*char* FileName = new char[1024];
    strcpy(FileName,"random.txt");*/

    char* sendBuffer = new char[TOTAL_MESSAGE_SIZE];
    memset(sendBuffer, 0, TOTAL_MESSAGE_SIZE);

    MessageHeader* hdr = (MessageHeader*)(sendBuffer);
    hdr->OffloadedToDPU = false;
    hdr->LastMessage = false;
    hdr->MessageSize = MessageSize;
    strcpy(hdr->FileName, "random.txt");
    hdr->Offset = 0;
    hdr->Length = MessageSize;
    // memcpy(sendBuffer + sizeof(MessageHeader), oneMessage, MessageSize);

    char* recvBuffer = new char[TOTAL_MESSAGE_SIZE];
    memset(recvBuffer, 0, TOTAL_MESSAGE_SIZE);

    const int TOTAL_MESSAGES = (int)((TotalBytes / TOTAL_MESSAGE_SIZE) + (TotalBytes % TOTAL_MESSAGE_SIZE == 0 ? 0 : 1));
    double* latencies = new double[TOTAL_MESSAGES];
    for (int l = 0; l != TOTAL_MESSAGES; l++) {
        latencies[l] = 0;
    }

    //
    // Prepare offload bits
    //
    //
    srand(RAND_SEED);
    bool* isOffloaded = new bool[TOTAL_MESSAGES];
    for (int o = 0; o != TOTAL_MESSAGES; o++) {
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

    uint64_t bytesCompleted = 0;
    uint64_t bytesSent = 0;

    auto startTime = high_resolution_clock::now().time_since_epoch().count(); // measure start time

    int oneSend = 0;
    int remainingBytesToSend = TOTAL_MESSAGE_SIZE;
    uint64_t msgIndexForOffloading = 0;

    for (uint64_t q = 0; q != QueueDepth; q++) {
        hdr->OffloadedToDPU = isOffloaded[msgIndexForOffloading];
        msgIndexForOffloading++;
        hdr->TimeSend = high_resolution_clock::now().time_since_epoch().count();
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

        bytesSent += oneSend;

        oneSend = 0;
        remainingBytesToSend = TOTAL_MESSAGE_SIZE;
    }

    int oneReceive = 0;
    int remainingBytesToReceive = TOTAL_MESSAGE_SIZE;
    uint64_t msgIndex = 0;

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
        long long diff = high_resolution_clock::now().time_since_epoch().count() - ((MessageHeader*)recvBuffer)->TimeSend;
        //latencies[msgIndex] = diff * nanoseconds::period::num / nanoseconds::period::den;
        //diff returns the time in nanosecond, here we divide it by 100 to get time in microsecond
        latencies[msgIndex] = diff/1000;
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
        if (bytesSent < TotalBytes) {
            hdr->OffloadedToDPU = isOffloaded[msgIndexForOffloading];
            msgIndexForOffloading++;
            hdr->TimeSend = high_resolution_clock::now().time_since_epoch().count();
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

            bytesSent += oneSend;

            oneSend = 0;
            remainingBytesToSend = TOTAL_MESSAGE_SIZE;
        }
    }
    auto endTime = high_resolution_clock::now().time_since_epoch().count(); // measure end time

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
    auto duration = (endTime - startTime)/1000; // duration in microseconds
    double throughput = (double)bytesCompleted / (double)duration * 1000000.0f / (1024.0 * 1024.0 * 1024.0); // in giga bytes per second

    //
    // Calculate latency
    //
    //
    Statistics LatencyStats;
    Percentiles PercentileStats;
    GetStatistics(latencies, TOTAL_MESSAGES, &LatencyStats, &PercentileStats);
    printf(
        "Result for %zu requests of %d bytes (%.2lf seconds): %.2lf RPS, Min: %.2lf, Max: %.2lf, 50th: %.2lf, 90th: %.2lf, 99th: %.2lf, 99.9th: %.2lf, 99.99th: %.2lf, StdErr: %.2lf\n",
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
    // delete[] oneMessage;
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
    const uint64_t BytesPerConn,
    const int QueueDepth,
    SOCKET* ClientSocket,
    char* SendBuffer,
    char* RecvBuffer,
    bool* IsOffloaded,
    std::chrono::steady_clock::time_point* StartTime,
    std::chrono::steady_clock::time_point* EndTime,
    uint64_t* BytesCompleted
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

    uint64_t bytesCompleted = 0;
    uint64_t bytesSent = 0;

    int oneSend = 0;
    int remainingBytesToSend = TOTAL_MESSAGE_SIZE;
    uint64_t msgIndexForOffloading = 0;

    MessageHeader* hdr = (MessageHeader*)(SendBuffer);

    *StartTime = high_resolution_clock::now();

    for (uint64_t q = 0; q != QueueDepth; q++) {
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

        bytesSent += oneSend;

        oneSend = 0;
        remainingBytesToSend = TOTAL_MESSAGE_SIZE;
    }

    int oneReceive = 0;
    int remainingBytesToReceive = TOTAL_MESSAGE_SIZE;
    uint64_t msgIndex = 0;

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
        if (bytesSent < BytesPerConn) {
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

            bytesSent += oneSend;

            oneSend = 0;
            remainingBytesToSend = TOTAL_MESSAGE_SIZE;
        }
    }

    *EndTime = high_resolution_clock::now();
    *BytesCompleted = bytesCompleted;
}

int RunClientForThroughput(
    int MessageSize,
    int QueueDepth,
    uint64_t BytesPerConn,
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

    char** sendBuffers = new char* [NumConnections];
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

    const int TOTAL_MESSAGES = (int)((BytesPerConn / TOTAL_MESSAGE_SIZE) + (BytesPerConn % TOTAL_MESSAGE_SIZE == 0 ? 0 : 1));

    //
    // Prepare offload bits
    //
    //
    srand(RAND_SEED);

    bool** isOffloaded = new bool* [NumConnections];

    for (int i = 0; i != NumConnections; i++) {
        isOffloaded[i] = new bool[TOTAL_MESSAGES];
        for (int o = 0; o != TOTAL_MESSAGES; o++) {
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
    uint64_t* bytesCompleted = new uint64_t[NumConnections];

    thread** ioThreads = new thread * [NumConnections];
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
    uint64_t aggBytesCompleted = bytesCompleted[0];
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
struct FileReadData {
    HANDLE hFile;
    char* buffer;
};

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

int GenerateFile(int MBSize) {
    int fileSizeMB = MBSize;
    size_t fileSizeBytes = fileSizeMB * 1024 * 1024; // actual size
    HANDLE hFile = CreateFile(L"random.txt", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile == INVALID_HANDLE_VALUE) {
        std::cerr << "Error creating the file. Error code: " << GetLastError() << std::endl;
        return 1;
    }

    char buffer[4096]; // Buffer size for writing (adjust as needed)

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
    std::cout << "File with random data created successfully." << std::endl;

    return 0;
}

void CALLBACK FileReadCompletion(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped) {
    cout << "callback function started" << endl;
    FileReadData* readData = reinterpret_cast<FileReadData*>(lpOverlapped->hEvent);
    if (dwErrorCode == 0) {
        cout << "Read " << dwNumberOfBytesTransfered << " bytes from file." << endl;
        // You can access the custom data here:
        // readData->hFile
        // readData->buffer
        //cout << "data: " << readData->buffer << endl;
        CloseHandle(readData->hFile);
        /*free(readData->buffer);
        free(readData->hFile);
        free(readData);*/

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
    if (argc == 6) {
        int msgSize = stoi(args[1]);
        int queueDepth = stoi(args[2]);
        uint64_t totalBytes = stoull(args[3]);
        int port = stoi(args[4]);
        int offloadPercent = stoi(args[5]);
        return RunClientForLatency(msgSize, queueDepth, totalBytes, port, offloadPercent);
    }
    else if (argc == 7) {
        int msgSize = stoi(args[1]);
        int queueDepth = stoi(args[2]);
        uint64_t totalBytes = stoull(args[3]);
        int port = stoi(args[4]);
        int offloadPercent = stoi(args[5]);
        int numConns = stoi(args[6]);
        return RunClientForThroughput(msgSize, queueDepth, totalBytes, port, offloadPercent, numConns);
    }
    else {
        cout << "Client (latency) usage: " << args[0] << " [Msg Size] [Queue Depth] [Total Bytes] [Port] [Offload Percentage]" << endl;
        cout << "Client (bandwidth) usage: " << args[0] << " [Msg Size] [Queue Depth] [Total Bytes] [Port Base] [Offload Percentage] [Num Connections]" << endl;
    }

    return 0;
}