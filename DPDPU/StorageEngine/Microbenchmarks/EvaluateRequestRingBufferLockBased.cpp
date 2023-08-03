#include <cstdlib>
#include <iostream>
#include <thread>

#include "Evaluation.h"
#include "RingBufferLockBased.h"
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

void RequestProducer(
	RequestRingBufferLock* RingBuffer,
	Request** Requests,
	size_t NumRequests
) {
	for (size_t r = 0; r != NumRequests; r++) {
		while (InsertToRequestBufferLock(RingBuffer, Requests[r]->Data, Requests[r]->Size + sizeof(int)) == false) {
			this_thread::yield();
		}
	}
}

void RequestConsumer(
	RequestRingBufferLock* RingBuffer,
	size_t TotalNumRequests
) {
	size_t numReqProcessed = 0;
	size_t sum = 0;
	char* pagesOfRequests = new char[DDS_REQUEST_RING_BYTES];
	BufferT requestPointer = NULL;
	FileIOSizeT requestSize = 0;
	BufferT startOfNext = NULL;
	FileIOSizeT remainingSize = 0;
	Profiler profiler(TotalNumRequests);
	
	profiler.Start();
	while (numReqProcessed != TotalNumRequests) {
		while(FetchFromRequestBufferLock(RingBuffer, pagesOfRequests, &remainingSize) == false) {
			this_thread::yield();
		}

		startOfNext = pagesOfRequests;

		while (true) {
			ParseNextRequestLock(
				startOfNext,
				remainingSize,
				&requestPointer,
				&requestSize,
				&startOfNext,
				&remainingSize);

			int* request = (int*)requestPointer;

			int numInts = request[0] / (int)sizeof(int);
			for (int i = 1; i != numInts + 1; i++) {
				sum += request[i];
			}

			numReqProcessed++;

			if (remainingSize == 0) {
				break;
			}
		}
	}

	profiler.Stop();

	delete[] pagesOfRequests;

	cout << "Microbenchmark completed" << endl;
	cout << "-- Result: sum = " << sum << endl;
	cout << "-- Ring tail = " << RingBuffer->Tail << endl;
	cout << "-- Ring head = " << RingBuffer->Head << endl;
	profiler.Report();
}

void EvaluateRequestRingBufferLock() {
	const size_t entireBufferSpace = 134217728; // 128 MB
	char* Buffer = new char[entireBufferSpace];
	RequestRingBufferLock* ringBuffer = NULL;
	const size_t totalProducers = 64;
	const size_t requestsPerProducer = 10000000;
	size_t totalRequests = requestsPerProducer * totalProducers;

	//
	// Allocate a ring buffer
	//
	//
	cout << "Allocating a ring buffer..." << endl;
	memset(Buffer, 0, entireBufferSpace);
	ringBuffer = AllocateRequestBufferLock(Buffer);

	//
	// Prepare requests
	//
	//
	cout << "Preparing requests..." << endl;
	unsigned int randomSeed = 0;
	srand(randomSeed);
	Request** allRequests = new Request*[totalRequests];
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
				RequestProducer(ringBuffer,
					&allRequests[requestsPerProducer * t],
					requestsPerProducer);
			}
		);
	}

	//
	// Start the consumer
	//
	//
	cout << "Starting the consumer..." << endl;
	thread* consumerThread = new thread(
		[ringBuffer, totalRequests] {
			RequestConsumer(ringBuffer, totalRequests);
		}
	);

	//
	// Wait for all threads to join
	//
	//
	cout << "Waiting for all threads to complete..." << endl;
	for (size_t t = 0; t != totalProducers; t++) {
		producerThreads[t]->join();
	}
	consumerThread->join();

	cout << "Release all of the memory..." << endl;
	DeallocateRequestBufferLock(ringBuffer);

	for (size_t r = 0; r != totalRequests; r++) {
		delete allRequests[r];
	}

	for (size_t t = 0; t != totalProducers; t++) {
		delete producerThreads[t];
	}
	delete consumerThread;

	delete[] Buffer;
}