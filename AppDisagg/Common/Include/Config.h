// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <chrono>

#define CLIENT_IP "10.1.0.4"//"10.1.0.6"
#define SERVER_IP "10.1.0.4"//"10.1.0.6"
#define SERVER_PORT 3232

#define FILE_DIR "E:\YimingZheng\dpdpu\DPDPU\AppDisagg\x64\Release"

#define SECTOR_SIZE 4096

#define SEND_PACKET_SIZE 1024

using namespace std;
using namespace std::chrono;

struct MessageHeader {
	long long TimeSend;
	uint16_t BatchId;
	uint16_t FileId;
	uint64_t Offset;
	int Length;
};
