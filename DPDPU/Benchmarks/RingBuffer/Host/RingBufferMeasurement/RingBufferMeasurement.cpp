#include "Evaluation.h"

int main() {
    // EvaluateRequestRingBufferNoConcurrency();
    
    // EvaluateRequestRingBufferProgressive();

    // EvaluateRequestRingBufferFaRMStyle();

    // EvaluateRequestRingBufferLockBased();

    // EvaluateRequestRingBufferProgressiveWithDPU();

    // EvaluateRequestRingBufferProgressiveNotAlignedWithDPU();

    EvaluateRequestRingBufferProgressiveFullyAlignedWithDPU();

    // EvaluateRequestRingBufferFaRMStyleWithDPU();
    
    return 0;
}