#include <cstdlib>
#include <iostream>
#include <thread>

#include "Evaluation.h"
#include "RingBufferProgressive.h"
#include "Profiler.h"

#define RESPONSE_VALUE 42
#define TOTAL_RESPONSES 10000000

using namespace DDS_FrontEnd;
using namespace std;
using std::cout;

atomic<size_t> TotalReceivedResponses = 0;

class Response {
public:
	int Size;
	int* Data;
};

class SmallResponse : Response {
public:
	SmallResponse() {
		Size = 8; // 8 B
		Data = new int[3];
	}
	~SmallResponse() {
		delete[] Data;
	}
};

struct MediumResponse : Response {
public:
	MediumResponse() {
		Size = 8192; // 8 KB
		Data = new int[2049];
	}
	~MediumResponse() {
		delete[] Data;
	}
};

struct LargeResponse : Response {
public:
	LargeResponse() {
		Size = 1048576; // 1 MB
		Data = new int[262145];
	}
};

void ResponseProducer(
	ResponseRingBufferProgressive* RingBuffer,
	BufferT* Responses,
	FileIOSizeT* ResponseSizes,
	size_t NumResponses
) {
	int sentResponses = 0;
	int curSentResponses = 0;
	Profiler profiler(NumResponses);

	profiler.Start();
	while (sentResponses != NumResponses) {
		while (InsertToResponseBufferProgressive(RingBuffer, &Responses[sentResponses], &ResponseSizes[sentResponses], NumResponses - sentResponses, &curSentResponses) == false) {
			this_thread::yield();
		}

		sentResponses += curSentResponses;
	}

	while (CheckForResponseCompletionProgressive(RingBuffer) == false) {
		this_thread::yield();
	}
	profiler.Stop();

	cout << "Microbenchmark completed" << endl;
	cout << "-- Ring tail = " << RingBuffer->Tail[0] << endl;
	cout << "-- Ring progress = " << RingBuffer->Progress[0] << endl;
	cout << "-- Ring head = " << RingBuffer->Head[0] << endl;
	profiler.Report();
}

void ResponseConsumer(
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

			/*
			//
			// There is only one response
			//
			//
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
			*/

			TotalReceivedResponses.fetch_add(1);
		}
	}

	delete[] pagesOfResponses;
}

void EvaluateResponseRingBufferProgressive() {
	const size_t entireBufferSpace = 1073741824; // 1 GB
	char* Buffer = new char[entireBufferSpace];
	ResponseRingBufferProgressive* ringBuffer = NULL;
	const size_t totalConsumers = 32;
	size_t totalResponses = TOTAL_RESPONSES;

	//
	// Allocate a ring buffer
	//
	//
	cout << "Allocating a ring buffer..." << endl;
	memset(Buffer, 0, entireBufferSpace);
	ringBuffer = AllocateResponseBufferProgressive(Buffer);

	//
	// Prepare Responses
	//
	//
	cout << "Preparing Responses..." << endl;
	Response** responses = new Response*[totalResponses];
	BufferT* allResponses = new BufferT[totalResponses];
	FileIOSizeT* allResponseSizes = new FileIOSizeT[totalResponses];
	for (size_t r = 0; r != totalResponses; r++) {
		Response* curReq = NULL;
		switch (rand() % 1)
		{
		case 2:
		{
			curReq = (Response*)(new LargeResponse());
			break;
		}
		case 1:
		{
			curReq = (Response*)(new MediumResponse());
			break;
		}
		default:
		{
			curReq = (Response*)(new MediumResponse());
			break;
		}
		}

		int numInts = curReq->Size / (int)sizeof(int);
		curReq->Data[0] = curReq->Size;
		for (int i = 1; i != numInts + 1; i++) {
			curReq->Data[i] = RESPONSE_VALUE;
		}

		responses[r] = curReq;
		allResponses[r] = (BufferT)curReq->Data;
		allResponseSizes[r] = (FileIOSizeT)(curReq->Size + sizeof(int));
	}
	cout << "Responses have been prepared" << endl;

	//
	// Start the producer
	//
	//
	cout << "Starting the producer..." << endl;
	thread* producerThread;
	producerThread = new thread(
		[ringBuffer, allResponses, allResponseSizes, totalResponses] {
			ResponseProducer(ringBuffer, allResponses, allResponseSizes, totalResponses);
		}
	);

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
	producerThread->join();

	cout << "Release all of the memory..." << endl;

	for (size_t r = 0; r != totalResponses; r++) {
		delete responses[r];
	}
	delete[] allResponses;
	delete[] allResponseSizes;

	for (size_t t = 0; t != totalConsumers; t++) {
		delete consumerThreads[t];
	}
	delete producerThread;

	delete[] Buffer;
}