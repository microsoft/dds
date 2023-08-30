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
#define BUFF_READ_REQUEST_META_WR_ID 2
#define BUFF_WRITE_REQUEST_META_WR_ID 3
#define BUFF_READ_REQUEST_DATA_WR_ID 4
#define BUFF_READ_REQUEST_DATA_SPLIT_WR_ID 5
#define BUFF_WRITE_REQUEST_DATA_WR_ID 6
#define BUFF_WRITE_REQUEST_DATA_SPLIT_WR_ID 7
#define BUFF_READ_RESPONSE_META_WR_ID 8
#define BUFF_WRITE_RESPONSE_META_WR_ID 9
#define BUFF_READ_RESPONSE_DATA_WR_ID 10
#define BUFF_READ_RESPONSE_DATA_SPLIT_WR_ID 11
#define BUFF_WRITE_RESPONSE_DATA_WR_ID 12
#define BUFF_WRITE_RESPONSE_DATA_SPLIT_WR_ID 13


#define BUFF_READ_DATA_SPLIT_STATE_NOT_SPLIT 1
#define BUFF_READ_DATA_SPLIT_STATE_SPLIT 0

#define DDS_STORAGE_FILE_BACKEND_VERBOSE

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
    // Setup for data exchange for requests
    //
    //
    struct ibv_send_wr RequestDMAReadDataWr;
    struct ibv_sge RequestDMAReadDataSgl;
    struct ibv_mr *RequestDMAReadDataMr;
    char* RequestDMAReadDataBuff;
    struct ibv_send_wr RequestDMAReadDataSplitWr;
    struct ibv_sge RequestDMAReadDataSplitSgl;
    RingSizeT RequestDMAReadDataSize;
    RingSizeT RequestDMAReadDataSplitState;
    struct ibv_send_wr RequestDMAReadMetaWr;
    struct ibv_sge RequestDMAReadMetaSgl;
    struct ibv_mr *RequestDMAReadMetaMr;
    char RequestDMAReadMetaBuff[RING_BUFFER_REQUEST_META_DATA_SIZE];
    struct ibv_send_wr RequestDMAWriteMetaWr;
    struct ibv_sge RequestDMAWriteMetaSgl;
    struct ibv_mr *RequestDMAWriteMetaMr;
    char* RequestDMAWriteMetaBuff;
    
    //
    // Two pointers to bookkeep request execution
    //
    //
    RingSizeT RequestHead;
    RingSizeT RequestTail;

    //
    // Setup for data exchange for responses
    //
    //
    struct ibv_send_wr ResponseDMAWriteDataWr;
    struct ibv_sge ResponseDMAWriteDataSgl;
    struct ibv_mr *ResponseDMAWriteDataMr;
    char* ResponseDMAWriteDataBuff;
    struct ibv_send_wr ResponseDMAWriteDataSplitWr;
    struct ibv_sge ResponseDMAWriteDataSplitSgl;
    RingSizeT ResponseDMAWriteDataSize;
    uint8_t ResponseDMAWriteDataSplitState;
    struct ibv_send_wr ResponseDMAReadMetaWr;
    struct ibv_sge ResponseDMAReadMetaSgl;
    struct ibv_mr *ResponseDMAReadMetaMr;
    char ResponseDMAReadMetaBuff[RING_BUFFER_RESPONSE_META_DATA_SIZE];
    struct ibv_send_wr ResponseDMAWriteMetaWr;
    struct ibv_sge ResponseDMAWriteMetaSgl;
    struct ibv_mr *ResponseDMAWriteMetaMr;
    char* ResponseDMAWriteMetaBuff;

    //
    // Two pointers to bookkeep response usage
    //
    //
    RingSizeT ResponseHead;
    RingSizeT ResponseTail;

    //
    // Ring buffers
    //
    //
    struct RequestRingBufferBackEnd RequestRing;
    struct ResponseRingBufferBackEnd ResponseRing;
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
};

//
// The entry point for the back end
//
//
int RunFileBackEnd(
    const char* ServerIpStr,
    const int ServerPort,
    const uint32_t MaxClients,
    const uint32_t MaxBuffs
);
