#include <cstdlib>
#include <iostream>
#include <thread>

#include "DMABuffer.h"
#include "Evaluation.h"
#include "RingBufferProgressive.h"
#include "Profiler.h"

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
	RequestRingBufferProgressive* RingBuffer,
	Request** Requests,
	size_t NumRequests
) {
	for (size_t r = 0; r != NumRequests; r++) {
		while (InsertToRequestBufferProgressive(RingBuffer, Requests[r]->Data, Requests[r]->Size + sizeof(int)) == false) {
			this_thread::yield();
		}
	}
}

void EvaluateRequestRingBufferProgressiveWithDPU() {
	const size_t entireBufferSpace = 134217728; // 128 MB
	char* Buffer = new char[entireBufferSpace];
	RequestRingBufferProgressive* ringBuffer = NULL;
	const size_t totalProducers = 1;
	const size_t requestsPerProducer = 10000000;
	size_t totalRequests = requestsPerProducer * totalProducers;

	//
	// Allocate a ring buffer
	//
	//
	cout << "Allocating a ring buffer..." << endl;
	memset(Buffer, 0, entireBufferSpace);
	ringBuffer = AllocateRequestBufferProgressive(Buffer);

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

	for (size_t t = 0; t != totalProducers; t++) {
		producerThreads[t] = new thread(
			[t, ringBuffer, allRequests, requestsPerProducer] {
				RequestProducerWithDPU(ringBuffer,
					&allRequests[requestsPerProducer * t],
					requestsPerProducer);
			}
		);
	}

	//
	// Wait for all threads to join
	//
	//
	cout << "Waiting for all threads to complete..." << endl;
	for (size_t t = 0; t != totalProducers; t++) {
		producerThreads[t]->join();
	}

	cout << "Release all of the memory..." << endl;
	DeallocateRequestBufferProgressive(ringBuffer);

	for (size_t r = 0; r != totalRequests; r++) {
		delete allRequests[r];
	}

	for (size_t t = 0; t != totalProducers; t++) {
		delete producerThreads[t];
	}

	delete[] Buffer;
}