#pragma once

#include <mutex>
#include <iostream>

#include "DDSFrontEndInterface.h"

#define RING_BUFFER_REQUEST_HEADER_SIZE DDS_CACHE_LINE_SIZE

namespace DDS_FrontEnd {

using Mutex = std::mutex;

//
// A Lock-like ring buffer
// No cache line alignment
//
//
struct RequestRingBufferLock{
	Mutex* RingLock;
	int Tail;
	int Head;
	char Buffer[DDS_REQUEST_RING_BYTES];
};

//
// Allocate a request buffer object
//
//
RequestRingBufferLock*
AllocateRequestBufferLock(
	BufferT BufferAddress
) {
	RequestRingBufferLock* ringBuffer = (RequestRingBufferLock*)BufferAddress;

	memset(ringBuffer, 0, sizeof(RequestRingBufferLock));
	ringBuffer->RingLock = new Mutex();

	return ringBuffer;
}

//
// Deallocate a request buffer object
//
//
void
DeallocateRequestBufferLock(
	RequestRingBufferLock* RingBuffer
) {
	delete RingBuffer->RingLock;
	memset(RingBuffer, 0, sizeof(RequestRingBufferLock));
}

//
// Insert a request into the request buffer
//
//
bool
InsertToRequestBufferLock(
	RequestRingBufferLock* RingBuffer,
	const BufferT CopyFrom,
	FileIOSizeT RequestSize
) {
	RingBuffer->RingLock->lock();

	int tail = RingBuffer->Tail;
	int head = RingBuffer->Head;
	RingSizeT distance = 0;

	if (tail < head) {
		distance = tail + DDS_REQUEST_RING_BYTES - head;
	}
	else {
		distance = tail - head;
	}

	FileIOSizeT requestBytes = sizeof(FileIOSizeT) + RequestSize;
	while (requestBytes % RING_BUFFER_REQUEST_HEADER_SIZE != 0) {
		requestBytes++;
	}

	//
	// Check space
	//
	//
	if (requestBytes > DDS_REQUEST_RING_BYTES - distance) {
		RingBuffer->RingLock->unlock();
		return false;
	}

	RingBuffer->Tail = (tail + requestBytes) % DDS_REQUEST_RING_BYTES;

	if (tail + requestBytes <= DDS_REQUEST_RING_BYTES) {
		char* requestAddress = &RingBuffer->Buffer[tail];

		//
		// Write the number of bytes in this request
		//
		//
		*((FileIOSizeT*)requestAddress) = requestBytes;

		//
		// Write the request
		//
		//
		memcpy(requestAddress + sizeof(FileIOSizeT), CopyFrom, RequestSize);
	}
	else {
		//
		// We need to wrap the buffer around
		//
		//
		RingSizeT remainingBytes = DDS_REQUEST_RING_BYTES - tail - sizeof(FileIOSizeT);
		char* requestAddress1 = &RingBuffer->Buffer[tail];
		char* requestAddress2 = &RingBuffer->Buffer[0];

		//
		// Write the number of bytes in this request
		//
		//
		*((FileIOSizeT*)requestAddress1) = requestBytes;

		//
		// Write the request to two memory locations
		//
		//
		if (RequestSize <= remainingBytes) {
			memcpy(requestAddress1 + sizeof(FileIOSizeT), CopyFrom, RequestSize);
		}
		else {
			memcpy(requestAddress1 + sizeof(FileIOSizeT), CopyFrom, remainingBytes);
			memcpy(requestAddress2, (const char*)CopyFrom + remainingBytes, RequestSize - remainingBytes);
		}
	}

	RingBuffer->RingLock->unlock();

	return true;
}

//
// Fetch requests from the request buffer
//
//
bool
FetchFromRequestBufferLock(
	RequestRingBufferLock* RingBuffer,
	BufferT CopyTo,
	FileIOSizeT* RequestSize
) {

	RingBuffer->RingLock->lock();

	int head = RingBuffer->Head;
	int tail = RingBuffer->Tail;
	
	//
	// Check if there are requests
	//
	//
	if (tail == head) {
		RingBuffer->RingLock->unlock();
		return false;
	}

	RingSizeT availBytes = 0;
	char* sourceBuffer1 = nullptr;
	char* sourceBuffer2 = nullptr;

	if (tail > head) {
		availBytes = tail - head;
		*RequestSize = availBytes;
		sourceBuffer1 = &RingBuffer->Buffer[head];
	}
	else {
		availBytes = DDS_REQUEST_RING_BYTES - head;
		*RequestSize = availBytes + tail;
		sourceBuffer1 = &RingBuffer->Buffer[head];
		sourceBuffer2 = &RingBuffer->Buffer[0];
	}

	memcpy(CopyTo, sourceBuffer1, availBytes);
	memset(sourceBuffer1, 0, availBytes);

	if (sourceBuffer2) {
		memcpy((char*)CopyTo + availBytes, sourceBuffer2, tail);
		memset(sourceBuffer2, 0, tail);
	}

	RingBuffer->Head = tail;

	RingBuffer->RingLock->unlock();

	return true;
}

//
// Parse a request from copied data
// Note: RequestSize is greater than the actual request size and is request header aligned
//
//
void
ParseNextRequestLock(
	BufferT CopyTo,
	FileIOSizeT TotalSize,
	BufferT* RequestPointer,
	FileIOSizeT* RequestSize,
	BufferT* StartOfNext,
	FileIOSizeT* RemainingSize
) {
	char* bufferAddress = (char*)CopyTo;
	FileIOSizeT totalBytes = *(FileIOSizeT*)bufferAddress;

	*RequestPointer = (BufferT)(bufferAddress + sizeof(FileIOSizeT));
	*RequestSize = totalBytes - sizeof(FileIOSizeT);
	*RemainingSize = TotalSize - totalBytes;

	if (*RemainingSize > 0) {
		*StartOfNext = (BufferT)(bufferAddress + totalBytes);
	}
	else {
		*StartOfNext = nullptr;
	}
}

}