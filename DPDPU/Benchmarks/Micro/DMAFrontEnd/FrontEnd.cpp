#include "FrontEnd.h"

FrontEnd::FrontEnd() {
	Buffer = NULL;
}

//
// Connect to the back end
//
//
bool
FrontEnd::Connect() {
	return BackEnd.Connect();
}

//
// Disconnect from the back end
//
//
void
FrontEnd::Disconnect() {
	BackEnd.Disconnect();
}

//
// Allocate the DMA buffer
//
//
char*
FrontEnd::AllocateBuffer(
	const size_t Capacity
) {
	Buffer = new DMABuffer(BackEnd.BackEndAddr, BackEnd.BackEndPort, Capacity, BackEnd.ClientId);
	if (Buffer->Allocate(&BackEnd.LocalSock, &BackEnd.BackEndSock, BackEnd.QueueDepth, BackEnd.MaxSge, BackEnd.InlineThreshold)) {
		return Buffer->BufferAddress;
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