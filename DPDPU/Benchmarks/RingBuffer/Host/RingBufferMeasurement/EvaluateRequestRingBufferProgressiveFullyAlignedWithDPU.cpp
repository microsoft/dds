#include <cstdlib>
#include <iostream>
#include <thread>

#include "BackEndBridge.h"
#include "DMABuffer.h"
#include "Evaluation.h"
#include "RingBufferProgressiveFullyAligned.h"
#include "Profiler.h"
#include "Protocol.h"

using namespace DDS_FrontEnd;
using namespace std;
using std::cout;

class Request {
public:
	int Size;
	int* Data;
};

class SmallRequest : Request {
public:
	SmallRequest() {
		Size = 8; // 8 B
		Data = new int[3];
	}
	~SmallRequest() {
		delete[] Data;
	}
};

struct MediumRequest : Request {
public:
	MediumRequest() {
		Size = 8192; // 8 KB
		Data = new int[2049];
	}
	~MediumRequest() {
		delete[] Data;
	}
};

struct LargeRequest : Request {
public:
	LargeRequest() {
		Size = 1048576; // 1 MB
		Data = new int[262145];
	}
};

void RequestProducerWithDPU(
	RequestRingBufferProgressiveFullyAligned* RingBuffer,
	Request** Requests,
	size_t NumRequests,
	double* Throughput
) {
	Profiler profiler(NumRequests);

	profiler.Start();
	for (size_t r = 0; r != NumRequests; r++) {
		while (InsertToRequestBufferProgressiveFullyAligned(RingBuffer, Requests[r]->Data, Requests[r]->Size + sizeof(int)) == false) {
			this_thread::yield();
		}
	}
	while (!CheckForCompletionProgressiveFullyAligned(RingBuffer)) {
		this_thread::yield();
	}
	profiler.Stop();
	cout << "Microbenchmark completed" << endl;
	profiler.ReportThroughput();
	*Throughput = NumRequests / profiler.Ellapsed();
}

void EvaluateRequestRingBufferProgressiveFullyAlignedWithDPU() {
	const size_t entireBufferSpace = 134217728; // 128 MB
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
	RequestRingBufferProgressiveFullyAligned* ringBuffer = NULL;
	const size_t totalProducers = 64;
	const size_t requestsPerProducer = 1000000;
	size_t totalRequests = requestsPerProducer * totalProducers;

	//
	// Allocate a ring buffer
	//
	//
	cout << "Allocating a ring buffer..." << endl;
	ringBuffer = AllocateRequestBufferProgressiveFullyAligned(buffer);

	//
	// Prepare requests
	//
	//
	cout << "Preparing requests..." << endl;
	unsigned int randomSeed = 0;
	srand(randomSeed);
	Request** allRequests = new Request * [totalRequests];
	size_t sum = 0;
	for (size_t r = 0; r != totalRequests; r++) {
		Request* curReq = NULL;
		switch (rand() % 1)
		{
		case 2:
		{
			curReq = (Request*)(new LargeRequest());
			break;
		}
		case 1:
		{
			curReq = (Request*)(new MediumRequest());
			break;
		}
		default:
		{
			curReq = (Request*)(new SmallRequest());
			break;
		}
		}

		int numInts = curReq->Size / (int)sizeof(int);
		curReq->Data[0] = curReq->Size;
		for (int i = 1; i != numInts + 1; i++) {
			curReq->Data[i] = rand();
			sum += curReq->Data[i];
		}

		allRequests[r] = curReq;
	}
	cout << "Requests have been prepared: sum = " << sum << endl;

	//
	// Start producers
	//
	//
	cout << "Starting producers..." << endl;
	thread* producerThreads[totalProducers];
	double throughputs[totalProducers];
	memset(throughputs, 0, sizeof(double) * totalProducers);
	double totalThroughput = 0;

	for (size_t t = 0; t != totalProducers; t++) {
		double* curThr = &throughputs[t];
		producerThreads[t] = new thread(
			[t, ringBuffer, allRequests, requestsPerProducer, curThr] {
				RequestProducerWithDPU(ringBuffer,
					&allRequests[requestsPerProducer * t],
					requestsPerProducer, curThr);
			}
		);
	}

	//
	// Wait for all threads to join
	//
	//
	for (size_t t = 0; t != totalProducers; t++) {
		producerThreads[t]->join();
		totalThroughput += throughputs[t];
	}

	cout << "Total throughput = " << totalThroughput << " MOPS" << endl;
	cout << "Release all of the memory..." << endl;
	dmaBuffer.Release();
	backEnd.Disconnect();

	for (size_t r = 0; r != totalRequests; r++) {
		delete allRequests[r];
	}

	for (size_t t = 0; t != totalProducers; t++) {
		delete producerThreads[t];
	}
}