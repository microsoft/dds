// LatencyDistribution.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "LatencyHelpers.h"

using namespace std;

inline size_t TimeToUS(string Time) {
    // time format: 17:01:12.290212
    size_t hourUs = stoi(Time.substr(0, 2).c_str()) * 3600ULL * 1000000;
    size_t minuteUs = stoi(Time.substr(3, 2).c_str()) * 60ULL * 1000000;
    size_t secondUs = stoi(Time.substr(6, 2).c_str()) * 1000000ULL;
    size_t Us = stoi(Time.substr(9, 6).c_str());
    return hourUs + minuteUs + secondUs + Us;
}

int AnalyzeComputeServer() {
    // const char* DUMP_FILE_PATH = "C:\\Socrates\\Benchmark\\GetRemotePage\\NetShCapture2.txt";
    const char* DUMP_FILE_PATH = "C:\\Socrates\\Benchmark\\GetRemotePage\\NetShCapture30-001-ComputeServer.txt";

    ifstream dumpFile(DUMP_FILE_PATH);
    if (!dumpFile.is_open())
    {
        std::cout << "Failed to open input file" << endl;
        return -1;
    }

    string line;
    string filter = " IP ";
    string requestMsg = " length 256";
    string firstResponseMsg = " length 1460"; // first packet
    string lastResponseMsg = " length 1052"; // last packet
    size_t count = 0;
    size_t lineCount = 0;
    bool requestIssued = false;
    bool firstPacketReceived = false;
    size_t timeRequest;
    size_t timeFirstResponse;
    size_t timeLastResponse;
    vector<int> LastPacketLatencies;
    vector<int> PageNetworkLatencies;

    while (getline(dumpFile, line))
    {
        lineCount++;
        if (line.find(filter) != string::npos) {
            if (line.find(requestMsg) != string::npos) {
                // this is a request
                requestIssued = true;
                timeRequest = TimeToUS(line.substr(0, 15));
            }
            else if (requestIssued) {
                if (line.find(firstResponseMsg) == string::npos)
                    continue;
                requestIssued = false;
                firstPacketReceived = true;
                timeFirstResponse = TimeToUS(line.substr(0, 15));
            }
            else if (firstPacketReceived) {
                if (line.find(lastResponseMsg) == string::npos)
                    continue;
                firstPacketReceived = false;
                timeLastResponse = TimeToUS(line.substr(0, 15));
                LastPacketLatencies.push_back((int)(timeLastResponse - timeRequest));
                PageNetworkLatencies.push_back((int)(timeLastResponse - timeFirstResponse));
                count++;
            }
        }
    }

    Statistics LatencyStats;
    Percentiles PercentileStats;

    GetStatistics(LastPacketLatencies, &LatencyStats, &PercentileStats);

    printf(
        "Result for %llu requests (time between request packet and last response packet): Min: %d, Max: %d, 50th: %.0f, 90th: %.0f, 99th: %.0f, 99.9th: %.0f, 99.99th: %.0f, StdErr: %f\n",
        count,
        LatencyStats.Min,
        LatencyStats.Max,
        PercentileStats.P50,
        PercentileStats.P90,
        PercentileStats.P99,
        PercentileStats.P99p9,
        PercentileStats.P99p99,
        LatencyStats.StandardError);

    GetStatistics(PageNetworkLatencies, &LatencyStats, &PercentileStats);

    printf(
        "Result for %llu network (time between first response packet and last response packet): Min: %d, Max: %d, 50th: %.0f, 90th: %.0f, 99th: %.0f, 99.9th: %.0f, 99.99th: %.0f, StdErr: %f\n",
        count,
        LatencyStats.Min,
        LatencyStats.Max,
        PercentileStats.P50,
        PercentileStats.P90,
        PercentileStats.P99,
        PercentileStats.P99p9,
        PercentileStats.P99p99,
        LatencyStats.StandardError);

    dumpFile.close();
}

int AnalyzePageServer() {
    const char* DUMP_FILE_PATH = "C:\\Socrates\\Benchmark\\GetRemotePage\\NetShCapture30-001-PageServer.txt";

    ifstream dumpFile(DUMP_FILE_PATH);
    if (!dumpFile.is_open())
    {
        std::cout << "Failed to open input file" << endl;
        return -1;
    }

    string line;
    string filter = " IP ";
    string requestMsg = " length 256";
    string clientPrefix = "DMX-133";
    string serverPrefix = " bad-len 0";
    size_t count = 0;
    size_t lineCount = 0;
    bool requestIssued = false;
    size_t timeRequest;
    size_t timeResponse;
    size_t lineRequest;
    size_t lineResponse;
    vector<int> latencies;

    while (getline(dumpFile, line))
    {
        lineCount++;
        if (line.find(filter) != string::npos) {
            if (line.find(requestMsg) != string::npos && line.find(clientPrefix) != string::npos) {
                // this is a request
                lineRequest = lineCount;
                requestIssued = true;
                timeRequest = TimeToUS(line.substr(0, 15));
            }
            else if (requestIssued) {
                if (line.find(serverPrefix) == string::npos)
                    continue;
                lineResponse = lineCount;
                requestIssued = false;
                timeResponse = TimeToUS(line.substr(0, 15));
                latencies.push_back((int)(timeResponse - timeRequest));
                count++;
            }
        }
    }

    Statistics LatencyStats;
    Percentiles PercentileStats;

    GetStatistics(latencies, &LatencyStats, &PercentileStats);

    printf(
        "Result for %llu requests: Min: %d, Max: %d, 50th: %.0f, 90th: %.0f, 99th: %.0f, 99.9th: %.0f, 99.99th: %.0f, StdErr: %f\n",
        count,
        LatencyStats.Min,
        LatencyStats.Max,
        PercentileStats.P50,
        PercentileStats.P90,
        PercentileStats.P99,
        PercentileStats.P99p9,
        PercentileStats.P99p99,
        LatencyStats.StandardError);

    dumpFile.close();
}

int main()
{
    return AnalyzeComputeServer();
    // return AnalyzePageServer();
}