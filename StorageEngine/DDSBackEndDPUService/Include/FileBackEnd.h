/*
 * Copyright (c) Microsoft Corporation.
 * Licensed under the MIT License
 */

#pragma once

#include <infiniband/ib.h>
#include <inttypes.h>
#include <rdma/rdma_cma.h>
#include <stdatomic.h>
#include <stdlib.h>

#include "BackEndTypes.h"
#include "FileService.h"
#include "ControlPlaneHandler.h"
#include "DataPlaneHandlers.h"
#include "DDSTypes.h"
#include "DPUBackEnd.h"
#include "DPUBackEndDir.h"
#include "DPUBackEndFile.h"
#include "DPUBackEndStorage.h"
#include "bdev.h"
#include "Zmalloc.h"

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

#define CONN_STATE_AVAILABLE 0
#define CONN_STATE_OCCUPIED 1
#define CONN_STATE_CONNECTED 2

#define DATA_PLANE_WEIGHT 10

#define DDS_STORAGE_FILE_BACKEND_VERBOSE

//
// The global config for (R)DMA
//
//
typedef struct {
    struct rdma_event_channel *CmChannel;
    struct rdma_cm_id *CmId;
} DMAConfig;

#ifdef CREATE_DEFAULT_DPU_FILE
//
// File creation state
//
//
enum FileCreationState {
    FILE_NULL = 0,
    FILE_CREATION_SUBMITTED,
    FILE_CREATED,
    FILE_CHANGED
};
#endif

//
// The configuration for a control connection
//
//
typedef struct {
    uint32_t CtrlId;
    uint8_t State;

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
    // Pending control-plane request
    //
    //
    ControlPlaneRequestContext PendingControlPlaneRequest;

#ifdef CREATE_DEFAULT_DPU_FILE
    //
    // Signals, reqs, and responses for creating a default file on DPU with specified size
    //
    //
    enum FileCreationState DefaultDpuFileCreationState;
    CtrlMsgF2BReqCreateFile DefaultCreateFileRequest;
    CtrlMsgB2FAckCreateFile DefaultCreateFileResponse;
    CtrlMsgF2BReqChangeFileSize DefaultChangeFileRequest;
    CtrlMsgB2FAckChangeFileSize DefaultChangeFileResponse;
#endif
} CtrlConnConfig;

//
// The configuration for a buffer connection
//
//
typedef struct {
    uint32_t BuffId;
    uint32_t CtrlId;
    uint8_t State;

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
    // Ring buffers
    //
    //
    struct RequestRingBufferBackEnd RequestRing;
    struct ResponseRingBufferBackEnd ResponseRing;

    //
    // Pending data-plane requests
    //
    //
    DataPlaneRequestContext PendingDataPlaneRequests[DDS_MAX_OUTSTANDING_IO];

    //
    // Next available context for the incoming request
    // Note: we don't check the availability of the context since it's guaranteed on the host
    //
    //
    RequestIdT NextRequestContext;
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

    FileService* FS;
} BackEndConfig;

//
// The entry point for the back end,
// but SPDK requires all the parameters be in just one struct, like the following
//
//
int RunFileBackEnd(
    const char* ServerIpStr,
    const int ServerPort,
    const uint32_t MaxClients,
    const uint32_t MaxBuffs,
    int Argc,
    char **Argv
);

//
// the main loop where we submit requests and check for completions, happens after all initializations
//
//
void RunAgentLoop(
    void *Ctx
);

//
// Stop the back end
//
//
int StopFileBackEnd();
