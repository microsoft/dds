#pragma once

void EvaluateRequestRingBufferNoConcurrency();
void EvaluateRequestRingBufferProgressive();
void EvaluateRequestRingBufferFaRMStyle();
void EvaluateRequestRingBufferLockBased();

void EvaluateRequestRingBufferProgressiveWithDPU();
void EvaluateRequestRingBufferFaRMStyleWithDPU();
void EvaluateRequestRingBufferProgressiveNotAlignedWithDPU();