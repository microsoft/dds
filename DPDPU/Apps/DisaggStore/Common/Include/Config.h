#pragma once

#include <chrono>

#define CLIENT_IP "10.1.0.6"
#define SERVER_IP "10.1.0.6"
#define SERVER_PORT 3232

using namespace std;
using namespace std::chrono;

struct MessageHeader {
	long long TimeSend;
	uint16_t BatchId;
	uint16_t FileId;
	int Offset;
	int Length;
};