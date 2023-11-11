#pragma once
#include <random>
#include <mutex>
#include "../Common/Include/LatencyHelpers.h"


// #define N_CONCURRENT_COMPLETION_THREADS 32
#define FILE_BASE_NAME "random.txt"


HANDLE *fileHandles;
HANDLE ioCompletionPort;

thread **allThreads;

struct ServerContext
{
    OVERLAPPED Overlapped;  // must be first member, will need to cast it to context to find other members
    //atomic_bool IsAvailable;
    uint16_t BatchId;
    atomic_uint16_t *batchCompletionCounts;
    int64_t ReadStartTime;
    int64_t ReadEndTime;
    int64_t SendBackTime;
    // uint16_t Index; // unused
    // the following are per socket data
    SOCKET hSocket;
    mutex *socketMutex;
    char *bufferMem;
    ServerContext* serverContexts;
    int clientIndex;
};

atomic_uint64_t *msgProcessedCount;  // of size [no. connections]
double **readLatencies;  // of size [no. connections][readNum]
double **sendBackLatencies;  // of size [no. connections][readNum]

Statistics *allClientsReadLatencyStats;
Percentiles *allClientsReadLatencyPercentiles;
Statistics *allClientsSendBackStats;
Percentiles *allClientsSendBackPercentiles;


int readNum;  // the value of 4th cmdline param
int RunServer(int msgSize, int batchSize, int queueDepth, int readNum, int fileSize, int nFile, int nConnections, int nCompletionThreads);