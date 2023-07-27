#pragma once

#include <chrono>

#define CLIENT_IP "192.168.200.133"
#define CLIENT_PORT_BASE 3333
#define SERVER_IP "192.168.200.132"
#define SERVER_PORT 3232

using namespace std;
using namespace std::chrono;

struct MessageHeader {
	bool OffloadedToDPU;
	bool LastMessage;
	int MessageSize;
	time_point<high_resolution_clock> TimeSend;
};