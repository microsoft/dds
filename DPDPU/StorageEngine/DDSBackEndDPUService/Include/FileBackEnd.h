#ifndef DDS_STORAGE_FILE_BACKEND_H
#define DDS_STORAGE_FILE_BACKEND_H

#include <inttypes.h>
#include <stdlib.h>

#include <rdma/rdma_cma.h>
#include <infiniband/ib.h>

#define DDS_STORAGE_FILE_BACKEND_VERBOSE

#define CTRL_CONN_PRIV_DATA 42
#define BUFF_CONN_PRIV_DATA 24

#define DDS_CTRL_MSG_SIZE 64
#define DDS_CACHE_LINE_SIZE 64
#define DDS_LISTEN_BACKLOG 64
#define RESOLVE_TIMEOUT_MS 2000

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
    char RecvBuff[DDS_CTRL_MSG_SIZE];

    struct ibv_send_wr SendWr;
    struct ibv_sge SendSgl;
    struct ibv_mr *SendMr;
    char SendBuff[DDS_CTRL_MSG_SIZE];
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
    char RecvBuff[DDS_CTRL_MSG_SIZE];

    struct ibv_send_wr SendWr;
    struct ibv_sge SendSgl;
    struct ibv_mr *SendMr;
    char SendBuff[DDS_CTRL_MSG_SIZE];

    //
    // Setup for data exchange
    // Every time we poll progess, we fetch two cache lines
    // One for the progress, one for the tail
    //
    //
    struct ibv_send_wr DMAWr;
    struct ibv_sge DMASgl;
    struct ibv_mr *DMAMr;
    char* DMABuff;

    //
    // Setup for the remote buffer
    //
    //
    uint64_t RemoteAddr;
    uint32_t AccessToken;
    uint32_t Capacity;
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

#endif // DDS_STORAGE_FILE_BACKEND_H
