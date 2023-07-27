#include <iostream>
#include <string>
#include <string.h>

#include "TCPClient.h"
#include "TCPServer.h"

using namespace std;

int main(
    int argc,
    const char** args
)
{
    if (argc == 7 && strcmp(args[1], "ClientLat") == 0) {
        int msgSize = stoi(args[2]);
        int queueDepth = stoi(args[3]);
        size_t totalBytes = stoull(args[4]);
        int port = stoi(args[5]);
        int offloadPercent = stoi(args[6]);
        RunClientForLatency(msgSize, queueDepth, totalBytes, port, offloadPercent);
    }
    else if (argc == 8 && strcmp(args[1], "ClientBw") == 0) {
        int msgSize = stoi(args[2]);
        int queueDepth = stoi(args[3]);
        size_t totalBytes = stoull(args[4]);
        int port = stoi(args[5]);
        int offloadPercent = stoi(args[6]);
        int numConns = stoi(args[7]);
        RunClientForThroughput(msgSize, queueDepth, totalBytes, port, offloadPercent, numConns);
    }
    else if (argc == 2 && strcmp(args[1], "Server") == 0) {
        RunServer();
    }
    else {
        cout << "Client (latency) usage: " << args[0] << " ClientLat [Msg Size] [Queue Depth] [Total Bytes] [Port] [Offload Percentage]" << endl;
        cout << "Client (bandwidth) usage: " << args[0] << " ClientBw [Msg Size] [Queue Depth] [Total Bytes] [Port Base] [Offload Percentage] [Num Connections]" << endl;
        cout << "Server usage: " << args[0] << " Server" << endl;
    }
}