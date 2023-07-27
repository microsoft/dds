#include "FrontEnd.h"

FrontEnd::FrontEnd() {
	Buffer = NULL;
}

//
// Allocate the DMA buffer
//
//
char*
FrontEnd::AllocateBuffer(
	const size_t Capacity
) {
	/*
	if (BackEnd.Connect()) {
		Buffer = new DMABuffer(BackEnd.BackEndAddr, BackEnd.BackEndPort, Capacity, BackEnd.ClientId);
		if (Buffer->Allocate(BackEnd.LocalSock, BackEnd.BackEndSock, BackEnd.QueueDepth, BackEnd.MaxSge, BackEnd.InlineThreshold)) {
			return Buffer->BufferAddress;
		}
	}
	*/

	if (BackEnd.ConnectTest()) {
		printf("BackEnd.ConnectTest returns true\n");
	}
	
	return NULL;
}

//
// Deallocate the buffer
//
//
void
FrontEnd::DeallocateBuffer() {
	if (Buffer) {
		Buffer->Release();
	}
}