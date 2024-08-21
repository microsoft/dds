#pragma once

int RunClientForLatency(
    int MessageSize,
    int QueueDepth,
    size_t TotalBytes,
    int Port,
    int OffloadPercent
);

int RunClientForThroughput(
    int MessageSize,
    int QueueDepth,
    size_t TotalBytes,
    int PortBase,
    int OffloadPercent,
    int NumConnections
);