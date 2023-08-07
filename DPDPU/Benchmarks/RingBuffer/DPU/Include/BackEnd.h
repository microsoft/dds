#pragma once

#include <inttypes.h>
#include <stdlib.h>

#include <rdma/rdma_cma.h>
#include <infiniband/ib.h>

#include "MsgType.h"
#include "Protocol.h"
#include "RingBufferPolling.h"

#define LISTEN_BACKLOG 64
#define RESOLVE_TIMEOUT_MS 2000

#define CTRL_COMPQ_DEPTH 16
#define CTRL_SENDQ_DEPTH 16
#define CTRL_RECVQ_DEPTH 16
#define CTRL_SEND_WR_ID 0
#define CTRL_RECV_WR_ID 1

#define BUFF_COMPQ_DEPTH 16
#define BUFF_SENDQ_DEPTH 16
#define BUFF_RECVQ_DEPTH 16
#define BUFF_SEND_WR_ID 0
#define BUFF_RECV_WR_ID 1
#define BUFF_READ_META_WR_ID 2
#define BUFF_WRITE_META_WR_ID 3
#define BUFF_READ_DATA_WR_ID 4
#define BUFF_READ_DATA_SPLIT_WR_ID 5
#define BUFF_WRITE_DATA_WR_ID 6
#define BUFF_WRITE_DATA_SPLIT_WR_ID 7

#define BUFF_READ_DATA_SPLIT_STATE_NOT_SPLIT 1
#define BUFF_READ_DATA_SPLIT_STATE_SPLIT 0

#define DDS_STORAGE_FILE_BACKEND_VERBOSE
#define RING_BUFFER_IMPL_PROGRESSIVE 0
#define RING_BUFFER_IMPL_PROGRESSIVE_NOTALIGNED 1
#define RING_BUFFER_IMPL_FARMSTYLE 2
#define RING_BUFFER_IMPL_LOCKBASED 3
#define RING_BUFFER_IMPL RING_BUFFER_IMPL_PROGRESSIVE

#if RING_BUFFER_IMPL == RING_BUFFER_IMPL_FARMSTYLE
//
// Only pointer is an int
//
//
#undef RING_BUFFER_META_DATA_SIZE
#define RING_BUFFER_META_DATA_SIZE sizeof(int)
#elif RING_BUFFER_IMPL == RING_BUFFER_IMPL_LOCKBASED
//
// Pointers are two ints
//
//
#undef RING_BUFFER_META_DATA_SIZE
#define RING_BUFFER_META_DATA_SIZE sizeof(int)
#elif RING_BUFFER_IMPL == RING_BUFFER_IMPL_PROGRESSIVE_NOTALIGNED
//
// Pointers are two ints
//
//
#undef RING_BUFFER_META_DATA_SIZE
#define RING_BUFFER_META_DATA_SIZE sizeof(int[2])
#endif

//
// The global config for (R)DMA
//
//
struct DMAConfig {
    struct rdma_event_channel *CmChannel;
    struct rdma_cm_id *CmId;
};

//
// The configuration for a control connection
//
//
struct CtrlConnConfig {
    uint32_t CtrlId;
    uint8_t InUse;

    //
    // Setup for a DMA channel
    //
    //
    struct rdma_cm_id *RemoteCmId;
    struct ibv_comp_channel *Channel;
    struct ibv_cq *CompQ;
    struct ibv_pd *PDomain;
    struct ibv_qp *QPair;

    //
    // Setup for control messages
    //
    //
    struct ibv_recv_wr RecvWr;
    struct ibv_sge RecvSgl;
    struct ibv_mr *RecvMr;
    char RecvBuff[CTRL_MSG_SIZE];

    struct ibv_send_wr SendWr;
    struct ibv_sge SendSgl;
    struct ibv_mr *SendMr;
    char SendBuff[CTRL_MSG_SIZE];
};

//
// The configuration for a buffer connection
//
//
struct BuffConnConfig {
    uint32_t BuffId;
    uint32_t CtrlId;
    uint8_t InUse;
    uint8_t Prefetching;

    //
    // Setup for a DMA channel
    //
    //
    struct rdma_cm_id *RemoteCmId;
    struct ibv_comp_channel *Channel;
    struct ibv_cq *CompQ;
    struct ibv_pd *PDomain;
    struct ibv_qp *QPair;

    //
    // Setup for control messages
    //
    //
    struct ibv_recv_wr RecvWr;
    struct ibv_sge RecvSgl;
    struct ibv_mr *RecvMr;
    char RecvBuff[CTRL_MSG_SIZE];

    struct ibv_send_wr SendWr;
    struct ibv_sge SendSgl;
    struct ibv_mr *SendMr;
    char SendBuff[CTRL_MSG_SIZE];

    //
    // Setup for data exchange
    // Every time we poll progess, we fetch two cache lines
    // One for the progress, one for the tail
    // Also, we separate pointers and content to enable prefetching
    //
    //
#if RING_BUFFER_IMPL == RING_BUFFER_IMPL_PROGRESSIVE
    struct ibv_send_wr DMAReadDataWr;
    struct ibv_sge DMAReadDataSgl;
    struct ibv_mr *DMAReadDataMr;
    char* DMAReadDataBuff;
    struct ibv_send_wr DMAReadDataSplitWr;
    struct ibv_sge DMAReadDataSplitSgl;
    uint32_t DMAReadDataSize;
    uint8_t DMAReadDataSplitState;
#elif RING_BUFFER_IMPL == RING_BUFFER_IMPL_PROGRESSIVE_NOTALIGNED
    struct ibv_send_wr DMAReadDataWr;
    struct ibv_sge DMAReadDataSgl;
    struct ibv_mr *DMAReadDataMr;
    char* DMAReadDataBuff;
    struct ibv_send_wr DMAReadDataSplitWr;
    struct ibv_sge DMAReadDataSplitSgl;
    uint32_t DMAReadDataSize;
    uint8_t DMAReadDataSplitState;
#elif RING_BUFFER_IMPL == RING_BUFFER_IMPL_LOCKBASED
    struct ibv_send_wr DMAReadDataWr;
    struct ibv_sge DMAReadDataSgl;
    struct ibv_mr *DMAReadDataMr;
    char* DMAReadDataBuff;
    struct ibv_send_wr DMAReadDataSplitWr;
    struct ibv_sge DMAReadDataSplitSgl;
    uint32_t DMAReadDataSize;
    uint8_t DMAReadDataSplitState;
#elif RING_BUFFER_IMPL == RING_BUFFER_IMPL_FARMSTYLE
    struct ibv_send_wr DMADataWr;
    struct ibv_sge DMADataSgl;
    struct ibv_mr *DMADataMr;
    char* DMADataBuff;
    struct ibv_send_wr DMADataSplitWr;
    struct ibv_sge DMADataSplitSgl;
    uint32_t DMADataSize;
    uint8_t DMADataSplitState;
#else
#error "Unknown ring buffer implementation"
#endif
    struct ibv_send_wr DMAReadMetaWr;
    struct ibv_sge DMAReadMetaSgl;
    struct ibv_mr *DMAReadMetaMr;
    char DMAReadMetaBuff[RING_BUFFER_META_DATA_SIZE];
    struct ibv_send_wr DMAWriteMetaWr;
    struct ibv_sge DMAWriteMetaSgl;
    struct ibv_mr *DMAWriteMetaMr;
    char* DMAWriteMetaBuff;

    //
    // Ring buffers
    //
    //
    struct RequestRingBufferBackEnd RequestRing;
};

//
// Back end configuration
//
//
struct BackEndConfig {
    uint32_t ServerIp;
    uint16_t ServerPort;
    uint32_t MaxClients;
    uint32_t MaxBuffs;
    struct DMAConfig DMAConf;
    struct CtrlConnConfig* CtrlConns;
    struct BuffConnConfig* BuffConns;
    uint8_t Prefetching;
};

//
// The entry point for the back end
//
//
int RunFileBackEnd(
    const char* ServerIpStr,
    const int ServerPort,
    const uint32_t MaxClients,
    const uint32_t MaxBuffs,
    const uint8_t Prefetching
);
