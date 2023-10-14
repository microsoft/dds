#include <winsock2.h>
#include <iostream>
#include <thread>
#include <vector>

#include "Config.h"
#include "Server.h"

#pragma comment(lib,"ws2_32.lib")

using std::cout;
using std::endl;

void ThreadFunc(SOCKET clientSocket) {
    int iResult = 0;

    cout << "Waiting for the first message..." << endl;

    // Receive the first message
    int msgSize = 0;
    iResult = recv(clientSocket, (char*)&msgSize, sizeof(int), 0);
    if (iResult == SOCKET_ERROR || iResult != sizeof(int)) {
        cout << "Error receiving the first message: " << WSAGetLastError() << endl;
        closesocket(clientSocket);
        WSACleanup();
        return;
    }

    cout << "Starting measurement (" << msgSize << "-byte messages)..." << endl;

    char* recvBuffer = new char[msgSize];
    memset(recvBuffer, 0, msgSize);

    int oneReceive = 0;
    int remainingBytesToReceive = msgSize;

    int oneSend = 0;
    int remainingBytesToSend = msgSize;

    bool finished = false;

    while (!finished) {
        // Receive message from client and send response
        while (oneReceive < msgSize) {
            iResult = recv(clientSocket, recvBuffer + oneReceive, remainingBytesToReceive, 0);

            if (iResult == SOCKET_ERROR) {
                cout << "Error receiving message: " << WSAGetLastError() << endl;
                closesocket(clientSocket);
                WSACleanup();
                return;
            }
            else if (iResult == 0) {
                finished = true;
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

        // Check continuation
        // if (((MessageHeader*)recvBuffer)->LastMessage) {
        //    break;
        // }

        while (oneSend < msgSize) {
            iResult = send(clientSocket, recvBuffer + oneSend, remainingBytesToSend, 0);
            if (iResult == SOCKET_ERROR) {
                cout << "Error sending message: " << WSAGetLastError() << endl;
                closesocket(clientSocket);
                WSACleanup();
                return;
            }

            oneSend += iResult;
            remainingBytesToSend = msgSize - oneSend;
        }

        oneSend = 0;
        remainingBytesToSend = msgSize;
    }

    cout << "Measurement completes" << endl;

    // Clear buffer
    delete[] recvBuffer;

    // Close the socket for this client and handle the next connection
    closesocket(clientSocket);
}

int RunServer() {
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

    while (true) {
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

        thread* worker = new thread([clientSocket]
            {
                ThreadFunc(clientSocket);
            });
    }

    // Close the listen socket and cleanup
    closesocket(listenSocket);
    WSACleanup();

    return 0;
}

int main(
    int argc,
    const char** args
)
{
    if (argc == 1) {
        return RunServer();
    }
    else {
        cout << "Server usage: " << args[0] << endl;
    }

    return 0;
}