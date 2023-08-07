#pragma once

void EvaluateRequestRingBufferNoConcurrency();
void EvaluateRequestRingBufferProgressive();
void EvaluateRequestRingBufferFaRMStyle();
void EvaluateRequestRingBufferLockBased();

void EvaluateRequestRingBufferProgressiveWithDPU();
void EvaluateRequestRingBufferProgressiveNotAlignedWithDPU();
void EvaluateRequestRingBufferProgressiveFullyAlignedWithDPU();
void EvaluateRequestRingBufferFaRMStyleWithDPU();
void EvaluateRequestRingBufferLockBasedWithDPU();