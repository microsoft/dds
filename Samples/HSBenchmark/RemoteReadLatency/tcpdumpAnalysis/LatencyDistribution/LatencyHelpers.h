#pragma once

/*++
    Copyright (c) Microsoft Corporation.
    Licensed under the MIT License.
Abstract:
    Latency math helpers.
--*/

#include <algorithm>
#include <math.h>
#include <vector>

using std::vector;

struct Statistics {
    double Mean{ 0 };
    double Variance{ 0 };
    double StandardDeviation{ 0 };
    double StandardError{ 0 };
    int Min{ 0 };
    int Max{ 0 };
};

struct Percentiles {
    double P50{ 0 };
    double P90{ 0 };
    double P99{ 0 };
    double P99p9{ 0 };
    double P99p99{ 0 };
    double P99p999{ 0 };
    double P99p9999{ 0 };
};

static
double
ComputeVariance(
    vector<int> Measurements,
    double Mean
)
{
    if (Measurements.size() <= 1) {
        return 0;
    }
    double Variance = 0;
    for (size_t i = 0; i < Measurements.size(); i++) {
        uint32_t Value = Measurements[i];
        Variance += (Value - Mean) * (Value - Mean) / (Measurements.size() - 1);
    }
    return Variance;
}

static
void
GetStatistics(
    vector<int> Data,
    Statistics* AllStatistics,
    Percentiles* PercentileStats
)
{
    if (Data.size() == 0) {
        return;
    }

    uint64_t Sum = 0;
    int Min = INT_MAX;
    int Max = 0;
    for (size_t i = 0; i < Data.size(); i++) {
        uint32_t Value = Data[i];
        Sum += Value;
        if (Value > Max) {
            Max = Value;
        }
        if (Value < Min) {
            Min = Value;
        }
    }
    double Mean = Sum / (double)Data.size();
    double Variance =
        ComputeVariance(
            Data,
            Mean);
    double StandardDeviation = sqrt(Variance);
    double StandardError = StandardDeviation / sqrt((double)Data.size());
    *AllStatistics = Statistics{
        Mean,
        Variance,
        StandardDeviation,
        StandardError,
        Min,
        Max
    };

    std::sort(Data.begin(), Data.end());

    uint32_t PercentileIndex = (uint32_t)(Data.size() * 0.5);
    PercentileStats->P50 = Data[PercentileIndex];

    PercentileIndex = (uint32_t)(Data.size() * 0.9);
    PercentileStats->P90 = Data[PercentileIndex];

    PercentileIndex = (uint32_t)(Data.size() * 0.99);
    PercentileStats->P99 = Data[PercentileIndex];

    PercentileIndex = (uint32_t)(Data.size() * 0.999);
    PercentileStats->P99p9 = Data[PercentileIndex];

    PercentileIndex = (uint32_t)(Data.size() * 0.9999);
    PercentileStats->P99p99 = Data[PercentileIndex];

    PercentileIndex = (uint32_t)(Data.size() * 0.99999);
    PercentileStats->P99p999 = Data[PercentileIndex];

    PercentileIndex = (uint32_t)(Data.size() * 0.999999);
    PercentileStats->P99p9999 = Data[PercentileIndex];
}