#pragma once

#include <chrono>

#define CLIENT_IP "10.1.0.6"
#define SERVER_IP "10.1.0.6"
#define SERVER_PORT 3232

using namespace std;
using namespace std::chrono;

struct MessageHeader {
	bool OffloadedToDPU;
	bool LastMessage;
	int MessageSize;
	//time_point<high_resolution_clock> TimeSend;
	long long TimeSend;
	char FileName[16];
	int Offset;
	int Length;
};