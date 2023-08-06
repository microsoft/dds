#include "Evaluation.h"

int main() {
    // EvaluateRequestRingBufferNoConcurrency();
    
    // EvaluateRequestRingBufferProgressive();

    // EvaluateRequestRingBufferFaRMStyle();

    // EvaluateRequestRingBufferLockBased();

    // EvaluateRequestRingBufferProgressiveWithDPU();

    // EvaluateRequestRingBufferFaRMStyleWithDPU();

    EvaluateRequestRingBufferProgressiveNotAlignedWithDPU();
    
    return 0;
}