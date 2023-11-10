#pragma once

#include <chrono>

#define CLIENT_IP "127.0.0.1"//"10.1.0.6"
#define SERVER_IP "127.0.0.1"//"10.1.0.6"
#define SERVER_PORT 3232

#define SECTOR_SIZE 4096

using namespace std;
using namespace std::chrono;

struct MessageHeader {
	long long TimeSend;
	uint16_t BatchId;
	uint16_t FileId;
	uint64_t Offset;
	int Length;
};