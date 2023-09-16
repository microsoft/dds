#include "DMABuffer.h"

DMABuffer::DMABuffer(
	const char* _BackEndAddr,
	const unsigned short _BackEndPort,
	const size_t _Capacity,
	const int _ClientId
) {
	//
	// Record buffer capacity
	//
	//
	Capacity = _Capacity;
	ClientId = _ClientId;
	BufferId = -1;

	//
	// Initialize NDSPI variables
	//
	//
	Adapter = NULL;
	AdapterFileHandle = NULL;
	memset(&Ov, 0, sizeof(Ov));
	CompQ = NULL;
	QPair = NULL;
	MemRegion = NULL;
	MemWindow = NULL;
	MsgSgl = NULL;
	memset(MsgBuf, 0, BUFF_MSG_SIZE);

	BufferAddress = NULL;
}

//
// Allocate the buffer with the specified capacity
// and register it to the NIC;
// Not thread-safe
//
//
bool
DMABuffer::Allocate(
	struct sockaddr_in* LocalSock,
	struct sockaddr_in* BackEndSock,
	const size_t QueueDepth,
	const size_t MaxSge,
	const size_t InlineThreshold
) {
	//
	// Set up RDMA with NDSPI
	//
	//
	Ov.hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (Ov.hEvent == nullptr) {
		printf("DMABuffer: failed to allocate event for overlapped operations\n");
		return false;
	}

	RDMC_OpenAdapter(&Adapter, LocalSock, &AdapterFileHandle, &Ov);
	RDMC_CreateConnector(Adapter, AdapterFileHandle, &Connector);
	RDMC_CreateCQ(Adapter, AdapterFileHandle, (DWORD)QueueDepth, &CompQ);
	RDMC_CreateQueuePair(Adapter, CompQ, (DWORD)QueueDepth, (DWORD)MaxSge, (DWORD)InlineThreshold, &QPair);

	//
	// Create and register a memory buffer and an additional buffer for messages
	//
	//
	BufferAddress = reinterpret_cast<char*>(HeapAlloc(GetProcessHeap(), 0, Capacity));
	if (BufferAddress == nullptr) {
		printf("DMABuffer: failed to allocate a buffer of %llu bytes\n", Capacity);
		return false;
	}
	memset(BufferAddress, 0, Capacity);
	
	RDMC_CreateMR(Adapter, AdapterFileHandle, &MemRegion);
	unsigned long flags = ND_MR_FLAG_ALLOW_LOCAL_WRITE | ND_MR_FLAG_ALLOW_REMOTE_READ | ND_MR_FLAG_ALLOW_REMOTE_WRITE;
	RDMC_RegisterDataBuffer(MemRegion, BufferAddress, (DWORD)Capacity, flags, &Ov);

	RDMC_CreateMR(Adapter, AdapterFileHandle, &MsgMemRegion);
	RDMC_RegisterDataBuffer(MsgMemRegion, MsgBuf, BUFF_MSG_SIZE, flags, &Ov);

	//
	// Connect to the back end
	//
	//
	uint8_t privData = BUFF_CONN_PRIV_DATA;
	RDMC_Connect(Connector, QPair, &Ov, *LocalSock, *BackEndSock, 0, (DWORD)QueueDepth, &privData, sizeof(privData));
	RDMC_CompleteConnect(Connector, &Ov);

	//
	// Create memory window
	// Send buffer address and token to the back end
	// Wait for response
	//
	//
	RDMC_CreateMW(Adapter, &MemWindow);
	RDMC_Bind(QPair, MemRegion, MemWindow, BufferAddress, (DWORD)Capacity, ND_OP_FLAG_ALLOW_READ | ND_OP_FLAG_ALLOW_WRITE, CompQ, &Ov);

	MsgSgl = new ND2_SGE[1];
	MsgSgl[0].Buffer = MsgBuf;
	MsgSgl[0].BufferLength = BUFF_MSG_SIZE;
	MsgSgl[0].MemoryRegionToken = MsgMemRegion->GetLocalToken();

	((MsgHeader*)MsgBuf)->MsgId = BUFF_MSG_F2B_REQUEST_ID;
	BuffMsgF2BRequestId* msg = (BuffMsgF2BRequestId*)(MsgBuf + sizeof(MsgHeader));
	msg->ClientId = ClientId;
	msg->BufferAddress = (uint64_t)BufferAddress;
	msg->Capacity = (uint32_t)Capacity;
	
	//
	// NOTE: translating the token from host encoding to network encoding is necessary for the Linux back end
	//
	//
	msg->AccessToken = htonl(MemWindow->GetRemoteToken());
	MsgSgl->BufferLength = sizeof(MsgHeader) + sizeof(BuffMsgF2BRequestId);

	RDMC_Send(QPair, MsgSgl, 1, 0, MSG_CTXT);
	RDMC_WaitForCompletionAndCheckContext(CompQ, &Ov, MSG_CTXT, false);

	MsgSgl->BufferLength = BUFF_MSG_SIZE;
	RDMC_PostReceive(QPair, MsgSgl, 1, MSG_CTXT);
	RDMC_WaitForCompletionAndCheckContext(CompQ, &Ov, MSG_CTXT, false);

	if (((MsgHeader*)MsgBuf)->MsgId == BUFF_MSG_B2F_RESPOND_ID) {
		BufferId = ((BuffMsgB2FRespondId*)(MsgBuf + sizeof(MsgHeader)))->BufferId;
		printf("DMABuffer: connected to the back end with assigned buffer id (%d)\n", BufferId);
	}
	else {
		printf("DMABuffer: wrong message from the back end\n");
		return false;
	}

#if DDS_NOTIFICATION_METHOD == DDS_NOTIFICATION_METHOD_INTERRUPT
	//
	// Post receives to allow backend to write responses
	//
	//
	for (int i = 0; i != DDS_MAX_COMPLETION_BUFFERING; i++) {
		RDMC_PostReceive(QPair, MsgSgl, 1, MSG_CTXT);
	}
#endif

	//
	// This buffer is RDMA-accessible from DPU now
	//
	//
	return true;
}

//
// Wait for a completion event
// Not thread-safe
//
//
void
DMABuffer::WaitForACompletion(
	bool Blocking
) {
	RDMC_WaitForCompletionAndCheckContext(CompQ, &Ov, MSG_CTXT, Blocking);
	RDMC_PostReceive(QPair, MsgSgl, 1, MSG_CTXT);
}


//
// Release the allocated buffer;
// Not thread-safe
//
//
void
DMABuffer::Release() {
	//
	// Send the exit message to the back end
	//
	//
	if (BufferId >= 0) {
		((MsgHeader*)MsgBuf)->MsgId = BUFF_MSG_F2B_RELEASE;
		BuffMsgF2BRelease* msg = (BuffMsgF2BRelease*)(MsgBuf + sizeof(MsgHeader));
		msg->ClientId = ClientId;
		msg->BufferId = BufferId;
		MsgSgl->BufferLength = sizeof(MsgHeader) + sizeof(BuffMsgF2BRelease);
		RDMC_Send(QPair, MsgSgl, 1, 0, MSG_CTXT);
		RDMC_WaitForCompletionAndCheckContext(CompQ, &Ov, MSG_CTXT, false);
		printf("BackEndBridge: released the back end buffer\n");
	}

	//
	// Disconnect and release all resources
	//
	//
	
	if (Connector) {
		Connector->Disconnect(&Ov);
		Connector->Release();
	}

	if (MemRegion) {
		MemRegion->Deregister(&Ov);
		MemRegion->Release();
	}

	if (MemWindow) {
		MemWindow->Release();
	}

	if (CompQ) {
		CompQ->Release();
	}

	if (QPair) {
		QPair->Release();
	}
	
	if (AdapterFileHandle) {
		CloseHandle(AdapterFileHandle);
	}

	if (Adapter) {
		Adapter->Release();
	}

	if (Ov.hEvent) {
		CloseHandle(Ov.hEvent);
	}

	//
	// Release memory buffer
	//
	//
	if (MsgSgl) {
		delete[] MsgSgl;
	}

	if (BufferAddress) {
		HeapFree(GetProcessHeap(), 0, BufferAddress);
	}
}