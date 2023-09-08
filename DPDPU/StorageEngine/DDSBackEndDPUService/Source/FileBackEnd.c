#define _GNU_SOURCE

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "ControlPlaneHandlers.h"
#include "DataPlaneHandlers.h"
#include "DDSTypes.h"
#include "FileBackEnd.h"
#include "DPUBackEnd.h"
#include "DPUBackEndDir.h"
#include "DPUBackEndFile.h"
#include "DPUBackEndStorage.h"
#include "bdev.h"

#define TRUE 1
#define FALSE 0

static volatile int ForceQuitFileBackEnd = 0;

struct DPUStorage *Sto;

//
// Set a CM channel to be non-blocking
//
//
int SetNonblocking(
    struct rdma_event_channel *Channel
) {
    int flags = fcntl(Channel->fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl F_GETFL");
        return -1;
    }

    if (fcntl(Channel->fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl F_SETFL O_NONBLOCK");
        return -1;
    }

    return 0;
}

//
// Initialize DMA
//
//
static int
InitDMA(
    struct DMAConfig* Config,
    uint32_t Ip,
    uint16_t Port
) {
    int ret = 0;
    struct sockaddr_in sin;

    Config->CmChannel = rdma_create_event_channel();
    if (!Config->CmChannel) {
        ret = errno;
        fprintf(stderr, "rdma_create_event_channel error %d\n", ret);
        return ret;
    }

    ret = SetNonblocking(Config->CmChannel);
    if (ret) {
        fprintf(stderr, "failed to set non-blocking\n");
        rdma_destroy_event_channel(Config->CmChannel);
        return ret;
    }

#ifdef DDS_STORAGE_FILE_BACKEND_VERBOSE
    fprintf(stdout, "Created CmChannel %p\n", Config->CmChannel);
#endif

    ret = rdma_create_id(Config->CmChannel, &Config->CmId, Config, RDMA_PS_TCP);
    if (ret) {
        ret = errno;
        fprintf(stderr, "rdma_create_id error %d\n", ret);
        rdma_destroy_event_channel(Config->CmChannel);
        return ret;
    }

#ifdef DDS_STORAGE_FILE_BACKEND_VERBOSE
    fprintf(stdout, "Created CmId %p\n", Config->CmId);
#endif

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = Ip;
    sin.sin_port = Port;

    ret = rdma_bind_addr(Config->CmId, (struct sockaddr *) &sin);
    if (ret) {
        ret = errno;
        fprintf(stderr, "rdma_bind_addr error %d\n", ret);
        rdma_destroy_event_channel(Config->CmChannel);
        rdma_destroy_id(Config->CmId);
        return ret;
    }

#ifdef DDS_STORAGE_FILE_BACKEND_VERBOSE
    fprintf(stdout, "rdma_bind_addr succeeded\n");
#endif

    return ret;
}

//
// Terminate DMA
//
//
static void
TermDMA(
    struct DMAConfig* Config
) {
    if (Config->CmId) {
        rdma_destroy_id(Config->CmId);
    }

    if (Config->CmChannel) {
        rdma_destroy_event_channel(Config->CmChannel);
    }
}

//
// Allocate connections
//
//
static int
AllocConns(struct BackEndConfig* Config) {
    Config->CtrlConns = (struct CtrlConnConfig*)malloc(sizeof(struct CtrlConnConfig) * Config->MaxClients);
    if (!Config->CtrlConns) {
        fprintf(stderr, "Failed to allocate CtrlConns\n");
        return ENOMEM;
    }
    memset(Config->CtrlConns, 0, sizeof(struct CtrlConnConfig) * Config->MaxClients);
    for (int c = 0; c < Config->MaxClients; c++) {
        Config->CtrlConns[c].CtrlId = c;
    }

    Config->BuffConns = (struct BuffConnConfig*)malloc(sizeof(struct BuffConnConfig) * Config->MaxClients);
    if (!Config->BuffConns) {
        fprintf(stderr, "Failed to allocate BuffConns\n");
        free(Config->CtrlConns);
        return ENOMEM;
    }
    memset(Config->BuffConns, 0, sizeof(struct BuffConnConfig) * Config->MaxClients);
    for (int c = 0; c < Config->MaxBuffs; c++) {
        Config->BuffConns[c].BuffId = c;
    }

    return 0;
}

//
// Deallocate connections
//
//
static void
DeallocConns(struct BackEndConfig* Config) {
    if (Config->CtrlConns) {
        free(Config->CtrlConns);
    }

    if (Config->BuffConns) {
        free(Config->BuffConns);
    }
}

//
// Handle signals
//
//
static void
SignalHandler(
    int SigNum
) {
    if (SigNum == SIGINT || SigNum == SIGTERM) {
        fprintf(stdout, "Received signal to exit\n");
        ForceQuitFileBackEnd = 1;
    }
}

//
// Set up queue pairs for a control connection
//
//
static int
SetUpCtrlQPair(
    struct CtrlConnConfig* CtrlConn
) {
    int ret = 0;
    struct ibv_qp_init_attr initAttr;

    CtrlConn->PDomain = ibv_alloc_pd(CtrlConn->RemoteCmId->verbs);
    if (!CtrlConn->PDomain) {
        fprintf(stderr, "%s [error]: ibv_alloc_pd failed\n", __func__);
        ret = -1;
        goto SetUpCtrlQPairReturn;
    }

    CtrlConn->Channel = ibv_create_comp_channel(CtrlConn->RemoteCmId->verbs);
    if (!CtrlConn->Channel) {
        fprintf(stderr, "%s [error]: ibv_create_comp_channel failed\n", __func__);
        ret = -1;
        goto DeallocPdReturn;
    }

    CtrlConn->CompQ = ibv_create_cq(
        CtrlConn->RemoteCmId->verbs,
        CTRL_COMPQ_DEPTH * 2,
        CtrlConn,
        CtrlConn->Channel,
        0
    );
    if (!CtrlConn->CompQ) {
        fprintf(stderr, "%s [error]: ibv_create_cq failed\n", __func__);
        ret = -1;
        goto DestroyCommChannelReturn;
    }

    ret = ibv_req_notify_cq(CtrlConn->CompQ, 0);
    if (ret) {
        fprintf(stderr, "%s [error]: ibv_req_notify_cq failed\n", __func__);
        goto DestroyCompQReturn;
    }

    memset(&initAttr, 0, sizeof(initAttr));
    initAttr.cap.max_send_wr = CTRL_SENDQ_DEPTH;
    initAttr.cap.max_recv_wr = CTRL_RECVQ_DEPTH;
    initAttr.cap.max_recv_sge = 1;
    initAttr.cap.max_send_sge = 1;
    initAttr.qp_type = IBV_QPT_RC;
    initAttr.send_cq = CtrlConn->CompQ;
    initAttr.recv_cq = CtrlConn->CompQ;

    ret = rdma_create_qp(CtrlConn->RemoteCmId, CtrlConn->PDomain, &initAttr);
    if (!ret) {
        CtrlConn->QPair = CtrlConn->RemoteCmId->qp;
    }
    else {
        fprintf(stderr, "%s [error]: rdma_create_qp failed\n", __func__);
        ret = -1;
        goto DestroyCompQReturn;
    }

    return 0;

DestroyCompQReturn:
    ibv_destroy_cq(CtrlConn->CompQ);

DestroyCommChannelReturn:
    ibv_destroy_comp_channel(CtrlConn->Channel);

DeallocPdReturn:
    ibv_dealloc_pd(CtrlConn->PDomain);

SetUpCtrlQPairReturn:
    return ret;
}

//
// Destrory queue pairs for a control connection
//
//
static void
DestroyCtrlQPair(
    struct CtrlConnConfig* CtrlConn
) {
    rdma_destroy_qp(CtrlConn->RemoteCmId);
    ibv_destroy_cq(CtrlConn->CompQ);
    ibv_destroy_comp_channel(CtrlConn->Channel);
    ibv_dealloc_pd(CtrlConn->PDomain);
}

//
// Set up regions and buffers for a control connection
//
//
static int
SetUpCtrlRegionsAndBuffers(
    struct CtrlConnConfig* CtrlConn
) {
    int ret = 0;
    CtrlConn->RecvMr = ibv_reg_mr(
        CtrlConn->PDomain,
        CtrlConn->RecvBuff,
        CTRL_MSG_SIZE,
        IBV_ACCESS_LOCAL_WRITE
    );
    if (!CtrlConn->RecvMr) {
        fprintf(stderr, "%s [error]: ibv_reg_mr for receive failed\n", __func__);
        ret = -1;
        goto SetUpCtrlRegionsAndBuffersReturn;
    }

    CtrlConn->SendMr = ibv_reg_mr(
        CtrlConn->PDomain,
        CtrlConn->SendBuff,
        CTRL_MSG_SIZE,
        0
    );
    if (!CtrlConn->SendMr) {
        fprintf(stderr, "%s [error]: ibv_reg_mr for send failed\n", __func__);
        ret = -1;
        goto DeregisterRecvMrReturn;
    }

    //
    // Set up work requests
    //
    //
    CtrlConn->RecvSgl.addr = (uint64_t)CtrlConn->RecvBuff;
    CtrlConn->RecvSgl.length = CTRL_MSG_SIZE;
    CtrlConn->RecvSgl.lkey = CtrlConn->RecvMr->lkey;
    CtrlConn->RecvWr.sg_list = &CtrlConn->RecvSgl;
    CtrlConn->RecvWr.num_sge = 1;
    CtrlConn->RecvWr.wr_id = CTRL_RECV_WR_ID;

    CtrlConn->SendSgl.addr = (uint64_t)CtrlConn->SendBuff;
    CtrlConn->SendSgl.length = CTRL_MSG_SIZE;
    CtrlConn->SendSgl.lkey = CtrlConn->SendMr->lkey;
    CtrlConn->SendWr.opcode = IBV_WR_SEND;
    CtrlConn->SendWr.send_flags = IBV_SEND_SIGNALED;
    CtrlConn->SendWr.sg_list = &CtrlConn->SendSgl;
    CtrlConn->SendWr.num_sge = 1;
    CtrlConn->SendWr.wr_id = CTRL_SEND_WR_ID;

    return 0;

DeregisterRecvMrReturn:
    ibv_dereg_mr(CtrlConn->RecvMr);

SetUpCtrlRegionsAndBuffersReturn:
    return ret;
}

//
// Destrory regions and buffers for a control connection
//
//
static void
DestroyCtrlRegionsAndBuffers(
    struct CtrlConnConfig* CtrlConn
) {
    ibv_dereg_mr(CtrlConn->SendMr);
    ibv_dereg_mr(CtrlConn->RecvMr);
    memset(&CtrlConn->SendSgl, 0, sizeof(CtrlConn->SendSgl));
    memset(&CtrlConn->RecvSgl, 0, sizeof(CtrlConn->RecvSgl));
    memset(&CtrlConn->SendWr, 0, sizeof(CtrlConn->SendWr));
    memset(&CtrlConn->RecvWr, 0, sizeof(CtrlConn->RecvWr));
}

//
// Set up queue pairs for a buffer connection
//
//
static int
SetUpBuffQPair(
    struct BuffConnConfig* BuffConn
) {
    int ret = 0;
    struct ibv_qp_init_attr initAttr;

    BuffConn->PDomain = ibv_alloc_pd(BuffConn->RemoteCmId->verbs);
    if (!BuffConn->PDomain) {
        fprintf(stderr, "%s [error]: ibv_alloc_pd failed\n", __func__);
        ret = -1;
        goto SetUpBuffQPairReturn;
    }

    BuffConn->Channel = ibv_create_comp_channel(BuffConn->RemoteCmId->verbs);
    if (!BuffConn->Channel) {
        fprintf(stderr, "%s [error]: ibv_create_comp_channel failed\n", __func__);
        ret = -1;
        goto DeallocBuffPdReturn;
    }

    BuffConn->CompQ = ibv_create_cq(
        BuffConn->RemoteCmId->verbs,
        BUFF_COMPQ_DEPTH * 2,
        BuffConn,
        BuffConn->Channel,
        0
    );
    if (!BuffConn->CompQ) {
        fprintf(stderr, "%s [error]: ibv_create_cq failed\n", __func__);
        ret = -1;
        goto DestroyBuffCommChannelReturn;
    }

    ret = ibv_req_notify_cq(BuffConn->CompQ, 0);
    if (ret) {
        fprintf(stderr, "%s [error]: ibv_req_notify_cq failed\n", __func__);
        goto DestroyBuffCompQReturn;
    }

    memset(&initAttr, 0, sizeof(initAttr));
    initAttr.cap.max_send_wr = BUFF_SENDQ_DEPTH;
    initAttr.cap.max_recv_wr = BUFF_RECVQ_DEPTH;
    initAttr.cap.max_recv_sge = 1;
    initAttr.cap.max_send_sge = 1;
    initAttr.qp_type = IBV_QPT_RC;
    initAttr.send_cq = BuffConn->CompQ;
    initAttr.recv_cq = BuffConn->CompQ;

    ret = rdma_create_qp(BuffConn->RemoteCmId, BuffConn->PDomain, &initAttr);
    if (!ret) {
        BuffConn->QPair = BuffConn->RemoteCmId->qp;
    }
    else {
        fprintf(stderr, "%s [error]: rdma_create_qp failed\n", __func__);
        ret = -1;
        goto DestroyBuffCompQReturn;
    }

    return 0;

DestroyBuffCompQReturn:
    ibv_destroy_cq(BuffConn->CompQ);

DestroyBuffCommChannelReturn:
    ibv_destroy_comp_channel(BuffConn->Channel);

DeallocBuffPdReturn:
    ibv_dealloc_pd(BuffConn->PDomain);

SetUpBuffQPairReturn:
    return ret;
}

//
// Destrory queue pairs for a buffer connection
//
//
static void
DestroyBuffQPair(
    struct BuffConnConfig* BuffConn
) {
    rdma_destroy_qp(BuffConn->RemoteCmId);
    ibv_destroy_cq(BuffConn->CompQ);
    ibv_destroy_comp_channel(BuffConn->Channel);
    ibv_dealloc_pd(BuffConn->PDomain);
}

//
// Set up RDMA regions for control messages
//
//
static int
SetUpForCtrlMsgs(
    struct BuffConnConfig* BuffConn
) {
    int ret = 0;

    //
    // Receive buffer and region
    //
    //
    BuffConn->RecvMr = ibv_reg_mr(
        BuffConn->PDomain,
        BuffConn->RecvBuff,
        CTRL_MSG_SIZE,
        IBV_ACCESS_LOCAL_WRITE
    );
    if (!BuffConn->RecvMr) {
        fprintf(stderr, "%s [error]: ibv_reg_mr for receive failed\n", __func__);
        ret = -1;
        goto SetUpForCtrlMsgsReturn;
    }

    //
    // Send buffer and region
    //
    //
    BuffConn->SendMr = ibv_reg_mr(
        BuffConn->PDomain,
        BuffConn->SendBuff,
        CTRL_MSG_SIZE,
        0
    );
    if (!BuffConn->SendMr) {
        fprintf(stderr, "%s [error]: ibv_reg_mr for send failed\n", __func__);
        ret = -1;
        goto DeregisterBuffRecvMrForCtrlMsgsReturn;
    }

    //
    // Set up work requests
    //
    //
    BuffConn->RecvSgl.addr = (uint64_t)BuffConn->RecvBuff;
    BuffConn->RecvSgl.length = CTRL_MSG_SIZE;
    BuffConn->RecvSgl.lkey = BuffConn->RecvMr->lkey;
    BuffConn->RecvWr.sg_list = &BuffConn->RecvSgl;
    BuffConn->RecvWr.num_sge = 1;
    BuffConn->RecvWr.wr_id = BUFF_SEND_WR_ID;

    BuffConn->SendSgl.addr = (uint64_t)BuffConn->SendBuff;
    BuffConn->SendSgl.length = CTRL_MSG_SIZE;
    BuffConn->SendSgl.lkey = BuffConn->SendMr->lkey;
    BuffConn->SendWr.opcode = IBV_WR_SEND;
    BuffConn->SendWr.send_flags = IBV_SEND_SIGNALED;
    BuffConn->SendWr.sg_list = &BuffConn->SendSgl;
    BuffConn->SendWr.num_sge = 1;
    BuffConn->SendWr.wr_id = BUFF_RECV_WR_ID;

    return 0;

DeregisterBuffRecvMrForCtrlMsgsReturn:
    ibv_dereg_mr(BuffConn->RecvMr);

SetUpForCtrlMsgsReturn:
    return ret;
}

//
// Destroy the setup for control messages
//
//
static void
DestroyForCtrlMsgs(
    struct BuffConnConfig* BuffConn
) {
    ibv_dereg_mr(BuffConn->SendMr);
    ibv_dereg_mr(BuffConn->RecvMr);
    memset(&BuffConn->SendSgl, 0, sizeof(BuffConn->SendSgl));
    memset(&BuffConn->RecvSgl, 0, sizeof(BuffConn->RecvSgl));
    memset(&BuffConn->SendWr, 0, sizeof(BuffConn->SendWr));
    memset(&BuffConn->RecvWr, 0, sizeof(BuffConn->RecvWr));
}

//
// Set up regions and buffers for the request buffer
//
//
static int
SetUpForRequests(
    struct BuffConnConfig* BuffConn
) {
    int ret = 0;

    //
    // Read data buffer and region
    //
    //
    BuffConn->RequestDMAReadDataBuff = malloc(BACKEND_REQUEST_BUFFER_SIZE);
    if (!BuffConn->RequestDMAReadDataBuff) {
        fprintf(stderr, "%s [error]: OOM for DMA read data buffer\n", __func__);
        ret = -1;
        goto SetUpForRequestsReturn;
    }

    BuffConn->RequestDMAReadDataMr = ibv_reg_mr(
        BuffConn->PDomain,
        BuffConn->RequestDMAReadDataBuff,
        BACKEND_REQUEST_BUFFER_SIZE,
        IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ
    );
    if (!BuffConn->RequestDMAReadDataMr) {
        fprintf(stderr, "%s [error]: ibv_reg_mr for DMA read data failed\n", __func__);
        ret = -1;
        goto FreeBuffDMAReadDataBuffForRequestsReturn;
    }

    //
    // Read meta buffer and region
    //
    //
    BuffConn->RequestDMAReadMetaMr = ibv_reg_mr(
        BuffConn->PDomain,
        BuffConn->RequestDMAReadMetaBuff,
        RING_BUFFER_REQUEST_META_DATA_SIZE,
        IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ
    );
    if (!BuffConn->RequestDMAReadMetaMr) {
        fprintf(stderr, "%s [error]: ibv_reg_mr for DMA read meta failed\n", __func__);
        ret = -1;
        goto DeregisterBuffReadDataMrForRequestsReturn;
    }

    //
    // Write meta buffer and region
    //
    //
    BuffConn->RequestDMAWriteMetaBuff = (char*)&BuffConn->RequestRing.Head;
    BuffConn->RequestDMAWriteMetaMr = ibv_reg_mr(
        BuffConn->PDomain,
        BuffConn->RequestDMAWriteMetaBuff,
        sizeof(int),
        IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ
    );
    if (!BuffConn->RequestDMAWriteMetaMr) {
        fprintf(stderr, "%s [error]: ibv_reg_mr for DMA write meta failed\n", __func__);
        ret = -1;
        goto DeregisterBuffReadMetaMrForRequestsReturn;
    }

    //
    // Set up work requests
    //
    //
    BuffConn->RequestDMAReadDataSgl.addr = (uint64_t)BuffConn->RequestDMAReadDataBuff;
    BuffConn->RequestDMAReadDataSgl.length = BACKEND_REQUEST_MAX_DMA_SIZE;
    BuffConn->RequestDMAReadDataSgl.lkey = BuffConn->RequestDMAReadDataMr->lkey;
    BuffConn->RequestDMAReadDataWr.opcode = IBV_WR_RDMA_READ;
    BuffConn->RequestDMAReadDataWr.send_flags = IBV_SEND_SIGNALED;
    BuffConn->RequestDMAReadDataWr.sg_list = &BuffConn->RequestDMAReadDataSgl;
    BuffConn->RequestDMAReadDataWr.num_sge = 1;
    BuffConn->RequestDMAReadDataWr.wr_id = BUFF_READ_REQUEST_DATA_WR_ID;

    BuffConn->RequestDMAReadMetaSgl.addr = (uint64_t)BuffConn->RequestDMAReadMetaBuff;
    BuffConn->RequestDMAReadMetaSgl.length = RING_BUFFER_REQUEST_META_DATA_SIZE;
    BuffConn->RequestDMAReadMetaSgl.lkey = BuffConn->RequestDMAReadMetaMr->lkey;
    BuffConn->RequestDMAReadMetaWr.opcode = IBV_WR_RDMA_READ;
    BuffConn->RequestDMAReadMetaWr.send_flags = IBV_SEND_SIGNALED;
    BuffConn->RequestDMAReadMetaWr.sg_list = &BuffConn->RequestDMAReadMetaSgl;
    BuffConn->RequestDMAReadMetaWr.num_sge = 1;
    BuffConn->RequestDMAReadMetaWr.wr_id = BUFF_READ_REQUEST_META_WR_ID;

    BuffConn->RequestDMAWriteMetaSgl.addr = (uint64_t)BuffConn->RequestDMAWriteMetaBuff;
    BuffConn->RequestDMAWriteMetaSgl.length = sizeof(int);
    BuffConn->RequestDMAWriteMetaSgl.lkey = BuffConn->RequestDMAWriteMetaMr->lkey;
    BuffConn->RequestDMAWriteMetaWr.opcode = IBV_WR_RDMA_WRITE;
    BuffConn->RequestDMAWriteMetaWr.send_flags = IBV_SEND_SIGNALED;
    BuffConn->RequestDMAWriteMetaWr.sg_list = &BuffConn->RequestDMAWriteMetaSgl;
    BuffConn->RequestDMAWriteMetaWr.num_sge = 1;
    BuffConn->RequestDMAWriteMetaWr.wr_id = BUFF_WRITE_REQUEST_META_WR_ID;

    return 0;

DeregisterBuffReadMetaMrForRequestsReturn:
    ibv_dereg_mr(BuffConn->RequestDMAReadMetaMr);

DeregisterBuffReadDataMrForRequestsReturn:
    ibv_dereg_mr(BuffConn->RequestDMAReadDataMr);

FreeBuffDMAReadDataBuffForRequestsReturn:
    free(BuffConn->RequestDMAReadDataBuff);

SetUpForRequestsReturn:
    return ret;
}

//
// Destrory regions and buffers for the request buffer
//
//
static void
DestroyForRequests(
    struct BuffConnConfig* BuffConn
) {
    free(BuffConn->RequestDMAReadDataBuff);

    ibv_dereg_mr(BuffConn->RequestDMAWriteMetaMr);
    ibv_dereg_mr(BuffConn->RequestDMAReadMetaMr);
    ibv_dereg_mr(BuffConn->RequestDMAReadDataMr);

    memset(&BuffConn->RequestDMAWriteMetaSgl, 0, sizeof(BuffConn->RequestDMAReadMetaSgl));
    memset(&BuffConn->RequestDMAReadMetaSgl, 0, sizeof(BuffConn->RequestDMAReadMetaSgl));
    memset(&BuffConn->RequestDMAReadDataSgl, 0, sizeof(BuffConn->RequestDMAReadDataSgl));
    
    memset(&BuffConn->RequestDMAWriteMetaWr, 0, sizeof(BuffConn->RequestDMAWriteMetaWr));
    memset(&BuffConn->RequestDMAReadMetaWr, 0, sizeof(BuffConn->RequestDMAReadMetaWr));
    memset(&BuffConn->RequestDMAReadDataWr, 0, sizeof(BuffConn->RequestDMAReadDataWr));
}

//
// Set up regions and buffers for the response buffer
//
//
static int
SetUpForResponses(
    struct BuffConnConfig* BuffConn
) {
    int ret = 0;

    //
    // Read data buffer and region
    //
    //
    BuffConn->ResponseDMAWriteDataBuff = malloc(BACKEND_RESPONSE_BUFFER_SIZE);
    if (!BuffConn->ResponseDMAWriteDataBuff) {
        fprintf(stderr, "%s [error]: OOM for DMA read data buffer\n", __func__);
        ret = -1;
        goto SetUpForResponsesReturn;
    }

    BuffConn->ResponseDMAWriteDataMr = ibv_reg_mr(
        BuffConn->PDomain,
        BuffConn->ResponseDMAWriteDataBuff,
        BACKEND_RESPONSE_BUFFER_SIZE,
        IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ
    );
    if (!BuffConn->ResponseDMAWriteDataMr) {
        fprintf(stderr, "%s [error]: ibv_reg_mr for DMA read data failed\n", __func__);
        ret = -1;
        goto FreeBuffDMAReadDataBuffForResponsesReturn;
    }

    //
    // Read meta buffer and region
    //
    //
    BuffConn->ResponseDMAReadMetaMr = ibv_reg_mr(
        BuffConn->PDomain,
        BuffConn->ResponseDMAReadMetaBuff,
        RING_BUFFER_RESPONSE_META_DATA_SIZE,
        IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ
    );
    if (!BuffConn->ResponseDMAReadMetaMr) {
        fprintf(stderr, "%s [error]: ibv_reg_mr for DMA read meta failed\n", __func__);
        ret = -1;
        goto DeregisterBuffReadDataMrForResponsesReturn;
    }

    //
    // Write meta buffer and region
    //
    //
    BuffConn->ResponseDMAWriteMetaBuff = (char*)&BuffConn->ResponseRing.Tail;
    BuffConn->ResponseDMAWriteMetaMr = ibv_reg_mr(
        BuffConn->PDomain,
        BuffConn->ResponseDMAWriteMetaBuff,
        sizeof(int),
        IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ
    );
    if (!BuffConn->ResponseDMAWriteMetaMr) {
        fprintf(stderr, "%s [error]: ibv_reg_mr for DMA write meta failed\n", __func__);
        ret = -1;
        goto DeregisterBuffReadMetaMrForResponsesReturn;
    }

    //
    // Set up work requests
    //
    //
    BuffConn->ResponseDMAWriteDataSgl.addr = (uint64_t)BuffConn->ResponseDMAWriteDataBuff;
    BuffConn->ResponseDMAWriteDataSgl.length = BACKEND_RESPONSE_MAX_DMA_SIZE;
    BuffConn->ResponseDMAWriteDataSgl.lkey = BuffConn->ResponseDMAWriteDataMr->lkey;
    BuffConn->ResponseDMAWriteDataWr.opcode = IBV_WR_RDMA_READ;
    BuffConn->ResponseDMAWriteDataWr.send_flags = IBV_SEND_SIGNALED;
    BuffConn->ResponseDMAWriteDataWr.sg_list = &BuffConn->ResponseDMAWriteDataSgl;
    BuffConn->ResponseDMAWriteDataWr.num_sge = 1;
    BuffConn->ResponseDMAWriteDataWr.wr_id = BUFF_READ_RESPONSE_DATA_WR_ID;

    BuffConn->ResponseDMAReadMetaSgl.addr = (uint64_t)BuffConn->ResponseDMAReadMetaBuff;
    BuffConn->ResponseDMAReadMetaSgl.length = RING_BUFFER_RESPONSE_META_DATA_SIZE;
    BuffConn->ResponseDMAReadMetaSgl.lkey = BuffConn->ResponseDMAReadMetaMr->lkey;
    BuffConn->ResponseDMAReadMetaWr.opcode = IBV_WR_RDMA_READ;
    BuffConn->ResponseDMAReadMetaWr.send_flags = IBV_SEND_SIGNALED;
    BuffConn->ResponseDMAReadMetaWr.sg_list = &BuffConn->ResponseDMAReadMetaSgl;
    BuffConn->ResponseDMAReadMetaWr.num_sge = 1;
    BuffConn->ResponseDMAReadMetaWr.wr_id = BUFF_READ_RESPONSE_META_WR_ID;

    BuffConn->ResponseDMAWriteMetaSgl.addr = (uint64_t)BuffConn->ResponseDMAWriteMetaBuff;
    BuffConn->ResponseDMAWriteMetaSgl.length = sizeof(int);
    BuffConn->ResponseDMAWriteMetaSgl.lkey = BuffConn->ResponseDMAWriteMetaMr->lkey;
    BuffConn->ResponseDMAWriteMetaWr.opcode = IBV_WR_RDMA_WRITE;
    BuffConn->ResponseDMAWriteMetaWr.send_flags = IBV_SEND_SIGNALED;
    BuffConn->ResponseDMAWriteMetaWr.sg_list = &BuffConn->ResponseDMAWriteMetaSgl;
    BuffConn->ResponseDMAWriteMetaWr.num_sge = 1;
    BuffConn->ResponseDMAWriteMetaWr.wr_id = BUFF_WRITE_RESPONSE_META_WR_ID;

    return 0;

DeregisterBuffReadMetaMrForResponsesReturn:
    ibv_dereg_mr(BuffConn->ResponseDMAReadMetaMr);

DeregisterBuffReadDataMrForResponsesReturn:
    ibv_dereg_mr(BuffConn->ResponseDMAWriteDataMr);

FreeBuffDMAReadDataBuffForResponsesReturn:
    free(BuffConn->ResponseDMAWriteDataBuff);

SetUpForResponsesReturn:
    return ret;
}

//
// Destrory regions and buffers for the request buffer
//
//
static void
DestroyForResponses(
    struct BuffConnConfig* BuffConn
) {
    free(BuffConn->ResponseDMAWriteDataBuff);

    ibv_dereg_mr(BuffConn->ResponseDMAWriteMetaMr);
    ibv_dereg_mr(BuffConn->ResponseDMAReadMetaMr);
    ibv_dereg_mr(BuffConn->ResponseDMAWriteDataMr);

    memset(&BuffConn->ResponseDMAWriteMetaSgl, 0, sizeof(BuffConn->ResponseDMAReadMetaSgl));
    memset(&BuffConn->ResponseDMAReadMetaSgl, 0, sizeof(BuffConn->ResponseDMAReadMetaSgl));
    memset(&BuffConn->ResponseDMAWriteDataSgl, 0, sizeof(BuffConn->ResponseDMAWriteDataSgl));
    
    memset(&BuffConn->ResponseDMAWriteMetaWr, 0, sizeof(BuffConn->ResponseDMAWriteMetaWr));
    memset(&BuffConn->ResponseDMAReadMetaWr, 0, sizeof(BuffConn->ResponseDMAReadMetaWr));
    memset(&BuffConn->ResponseDMAWriteDataWr, 0, sizeof(BuffConn->RequestDMAReadDataWr));
}

//
// Set up regions and buffers for a buffer connection
//
//
static int
SetUpBuffRegionsAndBuffers(
    struct BuffConnConfig* BuffConn
) {
    int ret = 0;

    //
    // Set up for control messages
    //
    //
    ret = SetUpForCtrlMsgs(BuffConn);
    if (ret) {
        fprintf(stderr, "%s [error]: SetUpForCtrlMsgs failed\n", __func__);
        goto SetUpBuffRegionsAndBuffersReturn;
    }

    //
    // Set up for the request ring
    //
    //
    ret = SetUpForRequests(BuffConn);
    if (ret) {
        fprintf(stderr, "%s [error]: SetUpForRequests failed\n", __func__);
        goto DeregisterBuffRecvMrForCtrlMsgsReturn;
    }

    //
    // Set up for the response ring
    //
    //
    ret = SetUpForResponses(BuffConn);
    if (ret) {
        fprintf(stderr, "%s [error]: SetUpForResponses failed\n", __func__);
        goto DeregisterBuffRecvMrForRequestsReturn;
    }

    return 0;

DeregisterBuffRecvMrForRequestsReturn:
    DestroyForRequests(BuffConn);

DeregisterBuffRecvMrForCtrlMsgsReturn:
    DestroyForCtrlMsgs(BuffConn);

SetUpBuffRegionsAndBuffersReturn:
    return ret;
}

//
// Destrory regions and buffers for a buffer connection
//
//
static void
DestroyBuffRegionsAndBuffers(
    struct BuffConnConfig* BuffConn
) {
    DestroyForCtrlMsgs(BuffConn);
    DestroyForRequests(BuffConn);
    DestroyForResponses(BuffConn);
}

//
// Find the id of a control connection
//
//
static int
FindConnId(
    struct BackEndConfig *Config,
    struct rdma_cm_id *CmId,
    uint8_t *IsCtrl
) {
    int i;
        for (i = 0; i < Config->MaxClients; i++) {
            if (Config->CtrlConns[i].RemoteCmId == CmId) {
                *IsCtrl = TRUE;
                return i;
            }
        }
        for (i = 0; i < Config->MaxBuffs; i++) {
            if (Config->BuffConns[i].RemoteCmId == CmId) {
                *IsCtrl = FALSE;
                return i;
            }
        }

    return -1;
}

//
// Process communication channel events
//
//
static inline int
ProcessCmEvents(
    struct BackEndConfig *Config,
    struct rdma_cm_event *Event
) {
    int ret = 0;

    switch(Event->event) {
        case RDMA_CM_EVENT_ADDR_RESOLVED:
        {
#ifdef DDS_STORAGE_FILE_BACKEND_VERBOSE
            fprintf(stdout, "CM: RDMA_CM_EVENT_ADDR_RESOLVED\n");
#endif
            ret = rdma_resolve_route(Event->id, RESOLVE_TIMEOUT_MS);
            if (ret) {
                fprintf(stderr, "rdma_resolve_route error %d\n", ret);
            }
            rdma_ack_cm_event(Event);
            break;
        }
        case RDMA_CM_EVENT_ROUTE_RESOLVED:
        {
#ifdef DDS_STORAGE_FILE_BACKEND_VERBOSE
            printf("CM: RDMA_CM_EVENT_ROUTE_RESOLVED\n");
#endif
            rdma_ack_cm_event(Event);
            break;
        }
        case RDMA_CM_EVENT_CONNECT_REQUEST:
        {
            //
            // Check the type of this connection
            //
            //
            uint8_t privData = *(uint8_t*)Event->param.conn.private_data;
            switch (privData) {
                case CTRL_CONN_PRIV_DATA:
                {
                    struct CtrlConnConfig *ctrlConn = NULL;

                    for (int index = 0; index < Config->MaxClients; index++) {
                        if (!Config->CtrlConns[index].InUse) {
                            ctrlConn = &Config->CtrlConns[index];
                            break;
                        }
                    }

                    if (ctrlConn) {
                        struct rdma_conn_param connParam;
                        struct ibv_recv_wr *badRecvWr;
                        ctrlConn->RemoteCmId = Event->id;
                        rdma_ack_cm_event(Event);

                        //
                        // Set up QPair
                        //
                        //
                        ret = SetUpCtrlQPair(ctrlConn);
                        if (ret) {
                            fprintf(stderr, "%s [error]: SetUpCtrlQPair failed\n", __func__);
                            break;
                        }

                        //
                        // Set up regions and buffers
                        //
                        //
                        ret = SetUpCtrlRegionsAndBuffers(ctrlConn);
                        if (ret) {
                            fprintf(stderr, "%s [error]: SetUpCtrlRegionsAndBuffers failed\n", __func__);
                            DestroyCtrlQPair(ctrlConn);
                            break;
                        }

                        //
                        // Post a receive
                        //
                        //
                        ret = ibv_post_recv(ctrlConn->QPair, &ctrlConn->RecvWr, &badRecvWr);
                        if (ret) {
                            fprintf(stderr, "%s [error]: ibv_post_recv failed %d\n", __func__, ret);
                            DestroyCtrlRegionsAndBuffers(ctrlConn);
                            DestroyCtrlQPair(ctrlConn);
                            break;
                        }

                        //
                        // Accept the connection
                        //
                        //
                        memset(&connParam, 0, sizeof(connParam));
                        connParam.responder_resources = CTRL_RECVQ_DEPTH;
                        connParam.initiator_depth = CTRL_SENDQ_DEPTH;
                        ret = rdma_accept(ctrlConn->RemoteCmId, &connParam);
                        if (ret) {
                            fprintf(stderr, "%s [error]: rdma_accept failed %d\n", __func__, ret);
                            DestroyCtrlRegionsAndBuffers(ctrlConn);
                            DestroyCtrlQPair(ctrlConn);
                            break;
                        }

                        //
                        // Mark this connection unavailable
                        //
                        //
                        ctrlConn->InUse = TRUE;
                        fprintf(stdout, "Control connection #%d is accepted\n", ctrlConn->CtrlId);
                    }
                    else {
                        fprintf(stderr, "%s [error]: no available control connection\n", __func__);
                        rdma_ack_cm_event(Event);
                    }

                    break;
                }
                case BUFF_CONN_PRIV_DATA:
                {
                    struct BuffConnConfig *buffConn = NULL;
                    int index;

                    for (index = 0; index < Config->MaxClients; index++) {
                        if (!Config->BuffConns[index].InUse) {
                            buffConn = &Config->BuffConns[index];
                            break;
                        }
                    }

                    if (buffConn) {
                        struct rdma_conn_param connParam;
                        struct ibv_recv_wr *badRecvWr;
                        buffConn->RemoteCmId = Event->id;
                        rdma_ack_cm_event(Event);

                        //
                        // Set up QPair
                        //
                        //
                        ret = SetUpBuffQPair(buffConn);
                        if (ret) {
                            fprintf(stderr, "%s [error]: SetUpBuffQPair failed\n", __func__);
                            break;
                        }

                        //
                        // Set up regions and buffers
                        //
                        //
                        ret = SetUpBuffRegionsAndBuffers(buffConn);
                        if (ret) {
                            fprintf(stderr, "%s [error]: SetUpCtrlRegionsAndBuffers failed\n", __func__);
                            DestroyBuffQPair(buffConn);
                            break;
                        }

                        //
                        // Post a receive
                        //
                        //
                        ret = ibv_post_recv(buffConn->QPair, &buffConn->RecvWr, &badRecvWr);
                        if (ret) {
                            fprintf(stderr, "%s [error]: ibv_post_recv failed %d\n", __func__, ret);
                            DestroyBuffRegionsAndBuffers(buffConn);
                            DestroyBuffQPair(buffConn);
                            break;
                        }

                        //
                        // Accept the connection
                        //
                        //
                        memset(&connParam, 0, sizeof(connParam));
                        connParam.responder_resources = BUFF_RECVQ_DEPTH;
                        connParam.initiator_depth = BUFF_SENDQ_DEPTH;
                        ret = rdma_accept(buffConn->RemoteCmId, &connParam);
                        if (ret) {
                            fprintf(stderr, "%s [error]: rdma_accept failed %d\n", __func__, ret);
                            DestroyBuffRegionsAndBuffers(buffConn);
                            DestroyBuffQPair(buffConn);
                            break;
                        }

                        //
                        // Mark this connection unavailable
                        //
                        //
                        buffConn->InUse = TRUE;
                        fprintf(stdout, "Buffer connection #%d is accepted\n", buffConn->BuffId);
                    }
                    else {
                        fprintf(stderr, "No available buffer connection\n");
                        rdma_ack_cm_event(Event);
                    }

                    break;
                }
                default:
                {
                    fprintf(stderr, "CM: unrecognized connection type\n");
                    rdma_ack_cm_event(Event);
                    break;
                }
            }
            break;
        }
        case RDMA_CM_EVENT_ESTABLISHED:
        {
            uint8_t privData;
            int connId = FindConnId(Config, Event->id, &privData);
            if (privData) {
#ifdef DDS_STORAGE_FILE_BACKEND_VERBOSE
                fprintf(stdout, "CM: RDMA_CM_EVENT_ESTABLISHED for Control Conn#%d\n", connId);
#endif
            }
            else
            {
#ifdef DDS_STORAGE_FILE_BACKEND_VERBOSE
                fprintf(stdout, "CM: RDMA_CM_EVENT_ESTABLISHED for Buffer Conn#%d\n", connId);
            }
#endif

            rdma_ack_cm_event(Event);
            break;
        }
        case RDMA_CM_EVENT_ADDR_ERROR:
        case RDMA_CM_EVENT_ROUTE_ERROR:
        case RDMA_CM_EVENT_CONNECT_ERROR:
        case RDMA_CM_EVENT_UNREACHABLE:
        case RDMA_CM_EVENT_REJECTED:
        {
#ifdef DDS_STORAGE_FILE_BACKEND_VERBOSE
            fprintf(stderr, "cma event %s, error %d\n",
                rdma_event_str(Event->event), Event->status);
#endif
            ret = -1;
            rdma_ack_cm_event(Event);
            break;
        }
        case RDMA_CM_EVENT_DISCONNECTED:
        {
            uint8_t privData;
            int connId = FindConnId(Config, Event->id, &privData);
            if (connId >= 0) {
                if (privData) {
                        if (Config->CtrlConns[connId].InUse) {
                            struct CtrlConnConfig *ctrlConn = &Config->CtrlConns[connId];
                            DestroyCtrlRegionsAndBuffers(ctrlConn);
                            DestroyCtrlQPair(ctrlConn);
                            ctrlConn->InUse = FALSE;
                        }
#ifdef DDS_STORAGE_FILE_BACKEND_VERBOSE
                        fprintf(stdout, "CM: RDMA_CM_EVENT_DISCONNECTED for Control Conn#%d\n", connId);
#endif
                }
                else {
                        if (Config->BuffConns[connId].InUse) {
                            struct BuffConnConfig *buffConn = &Config->BuffConns[connId];
                            DestroyBuffRegionsAndBuffers(buffConn);
                            DestroyBuffQPair(buffConn);
                            buffConn->InUse = FALSE;
                        }
#ifdef DDS_STORAGE_FILE_BACKEND_VERBOSE
                    fprintf(stdout, "CM: RDMA_CM_EVENT_DISCONNECTED for Buffer Conn#%d\n", connId);
#endif
                }
            }
            else {
                fprintf(stderr, "CM: RDMA_CM_EVENT_DISCONNECTED with unrecognized control connection\n");
            }
                
            rdma_ack_cm_event(Event);
            break;
        }
        case RDMA_CM_EVENT_DEVICE_REMOVAL:
        {
#ifdef DDS_STORAGE_FILE_BACKEND_VERBOSE
            fprintf(stderr, "CM: RDMA_CM_EVENT_DEVICE_REMOVAL\n");
#endif
            ret = -1;
            rdma_ack_cm_event(Event);
            break;
        }
        default:
#ifdef DDS_STORAGE_FILE_BACKEND_VERBOSE
            fprintf(stderr, "oof bad type!\n");
#endif
            ret = -1;
            rdma_ack_cm_event(Event);
            break;
    }

    return ret;
}

//
// Control message handler
//
//
static inline int
CtrlMsgHandler(
    struct CtrlConnConfig *CtrlConn
) {
    int ret = 0;
    MsgHeader* msgIn = (MsgHeader*)CtrlConn->RecvBuff;
    MsgHeader* msgOut = (MsgHeader*)CtrlConn->SendBuff;

    switch(msgIn->MsgId) {
        //
        // Request client id
        //
        //
        case CTRL_MSG_F2B_REQUEST_ID: {
            CtrlMsgB2FRespondId *resp = (CtrlMsgB2FRespondId *)(msgOut + 1);
            struct ibv_send_wr *badSendWr = NULL;
            struct ibv_recv_wr *badRecvWr = NULL;

            //
            // Post a receive first
            //
            //
            ret = ibv_post_recv(CtrlConn->QPair, &CtrlConn->RecvWr, &badRecvWr);
            if (ret) {
                fprintf(stderr, "%s [error]: ibv_post_recv failed: %d\n", __func__, ret);
                ret = -1;
            }

            //
            // Send the request id
            //
            //
            msgOut->MsgId = CTRL_MSG_B2F_RESPOND_ID;
            resp->ClientId = CtrlConn->CtrlId;
            CtrlConn->SendWr.sg_list->length = sizeof(MsgHeader) + sizeof(CtrlMsgB2FRespondId);
            ret = ibv_post_send(CtrlConn->QPair, &CtrlConn->SendWr, &badSendWr);
            if (ret) {
                fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                ret = -1;
            }
        }
            break;
        //
        // The client wants to terminate
        //
        //
        case CTRL_MSG_F2B_TERMINATE: {
            CtrlMsgF2BTerminate *req = (CtrlMsgF2BTerminate *)(msgIn + 1);

            if (req->ClientId == CtrlConn->CtrlId) {
                DestroyCtrlRegionsAndBuffers(CtrlConn);
                DestroyCtrlQPair(CtrlConn);
                CtrlConn->InUse = FALSE;
#ifdef DDS_STORAGE_FILE_BACKEND_VERBOSE
                fprintf(stdout, "%s [info]: Control Conn#%d is disconnected\n", __func__, req->ClientId);
#endif
            }
            else {
                fprintf(stderr, "%s [error]: mismatched client id\n", __func__);
            }
        }
            break;
        //
        // CreateDirectory request
        //
        //
        case CTRL_MSG_F2B_REQ_CREATE_DIR: {
            CtrlMsgF2BReqCreateDirectory *req = (CtrlMsgF2BReqCreateDirectory *)(msgIn + 1);
            CtrlMsgB2FAckCreateDirectory *resp = (CtrlMsgB2FAckCreateDirectory *)(msgOut + 1);
            struct ibv_send_wr *badSendWr = NULL;
            struct ibv_recv_wr *badRecvWr = NULL;

            //
            // Post a receive first
            //
            //
            ret = ibv_post_recv(CtrlConn->QPair, &CtrlConn->RecvWr, &badRecvWr);
            if (ret) {
                fprintf(stderr, "%s [error]: ibv_post_recv failed: %d\n", __func__, ret);
                ret = -1;
            }

            //
            // Create the directory
            //
            //
            CreateDirectoryHandler(req, resp);

            //
            // Respond
            //
            //
            msgOut->MsgId = CTRL_MSG_B2F_ACK_CREATE_DIR;
            CtrlConn->SendWr.sg_list->length = sizeof(MsgHeader) + sizeof(CtrlMsgB2FAckCreateDirectory);
            ret = ibv_post_send(CtrlConn->QPair, &CtrlConn->SendWr, &badSendWr);
            if (ret) {
                fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                ret = -1;
            }
        }
            break;
        //
        // RemoveDirectory request
        //
        //
        case CTRL_MSG_F2B_REQ_REMOVE_DIR: {
            CtrlMsgF2BReqRemoveDirectory *req = (CtrlMsgF2BReqRemoveDirectory *)(msgIn + 1);
            CtrlMsgB2FAckRemoveDirectory *resp = (CtrlMsgB2FAckRemoveDirectory *)(msgOut + 1);
            struct ibv_send_wr *badSendWr = NULL;
            struct ibv_recv_wr *badRecvWr = NULL;

            //
            // Post a receive first
            //
            //
            ret = ibv_post_recv(CtrlConn->QPair, &CtrlConn->RecvWr, &badRecvWr);
            if (ret) {
                fprintf(stderr, "%s [error]: ibv_post_recv failed: %d\n", __func__, ret);
                ret = -1;
            }

            //
            // Remove the directory
            //
            //
            RemoveDirectoryHandler(req, resp);

            //
            // Respond
            //
            //
            msgOut->MsgId = CTRL_MSG_B2F_ACK_REMOVE_DIR;
            CtrlConn->SendWr.sg_list->length = sizeof(MsgHeader) + sizeof(CtrlMsgB2FAckRemoveDirectory);
            ret = ibv_post_send(CtrlConn->QPair, &CtrlConn->SendWr, &badSendWr);
            if (ret) {
                fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                ret = -1;
            }
        }
            break;
        //
        // CreateFile request
        //
        //
        case CTRL_MSG_F2B_REQ_CREATE_FILE: {
            CtrlMsgF2BReqCreateFile *req = (CtrlMsgF2BReqCreateFile *)(msgIn + 1);
            CtrlMsgB2FAckCreateFile *resp = (CtrlMsgB2FAckCreateFile *)(msgOut + 1);
            struct ibv_send_wr *badSendWr = NULL;
            struct ibv_recv_wr *badRecvWr = NULL;

            //
            // Post a receive first
            //
            //
            ret = ibv_post_recv(CtrlConn->QPair, &CtrlConn->RecvWr, &badRecvWr);
            if (ret) {
                fprintf(stderr, "%s [error]: ibv_post_recv failed: %d\n", __func__, ret);
                ret = -1;
            }

            //
            // Create the file
            //
            //
            CreateFileHandler(req, resp);

            //
            // Respond
            //
            //
            msgOut->MsgId = CTRL_MSG_B2F_ACK_CREATE_FILE;
            CtrlConn->SendWr.sg_list->length = sizeof(MsgHeader) + sizeof(CtrlMsgB2FAckCreateFile);
            ret = ibv_post_send(CtrlConn->QPair, &CtrlConn->SendWr, &badSendWr);
            if (ret) {
                fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                ret = -1;
            }
        }
            break;
        //
        // DeleteFile request
        //
        //
        case CTRL_MSG_F2B_REQ_DELETE_FILE: {
            CtrlMsgF2BReqDeleteFile *req = (CtrlMsgF2BReqDeleteFile *)(msgIn + 1);
            CtrlMsgB2FAckDeleteFile *resp = (CtrlMsgB2FAckDeleteFile *)(msgOut + 1);
            struct ibv_send_wr *badSendWr = NULL;
            struct ibv_recv_wr *badRecvWr = NULL;

            //
            // Post a receive first
            //
            //
            ret = ibv_post_recv(CtrlConn->QPair, &CtrlConn->RecvWr, &badRecvWr);
            if (ret) {
                fprintf(stderr, "%s [error]: ibv_post_recv failed: %d\n", __func__, ret);
                ret = -1;
            }

            //
            // Delete the file
            //
            //
            DeleteFileHandler(req, resp);

            //
            // Respond
            //
            //
            msgOut->MsgId = CTRL_MSG_B2F_ACK_DELETE_FILE;
            CtrlConn->SendWr.sg_list->length = sizeof(MsgHeader) + sizeof(CtrlMsgB2FAckDeleteFile);
            ret = ibv_post_send(CtrlConn->QPair, &CtrlConn->SendWr, &badSendWr);
            if (ret) {
                fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                ret = -1;
            }
        }
            break;
        //
        // ChangeFileSize request
        //
        //
        case CTRL_MSG_F2B_REQ_CHANGE_FILE_SIZE: {
            CtrlMsgF2BReqChangeFileSize *req = (CtrlMsgF2BReqChangeFileSize *)(msgIn + 1);
            CtrlMsgB2FAckChangeFileSize *resp = (CtrlMsgB2FAckChangeFileSize *)(msgOut + 1);
            struct ibv_send_wr *badSendWr = NULL;
            struct ibv_recv_wr *badRecvWr = NULL;

            //
            // Post a receive first
            //
            //
            ret = ibv_post_recv(CtrlConn->QPair, &CtrlConn->RecvWr, &badRecvWr);
            if (ret) {
                fprintf(stderr, "%s [error]: ibv_post_recv failed: %d\n", __func__, ret);
                ret = -1;
            }

            //
            // Change the file size
            //
            //
            ChangeFileSizeHandler(req, resp);

            //
            // Respond
            //
            //
            msgOut->MsgId = CTRL_MSG_B2F_ACK_CHANGE_FILE_SIZE;
            CtrlConn->SendWr.sg_list->length = sizeof(MsgHeader) + sizeof(CtrlMsgB2FAckChangeFileSize);
            ret = ibv_post_send(CtrlConn->QPair, &CtrlConn->SendWr, &badSendWr);
            if (ret) {
                fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                ret = -1;
            }
        }
            break;
        //
        // GetFileSize request
        //
        //
        case CTRL_MSG_F2B_REQ_GET_FILE_SIZE: {
            CtrlMsgF2BReqGetFileSize *req = (CtrlMsgF2BReqGetFileSize *)(msgIn + 1);
            CtrlMsgB2FAckGetFileSize *resp = (CtrlMsgB2FAckGetFileSize *)(msgOut + 1);
            struct ibv_send_wr *badSendWr = NULL;
            struct ibv_recv_wr *badRecvWr = NULL;

            //
            // Post a receive first
            //
            //
            ret = ibv_post_recv(CtrlConn->QPair, &CtrlConn->RecvWr, &badRecvWr);
            if (ret) {
                fprintf(stderr, "%s [error]: ibv_post_recv failed: %d\n", __func__, ret);
                ret = -1;
            }

            //
            // Get the file size
            //
            //
            GetFileSizeHandler(req, resp);

            //
            // Respond
            //
            //
            msgOut->MsgId = CTRL_MSG_B2F_ACK_CHANGE_FILE_SIZE;
            CtrlConn->SendWr.sg_list->length = sizeof(MsgHeader) + sizeof(CtrlMsgB2FAckGetFileSize);
            ret = ibv_post_send(CtrlConn->QPair, &CtrlConn->SendWr, &badSendWr);
            if (ret) {
                fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                ret = -1;
            }
        }
            break;
        //
        // GetFileInformationById request
        //
        //
        case CTRL_MSG_F2B_REQ_GET_FILE_INFO: {
            CtrlMsgF2BReqGetFileInfo *req = (CtrlMsgF2BReqGetFileInfo *)(msgIn + 1);
            CtrlMsgB2FAckGetFileInfo *resp = (CtrlMsgB2FAckGetFileInfo *)(msgOut + 1);
            struct ibv_send_wr *badSendWr = NULL;
            struct ibv_recv_wr *badRecvWr = NULL;

            //
            // Post a receive first
            //
            //
            ret = ibv_post_recv(CtrlConn->QPair, &CtrlConn->RecvWr, &badRecvWr);
            if (ret) {
                fprintf(stderr, "%s [error]: ibv_post_recv failed: %d\n", __func__, ret);
                ret = -1;
            }

            //
            // Get the file info
            //
            //
            GetFileInformationByIdHandler(req, resp);

            //
            // Respond
            //
            //
            msgOut->MsgId = CTRL_MSG_B2F_ACK_GET_FILE_INFO;
            CtrlConn->SendWr.sg_list->length = sizeof(MsgHeader) + sizeof(CtrlMsgB2FAckGetFileInfo);
            ret = ibv_post_send(CtrlConn->QPair, &CtrlConn->SendWr, &badSendWr);
            if (ret) {
                fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                ret = -1;
            }
        }
            break;
        //
        // GetFileAttributes request
        //
        //
        case CTRL_MSG_F2B_REQ_GET_FILE_ATTR: {
            CtrlMsgF2BReqGetFileAttr *req = (CtrlMsgF2BReqGetFileAttr *)(msgIn + 1);
            CtrlMsgB2FAckGetFileAttr *resp = (CtrlMsgB2FAckGetFileAttr *)(msgOut + 1);
            struct ibv_send_wr *badSendWr = NULL;
            struct ibv_recv_wr *badRecvWr = NULL;

            //
            // Post a receive first
            //
            //
            ret = ibv_post_recv(CtrlConn->QPair, &CtrlConn->RecvWr, &badRecvWr);
            if (ret) {
                fprintf(stderr, "%s [error]: ibv_post_recv failed: %d\n", __func__, ret);
                ret = -1;
            }

            //
            // Get the file attributes
            //
            //
            GetFileAttributesHandler(req, resp);

            //
            // Respond
            //
            //
            msgOut->MsgId = CTRL_MSG_B2F_ACK_GET_FILE_ATTR;
            CtrlConn->SendWr.sg_list->length = sizeof(MsgHeader) + sizeof(CtrlMsgB2FAckGetFileAttr);
            ret = ibv_post_send(CtrlConn->QPair, &CtrlConn->SendWr, &badSendWr);
            if (ret) {
                fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                ret = -1;
            }
        }
            break;
        //
        // GetStorageFreeSpace request
        //
        //
        case CTRL_MSG_F2B_REQ_GET_FREE_SPACE: {
            CtrlMsgF2BReqGetFreeSpace *req = (CtrlMsgF2BReqGetFreeSpace *)(msgIn + 1);
            CtrlMsgB2FAckGetFreeSpace *resp = (CtrlMsgB2FAckGetFreeSpace *)(msgOut + 1);
            struct ibv_send_wr *badSendWr = NULL;
            struct ibv_recv_wr *badRecvWr = NULL;

            //
            // Post a receive first
            //
            //
            ret = ibv_post_recv(CtrlConn->QPair, &CtrlConn->RecvWr, &badRecvWr);
            if (ret) {
                fprintf(stderr, "%s [error]: ibv_post_recv failed: %d\n", __func__, ret);
                ret = -1;
            }

            //
            // Get the free storage space
            //
            //
            GetStorageFreeSpaceHandler(req, resp);

            //
            // Respond
            //
            //
            msgOut->MsgId = CTRL_MSG_B2F_ACK_GET_FREE_SPACE;
            CtrlConn->SendWr.sg_list->length = sizeof(MsgHeader) + sizeof(CtrlMsgB2FAckGetFreeSpace);
            ret = ibv_post_send(CtrlConn->QPair, &CtrlConn->SendWr, &badSendWr);
            if (ret) {
                fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                ret = -1;
            }
        }
            break;
        //
        // MoveFile request
        //
        //
        case CTRL_MSG_F2B_REQ_MOVE_FILE: {
            CtrlMsgF2BReqMoveFile *req = (CtrlMsgF2BReqMoveFile *)(msgIn + 1);
            CtrlMsgB2FAckMoveFile *resp = (CtrlMsgB2FAckMoveFile *)(msgOut + 1);
            struct ibv_send_wr *badSendWr = NULL;
            struct ibv_recv_wr *badRecvWr = NULL;

            //
            // Post a receive first
            //
            //
            ret = ibv_post_recv(CtrlConn->QPair, &CtrlConn->RecvWr, &badRecvWr);
            if (ret) {
                fprintf(stderr, "%s [error]: ibv_post_recv failed: %d\n", __func__, ret);
                ret = -1;
            }

            //
            // Move the file
            //
            //
            MoveFileHandler(req, resp);

            //
            // Respond
            //
            //
            msgOut->MsgId = CTRL_MSG_B2F_ACK_GET_FREE_SPACE;
            CtrlConn->SendWr.sg_list->length = sizeof(MsgHeader) + sizeof(CtrlMsgB2FAckMoveFile);
            ret = ibv_post_send(CtrlConn->QPair, &CtrlConn->SendWr, &badSendWr);
            if (ret) {
                fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                ret = -1;
            }
        }
            break;
        default:
            fprintf(stderr, "%s [error]: unrecognized control message\n", __func__);
            ret = -1;
            break;
    }

    return ret;
}

//
// Process communication channel events for contorl connections
//
//
static int inline
ProcessCtrlCqEvents(
    struct BackEndConfig *Config
) {
    int ret = 0;
    struct CtrlConnConfig *ctrlConn = NULL;
    struct ibv_wc wc;

    for (int i = 0; i != Config->MaxClients; i++) {
        ctrlConn = &Config->CtrlConns[i];
        if (!ctrlConn->InUse) {
            continue;
        }

        if ((ret = ibv_poll_cq(ctrlConn->CompQ, 1, &wc)) == 1) {
            ret = 0;
            if (wc.status != IBV_WC_SUCCESS) {
                fprintf(stderr, "%s [error]: ibv_poll_cq failed status %d\n", __func__, wc.status);
                ret = -1;
                continue;
            }

            //
            // Only receive events are expected
            //
            //
            switch(wc.opcode) {
                case IBV_WC_RECV: {
                    ret = CtrlMsgHandler(ctrlConn);
                    if (ret) {
                        fprintf(stderr, "%s [error]: CtrlMsgHandler failed\n", __func__);
                        goto ProcessCtrlCqEventsReturn;
                    }
                }
                    break;
                case IBV_WC_SEND:
                case IBV_WC_RDMA_WRITE:
                case IBV_WC_RDMA_READ:
                    break;

                default:
                    fprintf(stderr, "%s [error]: unknown completion\n", __func__);
                    ret = -1;
                    break;
            }
        }
    }

ProcessCtrlCqEventsReturn:
    return ret;
}

//
// Buffer message handler
//
//
static inline int
BuffMsgHandler(
    struct BuffConnConfig *BuffConn
) {
    int ret = 0;
    MsgHeader* msgIn = (MsgHeader*)BuffConn->RecvBuff;
    MsgHeader* msgOut = (MsgHeader*)BuffConn->SendBuff;

    switch(msgIn->MsgId) {
        case BUFF_MSG_F2B_REQUEST_ID: {
            BuffMsgF2BRequestId *req = (BuffMsgF2BRequestId *)(msgIn + 1);
            BuffMsgB2FRespondId *resp = (BuffMsgB2FRespondId *)(msgOut + 1);
            struct ibv_send_wr *badSendWr = NULL;
            struct ibv_recv_wr *badRecvWr = NULL;

            //
            // Post a receive first
            //
            //
            ret = ibv_post_recv(BuffConn->QPair, &BuffConn->RecvWr, &badRecvWr);
            if (ret) {
                fprintf(stderr, "%s [error]: ibv_post_recv failed %d\n", __func__, ret);
                ret = -1;
            }

            //
            // Update config and send the buffer id
            //
            //
            BuffConn->CtrlId = req->ClientId;
            InitializeRingBufferBackEnd(
                &BuffConn->RequestRing,
                &BuffConn->ResponseRing,
                req->BufferAddress,
                req->AccessToken,
                req->Capacity
            );

            BuffConn->RequestDMAReadMetaWr.wr.rdma.remote_addr = BuffConn->RequestRing.ReadMetaAddr;
            BuffConn->RequestDMAReadMetaWr.wr.rdma.rkey = BuffConn->RequestRing.AccessToken;
            BuffConn->RequestDMAWriteMetaWr.wr.rdma.remote_addr = BuffConn->RequestRing.WriteMetaAddr;
            BuffConn->RequestDMAWriteMetaWr.wr.rdma.rkey = BuffConn->RequestRing.AccessToken;
            BuffConn->RequestDMAReadDataWr.wr.rdma.rkey = BuffConn->RequestRing.AccessToken;
            
            BuffConn->ResponseDMAReadMetaWr.wr.rdma.remote_addr = BuffConn->ResponseRing.ReadMetaAddr;
            BuffConn->ResponseDMAReadMetaWr.wr.rdma.rkey = BuffConn->ResponseRing.AccessToken;
            BuffConn->ResponseDMAWriteMetaWr.wr.rdma.remote_addr = BuffConn->ResponseRing.WriteMetaAddr;
            BuffConn->ResponseDMAWriteMetaWr.wr.rdma.rkey = BuffConn->ResponseRing.AccessToken;
            BuffConn->ResponseDMAWriteDataWr.wr.rdma.rkey = BuffConn->ResponseRing.AccessToken;

            msgOut->MsgId = BUFF_MSG_B2F_RESPOND_ID;
            resp->BufferId = BuffConn->BuffId;
            BuffConn->SendWr.sg_list->length = sizeof(MsgHeader) + sizeof(BuffMsgB2FRespondId);
            ret = ibv_post_send(BuffConn->QPair, &BuffConn->SendWr, &badSendWr);
            if (ret) {
                fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                ret = -1;
            }
            
#ifdef DDS_STORAGE_FILE_BACKEND_VERBOSE
            fprintf(stdout, "%s [info]: Buffer Conn#%d is for Client#%d\n", __func__, BuffConn->BuffId, BuffConn->CtrlId);
            fprintf(stdout, "- Buffer address: %p\n", (void*)BuffConn->RequestRing.RemoteAddr);
            fprintf(stdout, "- Buffer capacity: %u\n", BuffConn->RequestRing.Capacity);
            fprintf(stdout, "- Access token: %x\n", BuffConn->RequestRing.AccessToken);
            fprintf(stdout, "- Request ring data base address: %p\n", (void*)BuffConn->RequestRing.DataBaseAddr);
            fprintf(stdout, "- Response ring data base address: %p\n", (void*)BuffConn->ResponseRing.DataBaseAddr);
#endif
            //
            // Start polling requests
            //
            //
            ret = ibv_post_send(BuffConn->QPair, &BuffConn->RequestDMAReadMetaWr, &badSendWr);
            if (ret) {
                fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                ret = -1;
            }
        }
            break;
        case BUFF_MSG_F2B_RELEASE: {
            BuffMsgF2BRelease *req = (BuffMsgF2BRelease *)(msgIn + 1);

            if (req->BufferId == BuffConn->BuffId && req->ClientId == BuffConn->CtrlId) {
                DestroyBuffRegionsAndBuffers(BuffConn);
                DestroyBuffQPair(BuffConn);
                BuffConn->InUse = 0;
#ifdef DDS_STORAGE_FILE_BACKEND_VERBOSE
                fprintf(stdout, "%s [info]: Buffer Conn#%d (Client#%d) is disconnected\n", __func__, req->BufferId, req->ClientId);
#endif
            }
            else {
                fprintf(stderr, "%s [error]: mismatched client id\n", __func__);
            }
        }
            break;
        default:
            fprintf(stderr, "%s [error]: unrecognized control message\n", __func__);
            ret = -1;
            break;
    }

    return ret;
}

//
// Execute received requests
//
//
static inline void
ExecuteRequests(
    struct BuffConnConfig* BuffConn
) {
    char* buffReq;
    char* buffResp;
    char* curReq;
    BuffMsgF2BReqHeader* curReqObj;
    FileIOSizeT curReqSize;
    FileIOSizeT bytesParsed = 0;
    FileIOSizeT bytesTotal = BuffConn->RequestDMAReadDataSize;

    int tailReq = BuffConn->RequestRing.Head;
    int headReq = tailReq >= bytesTotal ? tailReq - bytesTotal : BACKEND_REQUEST_BUFFER_SIZE + tailReq - bytesTotal;
    int tailResp = BuffConn->ResponseRing.AggressiveTail;
    int headResp = BuffConn->ResponseRing.Tail;
    int respRingCapacity = tailResp >= headResp ? (BACKEND_RESPONSE_BUFFER_SIZE - tailResp + headResp) : (headResp - tailResp);
    
    SplittableBuffer dataBuff;
    FileIOSizeT respSize = 0;
    FileIOSizeT totalRespSize = 0;
    int progressReq = headReq;
    int progressResp = tailResp;

    //
    // Parse all file requests in the batch
    //
    //
    buffReq = BuffConn->RequestDMAReadDataBuff;
    buffResp = BuffConn->ResponseDMAWriteDataBuff;
    while (bytesParsed != bytesTotal) {
        curReq = buffReq + progressReq;

        bytesParsed += *(FileIOSizeT*)(curReq);
        progressReq += *(FileIOSizeT*)(curReq);
        if (progressReq >= BACKEND_REQUEST_BUFFER_SIZE) {
            progressReq %= BACKEND_REQUEST_BUFFER_SIZE;
        }

        curReqSize = *(FileIOSizeT*)(curReq) - sizeof(FileIOSizeT);
        curReq += sizeof(FileIOSizeT);

        if (curReqSize > sizeof(BuffMsgF2BReqHeader)) {
            //
            // Process a write request
            // Allocate a response first, no need to check alignment
            //
            //
            respSize = sizeof(FileIOSizeT) + sizeof(BuffMsgB2FAckHeader);

            //
            // Record the size of the this response on the response ring
            //
            //
            *(FileIOSizeT*)(buffResp + progressResp) = respSize;

            BuffMsgB2FAckHeader *resp = (BuffMsgB2FAckHeader *)(buffResp + progressResp + sizeof(FileIOSizeT));
            resp->Result = DDS_ERROR_CODE_IO_PENDING;
            
            progressResp += respSize;
            totalRespSize += respSize;
            if (progressResp >= BACKEND_RESPONSE_BUFFER_SIZE) {
                progressResp %= BACKEND_RESPONSE_BUFFER_SIZE;
            }

            //
            // Extract write source buffer from the request ring
            //
            //
            curReqObj = (BuffMsgF2BReqHeader*)curReq;
            dataBuff.TotalSize = curReqObj->Bytes;
            dataBuff.FirstAddr = buffReq + progressReq;
            if (progressReq + dataBuff.TotalSize >= BACKEND_REQUEST_BUFFER_SIZE) {
                dataBuff.FirstSize = BACKEND_REQUEST_BUFFER_SIZE - progressReq;
                dataBuff.SecondAddr = buffReq;
                progressReq = dataBuff.TotalSize - dataBuff.FirstSize;
            }
            else {
                dataBuff.FirstSize = curReqObj->Bytes;
                dataBuff.SecondAddr = NULL;
                progressReq += dataBuff.TotalSize;
            }
            WriteHandler(curReqObj, resp, &dataBuff);
        }
        else {
            //
            // Process a read request
            // Allocate a response first, need to check alignment
            //
            //
            RingSizeT alignment = sizeof(FileIOSizeT) + sizeof(BuffMsgB2FAckHeader);
            curReqObj = (BuffMsgF2BReqHeader*)curReq;
            respSize = alignment + curReqObj->Bytes;
            if (respSize % alignment != 0) {
                respSize += (alignment - (respSize % alignment));
            }
            
            //
            // Record the size of the this response on the response ring
            //
            //
            *(FileIOSizeT*)(buffResp + progressResp) = respSize;

            BuffMsgB2FAckHeader *resp = (BuffMsgB2FAckHeader *)(buffResp + progressResp + sizeof(FileIOSizeT));
            resp->Result = DDS_ERROR_CODE_IO_PENDING;

            //
            // Extract read destination buffer from the response ring
            //
            //
            dataBuff.TotalSize = curReqObj->Bytes;
            if (progressResp + respSize <= BACKEND_RESPONSE_BUFFER_SIZE) {
                dataBuff.FirstAddr = buffResp + (progressResp + alignment);
                dataBuff.FirstSize = dataBuff.TotalSize;
                dataBuff.SecondAddr = NULL;
            }
            else {
                if (progressResp + alignment < BACKEND_RESPONSE_BUFFER_SIZE) {
                    dataBuff.FirstAddr = buffResp + (progressResp + alignment);
                    dataBuff.FirstSize = BACKEND_RESPONSE_BUFFER_SIZE - progressResp - alignment;
                    dataBuff.SecondAddr = buffResp + (dataBuff.TotalSize - dataBuff.FirstSize);
                }
                else {
                    dataBuff.FirstAddr = buffResp;
                    dataBuff.FirstSize = dataBuff.TotalSize;
                    dataBuff.SecondAddr = NULL;
                }
            }
            
            progressResp += respSize;
            totalRespSize += respSize;
            if (progressResp >= BACKEND_RESPONSE_BUFFER_SIZE) {
                progressResp %= BACKEND_RESPONSE_BUFFER_SIZE;
            }

            ReadHandler(curReqObj, resp, &dataBuff);
        }
    }

    //
    // Update response buffer tail
    //
    //
    if (totalRespSize >= respRingCapacity) {
        //
        // If this happens, increase response buffer size
        // TODO: a mechanism that holds the execution of requests
        //       until responses are drained and enough space is available
        //
        //
        fprintf(stderr, "%s [error]: Response buffer is corrupted!\n", __func__);
        exit(-1);
    }
    BuffConn->ResponseRing.AggressiveTail = progressResp;
}

//
// Process communication channel events for buffer connections
//
//
static inline int
ProcessBuffCqEvents(
    struct BackEndConfig *Config
) {
    int ret = 0;
    struct BuffConnConfig *buffConn = NULL;
    struct ibv_send_wr *badSendWr = NULL;
    struct ibv_wc wc;

    for (int i = 0; i != Config->MaxBuffs; i++) {
        buffConn = &Config->BuffConns[i];
        if (!buffConn->InUse) {
            continue;
        }

        if ((ret = ibv_poll_cq(buffConn->CompQ, 1, &wc)) == 1) {
            ret = 0;
            if (wc.status != IBV_WC_SUCCESS) {
                fprintf(stderr, "%s [error]: ibv_poll_cq failed status %d\n", __func__, wc.status);
                ret = -1;
                continue;
            }

            switch(wc.opcode) {
                case IBV_WC_RECV: {
                    ret = BuffMsgHandler(buffConn);
                    if (ret) {
                        fprintf(stderr, "%s [error]: BuffMsgHandler failed\n", __func__);
                        goto ProcessBuffCqEventsReturn;
                    }
                }
                    break;
                case IBV_WC_RDMA_READ: {
                    switch (wc.wr_id)
                    {
                    case BUFF_READ_REQUEST_META_WR_ID: {
                        //
                        // Process a meta read
                        //
                        //
                        int* pointers = (int*)buffConn->RequestDMAReadMetaBuff;
                        int progress = pointers[0];
                        int tail = pointers[DDS_CACHE_LINE_SIZE_BY_INT];
                        if (tail == buffConn->RequestRing.Head || tail != progress) {
                            //
                            // Not ready to read, poll again
                            //
                            //
                            ret = ibv_post_send(buffConn->QPair, &buffConn->RequestDMAReadMetaWr, &badSendWr);
                            if (ret) {
                                fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                                ret = -1;
                            }
                        }
                        else {
                            //
                            // Ready to read
                            //
                            //
                            RingSizeT availBytes = 0;
                            uint64_t sourceBuffer1 = 0;
                            uint64_t sourceBuffer2 = 0;
                            int head = buffConn->RequestRing.Head;

                            if (progress > head) {
                                availBytes = progress - head;
                                buffConn->RequestDMAReadDataSize = availBytes;
                                sourceBuffer1 = buffConn->RequestRing.DataBaseAddr + head;
                            }
                            else {
                                availBytes = DDS_REQUEST_RING_BYTES - head;
                                buffConn->RequestDMAReadDataSize = availBytes + progress;
                                sourceBuffer1 = buffConn->RequestRing.DataBaseAddr + head;
                                sourceBuffer2 = buffConn->RequestRing.DataBaseAddr;
                            }

                            //
                            // Post a DMA read, by making DPU buffer a mirror of the host buffer
                            //
                            //
                            buffConn->RequestDMAReadDataWr.wr.rdma.remote_addr = sourceBuffer1;
                            buffConn->RequestDMAReadDataWr.sg_list->addr = (uint64_t)(buffConn->RequestDMAReadDataBuff + head);
                            buffConn->RequestDMAReadDataWr.sg_list->length = availBytes;

                            if (sourceBuffer2) {
                                buffConn->RequestDMAReadDataSplitState = BUFF_READ_DATA_SPLIT_STATE_SPLIT;

                                ret = ibv_post_send(buffConn->QPair, &buffConn->RequestDMAReadDataWr, &badSendWr);
                                if (ret) {
                                    fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                                    ret = -1;
                                }

                                buffConn->RequestDMAReadDataSplitWr.sg_list->addr = (uint64_t)(buffConn->RequestDMAReadDataBuff);
                                buffConn->RequestDMAReadDataSplitWr.sg_list->length = progress;
                                buffConn->RequestDMAReadDataSplitWr.wr.rdma.remote_addr = sourceBuffer2;

                                ret = ibv_post_send(buffConn->QPair, &buffConn->RequestDMAReadDataSplitWr, &badSendWr);
                                if (ret) {
                                    fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                                    ret = -1;
                                }
                            }
                            else {
                                buffConn->RequestDMAReadDataSplitState = BUFF_READ_DATA_SPLIT_STATE_NOT_SPLIT;

                                ret = ibv_post_send(buffConn->QPair, &buffConn->RequestDMAReadDataWr, &badSendWr);
                                if (ret) {
                                    fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                                    ret = -1;
                                }
                            }

                            buffConn->RequestRing.Head = progress;
                            
                            //
                            // Immediately update remote head, assuming DMA requests are exected in order
                            //
                            //
                            ret = ibv_post_send(buffConn->QPair, &buffConn->RequestDMAWriteMetaWr, &badSendWr);
                            if (ret) {
                                fprintf(stderr, "%s [error]: ibv_post_send failed: %d (%s)\n", __func__, ret, strerror(ret));
                                ret = -1;
                            }
                        }
                    }
                        break;
                    case BUFF_READ_REQUEST_DATA_WR_ID: {
                        //
                        // Check splitting and update the head
                        //
                        //
                        if (buffConn->RequestDMAReadDataSplitState == BUFF_READ_DATA_SPLIT_STATE_NOT_SPLIT) {
                            //
                            // Execute all the requests
                            //
                            //
                            ExecuteRequests(buffConn);
                        }
                        else {
                            buffConn->RequestDMAReadDataSplitState++;
                        }
                    }
                        break;
                    case BUFF_READ_REQUEST_DATA_SPLIT_WR_ID: {
                        //
                        // Check splitting and update the head
                        //
                        //
                        if (buffConn->RequestDMAReadDataSplitState == BUFF_READ_DATA_SPLIT_STATE_NOT_SPLIT) {
                            //
                            // Execute all the requests
                            //
                            //
                            ExecuteRequests(buffConn);
                        }
                        else {
                            buffConn->RequestDMAReadDataSplitState++;
                        }
                    }
                        break;
		    case BUFF_READ_RESPONSE_META_WR_ID: {
                        //
                        // Process a response meta read
                        //
                        //
                        int* pointers = (int*)buffConn->ResponseDMAReadMetaBuff;
                        int progress = pointers[0];
                        int head = pointers[DDS_CACHE_LINE_SIZE_BY_INT];
                        int tail = buffConn->ResponseRing.Tail;
                        
                        RingSizeT distance = 0;
                        if (head != progress) {
                            //
                            // Not ready to write, poll again
                            //
                            //
                            ret = ibv_post_send(buffConn->QPair, &buffConn->ResponseDMAReadMetaWr, &badSendWr);
                            if (ret) {
                                fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                                ret = -1;
                            }
                            break;
                        }

                        if (tail >= head) {
                            distance = head + DDS_RESPONSE_RING_BYTES - tail;
                        }
                        else {
                            distance = head - tail;
                        }

                        if (distance < buffConn->ResponseDMAWriteDataSize) {
                            //
                            // Not ready to write, poll again
                            //
                            //
                            ret = ibv_post_send(buffConn->QPair, &buffConn->ResponseDMAReadMetaWr, &badSendWr);
                            if (ret) {
                                fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                                ret = -1;
                            }
                            break;
                        }
                        else {
                            //
                            // Ready to write
                            //
                            //
                            FileIOSizeT totalResponseBytes = 0;
                            RingSizeT availBytes = 0;
                            uint64_t sourceBuffer1 = 0;
                            uint64_t sourceBuffer2 = 0;

                            totalResponseBytes = buffConn->ResponseDMAWriteDataSize;
                            if (tail + totalResponseBytes <= DDS_RESPONSE_RING_BYTES) {
                                //
                                // No split
                                //
                                //
                                availBytes = totalResponseBytes;
                                sourceBuffer1 = (uint64_t)(buffConn->ResponseRing.DataBaseAddr + tail);
                            }
                            else {
                                //
                                // Split
                                //
                                //
                                availBytes = DDS_RESPONSE_RING_BYTES - tail;
                                sourceBuffer1 = (uint64_t)(buffConn->ResponseRing.DataBaseAddr + tail);
                                sourceBuffer2 = (uint64_t)(buffConn->ResponseRing.DataBaseAddr);
                            }

                            //
                            // Post DMA writes
                            //
                            //
                            buffConn->ResponseDMAWriteDataWr.wr.rdma.remote_addr = sourceBuffer1;
                            buffConn->ResponseDMAWriteDataWr.sg_list->length = availBytes;

                            if (sourceBuffer2) {
                                buffConn->ResponseDMAWriteDataSplitState = BUFF_READ_DATA_SPLIT_STATE_SPLIT;

                                ret = ibv_post_send(buffConn->QPair, &buffConn->ResponseDMAWriteDataWr, &badSendWr);
                                if (ret) {
                                    fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                                    ret = -1;
                                }

                                buffConn->ResponseDMAWriteDataSplitWr.sg_list->addr = (uint64_t)(buffConn->ResponseDMAWriteDataBuff + availBytes);
                                buffConn->ResponseDMAWriteDataSplitWr.sg_list->length = totalResponseBytes - availBytes;
                                buffConn->ResponseDMAWriteDataSplitWr.wr.rdma.remote_addr = sourceBuffer2;

                                ret = ibv_post_send(buffConn->QPair, &buffConn->ResponseDMAWriteDataSplitWr, &badSendWr);
                                if (ret) {
                                    fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                                    ret = -1;
                                }
                            }
                            else {
                                buffConn->ResponseDMAWriteDataSplitState = BUFF_READ_DATA_SPLIT_STATE_NOT_SPLIT;

                                ret = ibv_post_send(buffConn->QPair, &buffConn->ResponseDMAWriteDataWr, &badSendWr);
                                if (ret) {
                                    fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                                    ret = -1;
                                }
                            }

                            buffConn->ResponseRing.Tail = (tail + totalResponseBytes) % DDS_RESPONSE_RING_BYTES;
                            
                            //
                            // Immediately update remote tail, assuming DMA requests are exected in order
                            //
                            //
                            ret = ibv_post_send(buffConn->QPair, &buffConn->ResponseDMAWriteMetaWr, &badSendWr);
                            if (ret) {
                                fprintf(stderr, "%s [error]: ibv_post_send failed: %d (%s)\n", __func__, ret, strerror(ret));
                                ret = -1;
                            }
                        }
                    }
                        break;
                    default:
                        fprintf(stderr, "%s [error]: unknown read completion\n", __func__);
                        break;
                    }
                }
                    break;
                case IBV_WC_RDMA_WRITE: {
                    switch (wc.wr_id)
                    {
                    case BUFF_WRITE_REQUEST_META_WR_ID: {
                        //
                        // Ready to poll
                        //
                        //
                        ret = ibv_post_send(buffConn->QPair, &buffConn->RequestDMAReadMetaWr, &badSendWr);
                        if (ret) {
                            fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                            ret = -1;
                        }
                    }
                        break;
                    case BUFF_WRITE_RESPONSE_META_WR_ID: {
                        //
                        // There is nothing to do here because response completions are checked in the big loop
                        //
                        //
                    }
                        break;
                    case BUFF_WRITE_RESPONSE_DATA_WR_ID: {
                        //
                        // Check splitting and update the head
                        //
                        //
                        if (buffConn->ResponseDMAWriteDataSplitState == BUFF_READ_DATA_SPLIT_STATE_NOT_SPLIT) {
                            //
                            // There is nothing to do here because response completions are checked in the big loop
                            //
                            //
                        }
                        else {
                            buffConn->ResponseDMAWriteDataSplitState++;
                        }
                    }
                        break;
                    case BUFF_WRITE_RESPONSE_DATA_SPLIT_WR_ID: {
                        //
                        // Check splitting and update the head
                        //
                        //
                        if (buffConn->ResponseDMAWriteDataSplitState == BUFF_READ_DATA_SPLIT_STATE_NOT_SPLIT) {
                            //
                            // There is nothing to do here because response completions are checked in the big loop
                            //
                            //
                        }
                        else {
                            buffConn->ResponseDMAWriteDataSplitState++;
                        }
                    }
                        break;
                    default:
                        fprintf(stderr, "%s [error]: unknown write completion\n", __func__);
                        ret = -1;
                        break;
                    }
                }
                    break;
                case IBV_WC_SEND:
                    break;
                default:
                    fprintf(stderr, "%s [error]: unknown completion\n", __func__);
                    ret = -1;
                    break;
            }
        }
    }

ProcessBuffCqEventsReturn:
    return ret;
}

struct runFileBackEndArgs
{
    const char* ServerIpStr;
    const int ServerPort;
    const uint32_t MaxClients;
    const uint32_t MaxBuffs;
};


//
// Check and process I/O completions
//
//
static inline int
CheckAndProcessIOCompletions(
    struct BackEndConfig *Config
) {
    int ret = 0;
    struct BuffConnConfig *buffConn = NULL;

    for (int i = 0; i != Config->MaxBuffs; i++) {
        buffConn = &Config->BuffConns[i];
        if (!buffConn->InUse) {
            continue;
        }

        //
        // Check the error code of each response, sequentially
        //
        //
        int head = buffConn->ResponseRing.Tail;
        int tail = buffConn->ResponseRing.AggressiveTail;
        char* buffResp = buffConn->ResponseDMAWriteDataBuff;
        char* curResp;
        FileIOSizeT curRespSize;

        while (head != tail) {
            curResp = buffResp + head;
            curRespSize = *(FileIOSizeT*)curResp;
            curResp += sizeof(FileIOSizeT);
            if (((BuffMsgB2FAckHeader*)curResp)->Result == DDS_ERROR_CODE_IO_PENDING) {
                break;
            }

            head += curRespSize;
            if (head >= BACKEND_RESPONSE_BUFFER_SIZE) {
                head %= BACKEND_RESPONSE_BUFFER_SIZE;
            }
        }

        //
        // Do we batch?
        // TODO: inherit the batching effect in requests
        //
        //
        if (head != buffConn->ResponseRing.Tail) {
            //
            // Send the response back to the host
            //
            //
            struct ibv_send_wr *badSendWr = NULL;
            int oldHead = buffConn->ResponseRing.Tail;
            buffConn->ResponseDMAWriteDataSize = head > oldHead ? (head - oldHead) : (BACKEND_RESPONSE_BUFFER_SIZE - oldHead + head);
            
            //
            // Poll the distance from the host
            //
            //
            ret = ibv_post_send(buffConn->QPair, &buffConn->ResponseDMAReadMetaWr, &badSendWr);
            if (ret) {
                fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                ret = -1;
            }
        }
    }

    return ret;  
}

//
// The entry point for the back end
//
// Jason: this is the mainloop, so it needs to be supplied to SPDK's `spdk_app_start`, which will call it
// `spdk_app_start` itself will block until spdk_app_stop() is called, or an error occured during start
//
//
void RunFileBackEnd(
    void *args
) {
    struct runFileBackEndArgs *thisArgs = (struct runFileBackEndArgs *) args;
    const char* ServerIpStr = thisArgs->ServerIpStr;
    const int ServerPort = thisArgs->ServerPort;
    const uint32_t MaxClients = thisArgs->MaxClients;
    const uint32_t MaxBuffs = thisArgs->MaxBuffs;

    struct BackEndConfig config;
    struct rdma_cm_event *event;
    int ret = 0;

    //
    // Initialize the back end configuration
    //
    //
    config.ServerIp = inet_addr(ServerIpStr);
    config.ServerPort = htons(ServerPort);;
    config.MaxClients = MaxClients;
    config.MaxBuffs = MaxBuffs;
    config.CtrlConns = NULL;
    config.BuffConns = NULL;
    config.DMAConf.CmChannel = NULL;
    config.DMAConf.CmId = NULL;

    //
    // Initialize SPDK stuff
    //
    //
    SPDKContextT *SPDKContext = malloc(sizeof(SPDKContextT));
    char* G_BDEV_NAME = "Malloc0";
    SPDKContext->bdev_name = G_BDEV_NAME;

    SPDK_NOTICELOG("Successfully started the application\n");
    ret = spdk_bdev_open_ext(SPDKContext->bdev_name, true, dds_bdev_event_cb, NULL,
				SPDKContext->bdev_desc);
	if (ret) {
		SPDK_ERRLOG("Could not open bdev: %s\n", SPDKContext->bdev_name);
		spdk_app_stop(-1);
		return;
	}

    /* A bdev pointer is valid while the bdev is opened. */
	SPDKContext->bdev = spdk_bdev_desc_get_bdev(SPDKContext->bdev_desc);
    SPDK_NOTICELOG("Opening io channel\n");
	/* Open I/O channel */
	SPDKContext->bdev_io_channel = spdk_bdev_get_io_channel(SPDKContext->bdev_desc);
	if (SPDKContext->bdev_io_channel == NULL) {
		SPDK_ERRLOG("Could not create bdev I/O channel!!\n");
		spdk_bdev_close(SPDKContext->bdev_desc);
		spdk_app_stop(-1);
		return;
	}

    // TODO: maybe spdk malloc buffer here? See bdev.c sample

    /* Allocate memory for the write buffer.
	 * Initialize the write buffer with the string "Hello World!"
	 */
	SPDKContext->buff_size = 2* ONE_GB;
	uint32_t buf_align = spdk_bdev_get_buf_align(SPDKContext->bdev);
	SPDKContext->buff = spdk_dma_zmalloc(SPDKContext->buff_size, buf_align, NULL);
	if (!SPDKContext->buff) {
		SPDK_ERRLOG("Failed to allocate buffer\n");
		spdk_put_io_channel(SPDKContext->bdev_io_channel);
		spdk_bdev_close(SPDKContext->bdev_desc);
		spdk_app_stop(-1);
		return;
	}
	
    
    //
    // Initialize Storage, we are now using a global DPUStorage *, potentially cross thread
    //
    //
    /* struct DPUStorage* Sto = BackEndStorage(); */
    ErrorCodeT result = Initialize(Sto, SPDKContext);
    if (result != DDS_ERROR_CODE_SUCCESS){
        fprintf(stderr, "InitStorage failed with %d\n", result);
        return;
    }

    //
    // Initialize DMA
    //
    //
    ret = InitDMA(&config.DMAConf, config.ServerIp, config.ServerPort);
    if (ret) {
        fprintf(stderr, "InitDMA failed with %d\n", ret);
        return;
    }

    //
    // Allocate connections
    //
    //
    ret = AllocConns(&config);
    if (ret) {
        fprintf(stderr, "AllocConns failed with %d\n", ret);
        TermDMA(&config.DMAConf);
        return;
    }

    //
    // Listen for incoming connections
    //
    //
    ret = rdma_listen(config.DMAConf.CmId, LISTEN_BACKLOG);
    if (ret) {
        ret = errno;
        fprintf(stderr, "rdma_listen error %d\n", ret);
        return;
    }

    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);

    while (ForceQuitFileBackEnd == 0) {
        //
        // Process connection events
        //
        //
        ret = rdma_get_cm_event(config.DMAConf.CmChannel, &event);
        if (ret && errno != EAGAIN) {
            ret = errno;
            fprintf(stderr, "rdma_get_cm_event error %d\n", ret);
            SignalHandler(SIGTERM);
        }
        else if (!ret) {
#ifdef DDS_STORAGE_FILE_BACKEND_VERBOSE
            fprintf(stdout, "cma_event type %s cma_id %p (%s)\n",
                rdma_event_str(event->event), event->id,
                (event->id == config.DMAConf.CmId) ? "parent" : "child");
#endif

            ret = ProcessCmEvents(&config, event);
            if (ret) {
                fprintf(stderr, "ProcessCmEvents error %d\n", ret);
                SignalHandler(SIGTERM);
            }
        }

        //
        // Process RDMA events for control connections
        //
        //
        ret = ProcessCtrlCqEvents(&config);
        if (ret) {
            fprintf(stderr, "ProcessCtrlCqEvents error %d\n", ret);
            SignalHandler(SIGTERM);
        }

        //
        // Process RDMA events for buffer connections
        //
        //
        ret = ProcessBuffCqEvents(&config);
        if (ret) {
            fprintf(stderr, "ProcessBuffCqEvents error %d\n", ret);
            SignalHandler(SIGTERM);
        }

        //
        // Check and process I/O completions
        //
        //
        ret = CheckAndProcessIOCompletions(&config);
        if (ret) {
            fprintf(stderr, "CheckAndProcessIOCompletions error %d\n", ret);
            SignalHandler(SIGTERM);
        }
    }

    //
    // Clean up
    //
    //
    DeallocConns(&config);
    TermDMA(&config.DMAConf);

    spdk_app_stop(ret);
    printf("spdk_app_stop returned with: %d\n", ret);

    return;
}

//
// Usage function for printing parameters that are specific to this application, needed for SPDK app start
//
//
static void
dds_custom_args_usage(void)
{
	printf("if there is any custom cmdline params, add a description for it here\n");
}

//
// This function is called to parse the parameters that are specific to this application
//
//
static int
dds_parse_arg(int ch, char *arg)
{
	printf("parse custom arg func called, currently doing nothing...\n");
}

int main(int argc, char **argv) {
    int rc;
    struct spdk_app_opts opts = {};
    /* Set default values in opts structure. */
	spdk_app_opts_init(&opts, sizeof(opts));
	opts.name = "hello_bdev";
    /*
	 * Parse built-in SPDK command line parameters as well
	 * as our custom one(s).
	 */
	if ((rc = spdk_app_parse_args(argc, argv, &opts, "b:", NULL, dds_parse_arg,
				      dds_custom_args_usage)) != SPDK_APP_PARSE_ARGS_SUCCESS) {
        printf("spdk_app_parse_args() failed with: %d\n", rc);
		exit(rc);
	}
    /* int ret = RunFileBackEnd(DDS_BACKEND_ADDR, DDS_BACKEND_PORT, 32, 32); */
    
    struct runFileBackEndArgs args = {
        .ServerIpStr = DDS_BACKEND_ADDR,
        .ServerPort = DDS_BACKEND_PORT,
        .MaxClients = 32,
        .MaxBuffs = 32
    };
    
    rc = spdk_app_start(&opts, RunFileBackEnd, &args);  // block until `spdk_app_stop` is called
    printf("spdk_app_start returned with: %d\n", rc);
    spdk_app_fini();
}
