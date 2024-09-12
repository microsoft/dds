/*
 * Copyright (c) Microsoft Corporation.
 * Licensed under the MIT License
 */

#pragma once

#include <random>
#include <mutex>
#include <winsock2.h>
#include <iostream>
#include <thread>
#include <vector>
#include <cstdlib>
#include <string>
#include "../Common/Include/Config.h"
#include "../Common/Include/LatencyHelpers.h"

// DDSApplication includes
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>

#include "DDSFrontEnd.h"
#include "Profiler.h"


// #define N_CONCURRENT_COMPLETION_THREADS 32
#define FILE_BASE_NAME "random.txt"

// assuming only 1 file for now
// HANDLE* fileHandles;
HANDLE ioCompletionPort;
FileIdT *fileIds;

thread** allClientThreads;
thread* pollWaitThread;

struct ServerContext
{
    union {
        FileIOSizeT BytesRead;
        FileIOSizeT BytesWritten;
    };
    //FileIOSizeT BytesRead;
    //atomic_bool IsAvailable;
    uint16_t BatchId;
    atomic_uint16_t* batchCompletionCounts;
    int64_t ReadStartTime;
    int64_t ReadEndTime;
    int64_t SendBackTime;
    // uint16_t Index; // unused
    // the following are per socket data
    SOCKET hSocket;
    mutex* socketMutex;
    char* bufferMem;
    ServerContext* serverContexts;
    int clientIndex;
};

atomic_uint64_t* msgProcessedCount;  // of size [no. connections]
double** readLatencies;  // of size [no. connections][readNum]
double** sendBackLatencies;  // of size [no. connections][readNum]

Statistics* allClientsReadLatencyStats;
Percentiles* allClientsReadLatencyPercentiles;
Statistics* allClientsSendBackStats;
Percentiles* allClientsSendBackPercentiles;

atomic_bool doWork = true;

atomic_uint64_t outstandingCount = 0;
atomic_uint64_t sendCount = 0;

int readNum;  // the value of 4th cmdline param
int RunServer(int msgSize, int batchSize, int queueDepth, int readNum, int fileSize, int nFile, int nConnections, int nCompletionThreads);