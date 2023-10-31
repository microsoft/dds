#pragma once

#include <chrono>

#define CLIENT_IP "172.16.1.4"
#define SERVER_IP "172.16.1.4"
#define SERVER_PORT 3232

using namespace std;
using namespace std::chrono;

struct MessageHeader {
	bool OffloadedToDPU;
	bool LastMessage;
	int MessageSize;
	time_point<high_resolution_clock> TimeSend;
};