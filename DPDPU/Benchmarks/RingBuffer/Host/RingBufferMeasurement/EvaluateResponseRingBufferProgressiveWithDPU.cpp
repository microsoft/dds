#include <cstdlib>
#include <iostream>
#include <thread>

#include "BackEndBridge.h"
#include "DMABuffer.h"
#include "Evaluation.h"
#include "RingBufferProgressive.h"
#include "Profiler.h"
#include "Protocol.h"

#define RESPONSE_SIZE_8B 0
#define RESPONSE_SIZE_8KB 1
#define RESPONSE_SIZE RESPONSE_SIZE_8B

#define RESPONSE_VALUE 42
#if RESPONSE_SIZE == RESPONSE_SIZE_8B
#define TOTAL_RESPONSES 10000000
#elif RESPONSE_SIZE == RESPONSE_SIZE_8KB
#define TOTAL_RESPONSES 100000
#endif

#undef CHECK_RESPONSE_CORRECTNESS

using namespace DDS_FrontEnd;
using namespace std;
using std::cout;

static atomic<size_t> TotalReceivedResponses = 0;

static void ResponseConsumer(
	ResponseRingBufferProgressive* RingBuffer
) {
	size_t numReqProcessed = 0;
	char* pagesOfResponses = new char[DDS_RESPONSE_RING_BYTES];
	BufferT responsePointer = NULL;
	FileIOSizeT responseSize = 0;
	BufferT startOfNext = NULL;
	FileIOSizeT remainingSize = 0;

	while (TotalReceivedResponses != TOTAL_RESPONSES) {
		if (FetchFromResponseBufferProgressive(RingBuffer, pagesOfResponses, &remainingSize)) {
			startOfNext = pagesOfResponses;

#ifdef RING_BUFFER_RESPONSE_BATCH_ENABLED
			//
			// There can be many responses
			//
			//
			ParseNextResponseProgressive(
				startOfNext,
				remainingSize,
				&responsePointer,
				&responseSize,
				&startOfNext,
				&remainingSize);

			int numResps = *(int*)responsePointer;
#ifdef CHECK_RESPONSE_CORRECTNESS
			int numInts = 0;
			int* curResp = (int*)responsePointer + 1;

			for (int r = 0; r != numResps; r++) {
				numInts = curResp[0] / (int)sizeof(int);
				for (int i = 1; i != numInts + 1; i++) {
					if (curResp[i] != RESPONSE_VALUE) {
						cout << "[Error] Incorrect response!" << endl;
					}
				}
				curResp = curResp + numInts + 1;
			}
#endif
			TotalReceivedResponses.fetch_add(numResps);
#else
			//
			// There is only one response
			//
			//
#ifdef CHECK_RESPONSE_CORRECTNESS
			ParseNextResponseProgressive(
				startOfNext,
				remainingSize,
				&responsePointer,
				&responseSize,
				&startOfNext,
				&remainingSize);

			int* response = (int*)responsePointer;

			int numInts = response[0] / (int)sizeof(int);
			for (int i = 1; i != numInts + 1; i++) {
				if (response[i] != RESPONSE_VALUE) {
					cout << "[Error] Incorrect response!" << endl;
				}
			}
#endif
			TotalReceivedResponses.fetch_add(1);
#endif
		}
	}

	delete[] pagesOfResponses;
}

void EvaluateResponseRingBufferProgressiveWithDPU() {
	const size_t entireBufferSpace = 1073741824; // 1 GB
	BackEndBridge backEnd;
	if (!backEnd.Connect()) {
		cout << "Failed to connect to the back end" << endl;
		return;
	}
	cout << "Connected to the back end" << endl;
	DMABuffer dmaBuffer(DDS_BACKEND_ADDR, DDS_BACKEND_PORT, entireBufferSpace, backEnd.ClientId);
	if (!dmaBuffer.Allocate(&backEnd.LocalSock, &backEnd.BackEndSock, backEnd.QueueDepth, backEnd.MaxSge, backEnd.InlineThreshold)) {
		cout << "Failed to allocate DMA buffer" << endl;
		return;
	}
	cout << "Allocated DMA buffer" << endl;

	char* buffer = dmaBuffer.BufferAddress;
	ResponseRingBufferProgressive* ringBuffer = NULL;
	const size_t totalConsumers = 64;
	size_t totalResponses = TOTAL_RESPONSES;

	//
	// Allocate a ring buffer
	//
	//
	cout << "Allocating a ring buffer..." << endl;
	ringBuffer = AllocateResponseBufferProgressive(buffer);

	//
	// Start consumers
	//
	//
	cout << "Starting consumers..." << endl;
	thread* consumerThreads[totalConsumers];
	for (size_t t = 0; t != totalConsumers; t++) {
		consumerThreads[t] = new thread(
			[ringBuffer] {
				ResponseConsumer(ringBuffer);
			}
		);
	}

	//
	// Wait for all threads to join
	//
	//
	cout << "Waiting for all threads to complete..." << endl;
	for (size_t t = 0; t != totalConsumers; t++) {
		consumerThreads[t]->join();
	}
	cout << "All responses have been received" << endl;

	for (size_t t = 0; t != totalConsumers; t++) {
		delete consumerThreads[t];
	}

	delete[] buffer;
}