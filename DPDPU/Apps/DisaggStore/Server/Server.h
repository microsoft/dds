#pragma once
#include <random>
#include <mutex>


#define N_CONCURRENT_COMPLETION_THREADS 32
#define FILE_BASE_NAME "random.txt"


HANDLE *fileHandles;
HANDLE ioCompletionPort;

struct ServerContext
{
    OVERLAPPED Overlapped;  // must be first member, will need to cast it to context to find other members
    //atomic_bool IsAvailable;
    // index into preallocated buffer mem
    uint16_t BatchId;
    atomic_uint16_t *batchCompletionCounts;
    // uint16_t Index; // unused
    SOCKET hSocket;
    mutex *socketMutex;
    char *bufferMem;
    ServerContext* serverContexts;
};


int RunServer(int msgSize, int batchSize, int queueDepth, int fileSize, int nFile);