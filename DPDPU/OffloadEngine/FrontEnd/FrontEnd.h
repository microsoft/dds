#pragma once

#include "BackEndBridge.h"
#include "DMABuffer.h"

//
// The front end of the offload engine
//
//
class FrontEnd {
private:
	BackEndBridge BackEnd;
	DMABuffer* Buffer;

public:
	FrontEnd();

	//
	// Allocate the DMA buffer
	//
	//
	char*
	AllocateBuffer(
		const size_t Capacity
	);

	//
	// Deallocate the buffer
	//
	//
	void
	DeallocateBuffer();
};