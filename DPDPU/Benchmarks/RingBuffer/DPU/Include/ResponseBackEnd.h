#pragma once

#include <inttypes.h>
#include <stdlib.h>

#include <rdma/rdma_cma.h>
#include <infiniband/ib.h>

#include "MsgTypes.h"
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

#define RESPONSE_CACHE_LINE_ALIGNED
#define RESPONSE_SIZE_8B 0
#define RESPONSE_SIZE_8KB 1
#define RESPONSE_SIZE RESPONSE_SIZE_8B

#define DDS_STORAGE_FILE_BACKEND_VERBOSE

//
// The global config for (R)DMA
//
//
typedef struct {
    struct rdma_event_channel *CmChannel;
    struct rdma_cm_id *CmId;
} DMAConfig;

//
// The configuration for a control connection
//
//
typedef struct {
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
} CtrlConnConfig;

//
// The configuration for a buffer connection
//
//
typedef struct {
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
    struct ibv_send_wr DMAWriteDataWr;
    struct ibv_sge DMAWriteDataSgl;
    struct ibv_mr *DMAWriteDataMr;
    char* DMAWriteDataBuff;
    struct ibv_send_wr DMAWriteDataSplitWr;
    struct ibv_sge DMAWriteDataSplitSgl;
    uint32_t DMAWriteDataSize;
    uint8_t DMAWriteDataSplitState;

    struct ibv_send_wr DMAReadMetaWr;
    struct ibv_sge DMAReadMetaSgl;
    struct ibv_mr *DMAReadMetaMr;
    char DMAReadMetaBuff[RING_BUFFER_RESPONSE_META_DATA_SIZE];
    struct ibv_send_wr DMAWriteMetaWr;
    struct ibv_sge DMAWriteMetaSgl;
    struct ibv_mr *DMAWriteMetaMr;
    char* DMAWriteMetaBuff;

    //
    // Ring buffers
    //
    //
    struct ResponseRingBufferBackEnd ResponseRing;
} BuffConnConfig;

//
// Back end configuration
//
//
typedef struct {
    uint32_t ServerIp;
    uint16_t ServerPort;
    uint32_t MaxClients;
    uint32_t MaxBuffs;
    DMAConfig DMAConf;
    CtrlConnConfig* CtrlConns;
    BuffConnConfig* BuffConns;
    uint8_t Prefetching;
} BackEndConfig;

//
// The entry point for the back end
//
//
int RunBenchmarkResponseBackEnd(
    const char* ServerIpStr,
    const int ServerPort,
    const uint32_t MaxClients,
    const uint32_t MaxBuffs,
    const uint8_t Prefetching
);
