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

#include "DDSTypes.h"
#include "BackEnd.h"

#define TRUE 1
#define FALSE 0

static volatile int ForceQuitFileBackEnd = 0;

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
// Set up regions and buffers for a buffer connection
//
//
static int
SetUpBuffRegionsAndBuffers(
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
        goto SetUpBuffRegionsAndBuffersReturn;
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
        goto DeregisterBuffRecvMrReturn;
    }

#if RING_BUFFER_IMPL == RING_BUFFER_IMPL_PROGRESSIVE
    //
    // Read data buffer and region
    //
    //
    BuffConn->DMAReadDataBuff = malloc(BACKEND_MAX_DMA_SIZE);
    if (!BuffConn->DMAReadDataBuff) {
        fprintf(stderr, "%s [error]: OOM for DMA read data buffer\n", __func__);
        ret = -1;
        goto DeregisterBuffSendMrReturn;
    }
    BuffConn->DMAReadDataMr = ibv_reg_mr(
        BuffConn->PDomain,
        BuffConn->DMAReadDataBuff,
        BACKEND_MAX_DMA_SIZE,
        IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ
    );
    if (!BuffConn->DMAReadDataMr) {
        fprintf(stderr, "%s [error]: ibv_reg_mr for DMA read data failed\n", __func__);
        ret = -1;
        goto FreeBuffDMAReadDataBuffReturn;
    }
#elif RING_BUFFER_IMPL == RING_BUFFER_IMPL_PROGRESSIVE_NOTALIGNED
    //
    // Read data buffer and region
    //
    //
    BuffConn->DMAReadDataBuff = malloc(BACKEND_MAX_DMA_SIZE);
    if (!BuffConn->DMAReadDataBuff) {
        fprintf(stderr, "%s [error]: OOM for DMA read data buffer\n", __func__);
        ret = -1;
        goto DeregisterBuffSendMrReturn;
    }
    BuffConn->DMAReadDataMr = ibv_reg_mr(
        BuffConn->PDomain,
        BuffConn->DMAReadDataBuff,
        BACKEND_MAX_DMA_SIZE,
        IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ
    );
    if (!BuffConn->DMAReadDataMr) {
        fprintf(stderr, "%s [error]: ibv_reg_mr for DMA read data failed\n", __func__);
        ret = -1;
        goto FreeBuffDMAReadDataBuffReturn;
    }
#elif RING_BUFFER_IMPL == RING_BUFFER_IMPL_LOCKBASED
    //
    // Read data buffer and region
    //
    //
    BuffConn->DMAReadDataBuff = malloc(BACKEND_MAX_DMA_SIZE);
    if (!BuffConn->DMAReadDataBuff) {
        fprintf(stderr, "%s [error]: OOM for DMA read data buffer\n", __func__);
        ret = -1;
        goto DeregisterBuffSendMrReturn;
    }
    BuffConn->DMAReadDataMr = ibv_reg_mr(
        BuffConn->PDomain,
        BuffConn->DMAReadDataBuff,
        BACKEND_MAX_DMA_SIZE,
        IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ
    );
    if (!BuffConn->DMAReadDataMr) {
        fprintf(stderr, "%s [error]: ibv_reg_mr for DMA read data failed\n", __func__);
        ret = -1;
        goto FreeBuffDMAReadDataBuffReturn;
    }
#elif RING_BUFFER_IMPL == RING_BUFFER_IMPL_FARMSTYLE
    //
    // Read data buffer and region
    //
    //
    BuffConn->DMADataBuff = malloc(BACKEND_MAX_DMA_SIZE);
    if (!BuffConn->DMADataBuff) {
        fprintf(stderr, "%s [error]: OOM for DMA data buffer\n", __func__);
        ret = -1;
        goto DeregisterBuffSendMrReturn;
    }
    BuffConn->DMADataMr = ibv_reg_mr(
        BuffConn->PDomain,
        BuffConn->DMADataBuff,
        BACKEND_MAX_DMA_SIZE,
        IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ
    );
    if (!BuffConn->DMADataMr) {
        fprintf(stderr, "%s [error]: ibv_reg_mr for DMA data failed\n", __func__);
        ret = -1;
        goto FreeBuffDMADataBuffReturn;
    }
#else
#error "Unknown ring buffer implementation"
#endif

    //
    // Read meta buffer and region
    //
    //
    BuffConn->DMAReadMetaMr = ibv_reg_mr(
        BuffConn->PDomain,
        BuffConn->DMAReadMetaBuff,
        RING_BUFFER_META_DATA_SIZE,
        IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ
    );
    if (!BuffConn->DMAReadMetaMr) {
        fprintf(stderr, "%s [error]: ibv_reg_mr for DMA read meta failed\n", __func__);
        ret = -1;
#if RING_BUFFER_IMPL == RING_BUFFER_IMPL_PROGRESSIVE
        goto DeregisterBuffReadDataMrReturn;
#elif RING_BUFFER_IMPL == RING_BUFFER_IMPL_PROGRESSIVE_NOTALIGNED
        goto DeregisterBuffReadDataMrReturn;
#elif RING_BUFFER_IMPL == RING_BUFFER_IMPL_LOCKBASED
        goto DeregisterBuffReadDataMrReturn;
#elif RING_BUFFER_IMPL == RING_BUFFER_IMPL_FARMSTYLE
        goto DeregisterBuffDataMrReturn;
#else
#error "Unknown ring buffer implementation"
#endif
    }

    //
    // Write meta buffer and region
    //
    //
    BuffConn->DMAWriteMetaBuff = (char*)&BuffConn->RequestRing.Head;
    BuffConn->DMAWriteMetaMr = ibv_reg_mr(
        BuffConn->PDomain,
        BuffConn->DMAWriteMetaBuff,
        sizeof(int),
        IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ
    );
    if (!BuffConn->DMAWriteMetaMr) {
        fprintf(stderr, "%s [error]: ibv_reg_mr for DMA write meta failed\n", __func__);
        ret = -1;
        goto DeregisterBuffReadMetaMrReturn;
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

#if RING_BUFFER_IMPL == RING_BUFFER_IMPL_PROGRESSIVE
    BuffConn->DMAReadDataSgl.addr = (uint64_t)BuffConn->DMAReadDataBuff;
    BuffConn->DMAReadDataSgl.length = BACKEND_MAX_DMA_SIZE;
    BuffConn->DMAReadDataSgl.lkey = BuffConn->DMAReadDataMr->lkey;
    BuffConn->DMAReadDataWr.opcode = IBV_WR_RDMA_READ;
    BuffConn->DMAReadDataWr.send_flags = IBV_SEND_SIGNALED;
    BuffConn->DMAReadDataWr.sg_list = &BuffConn->DMAReadDataSgl;
    BuffConn->DMAReadDataWr.num_sge = 1;
    BuffConn->DMAReadDataWr.wr_id = BUFF_READ_DATA_WR_ID;

    BuffConn->DMAReadDataSplitSgl.addr = (uint64_t)BuffConn->DMAReadDataBuff;
    BuffConn->DMAReadDataSplitSgl.length = BACKEND_MAX_DMA_SIZE;
    BuffConn->DMAReadDataSplitSgl.lkey = BuffConn->DMAReadDataMr->lkey;
    BuffConn->DMAReadDataSplitWr.opcode = IBV_WR_RDMA_READ;
    BuffConn->DMAReadDataSplitWr.send_flags = IBV_SEND_SIGNALED;
    BuffConn->DMAReadDataSplitWr.sg_list = &BuffConn->DMAReadDataSplitSgl;
    BuffConn->DMAReadDataSplitWr.num_sge = 1;
    BuffConn->DMAReadDataSplitWr.wr_id = BUFF_READ_DATA_SPLIT_WR_ID;
#elif RING_BUFFER_IMPL == RING_BUFFER_IMPL_PROGRESSIVE_NOTALIGNED
    BuffConn->DMAReadDataSgl.addr = (uint64_t)BuffConn->DMAReadDataBuff;
    BuffConn->DMAReadDataSgl.length = BACKEND_MAX_DMA_SIZE;
    BuffConn->DMAReadDataSgl.lkey = BuffConn->DMAReadDataMr->lkey;
    BuffConn->DMAReadDataWr.opcode = IBV_WR_RDMA_READ;
    BuffConn->DMAReadDataWr.send_flags = IBV_SEND_SIGNALED;
    BuffConn->DMAReadDataWr.sg_list = &BuffConn->DMAReadDataSgl;
    BuffConn->DMAReadDataWr.num_sge = 1;
    BuffConn->DMAReadDataWr.wr_id = BUFF_READ_DATA_WR_ID;

    BuffConn->DMAReadDataSplitSgl.addr = (uint64_t)BuffConn->DMAReadDataBuff;
    BuffConn->DMAReadDataSplitSgl.length = BACKEND_MAX_DMA_SIZE;
    BuffConn->DMAReadDataSplitSgl.lkey = BuffConn->DMAReadDataMr->lkey;
    BuffConn->DMAReadDataSplitWr.opcode = IBV_WR_RDMA_READ;
    BuffConn->DMAReadDataSplitWr.send_flags = IBV_SEND_SIGNALED;
    BuffConn->DMAReadDataSplitWr.sg_list = &BuffConn->DMAReadDataSplitSgl;
    BuffConn->DMAReadDataSplitWr.num_sge = 1;
    BuffConn->DMAReadDataSplitWr.wr_id = BUFF_READ_DATA_SPLIT_WR_ID;
#elif RING_BUFFER_IMPL == RING_BUFFER_IMPL_LOCKBASED
    BuffConn->DMAReadDataSgl.addr = (uint64_t)BuffConn->DMAReadDataBuff;
    BuffConn->DMAReadDataSgl.length = BACKEND_MAX_DMA_SIZE;
    BuffConn->DMAReadDataSgl.lkey = BuffConn->DMAReadDataMr->lkey;
    BuffConn->DMAReadDataWr.opcode = IBV_WR_RDMA_READ;
    BuffConn->DMAReadDataWr.send_flags = IBV_SEND_SIGNALED;
    BuffConn->DMAReadDataWr.sg_list = &BuffConn->DMAReadDataSgl;
    BuffConn->DMAReadDataWr.num_sge = 1;
    BuffConn->DMAReadDataWr.wr_id = BUFF_READ_DATA_WR_ID;

    BuffConn->DMAReadDataSplitSgl.addr = (uint64_t)BuffConn->DMAReadDataBuff;
    BuffConn->DMAReadDataSplitSgl.length = BACKEND_MAX_DMA_SIZE;
    BuffConn->DMAReadDataSplitSgl.lkey = BuffConn->DMAReadDataMr->lkey;
    BuffConn->DMAReadDataSplitWr.opcode = IBV_WR_RDMA_READ;
    BuffConn->DMAReadDataSplitWr.send_flags = IBV_SEND_SIGNALED;
    BuffConn->DMAReadDataSplitWr.sg_list = &BuffConn->DMAReadDataSplitSgl;
    BuffConn->DMAReadDataSplitWr.num_sge = 1;
    BuffConn->DMAReadDataSplitWr.wr_id = BUFF_READ_DATA_SPLIT_WR_ID;
#elif RING_BUFFER_IMPL == RING_BUFFER_IMPL_FARMSTYLE
    BuffConn->DMADataSgl.addr = (uint64_t)BuffConn->DMADataBuff;
    BuffConn->DMADataSgl.length = BACKEND_MAX_DMA_SIZE;
    BuffConn->DMADataSgl.lkey = BuffConn->DMADataMr->lkey;
    BuffConn->DMADataWr.send_flags = IBV_SEND_SIGNALED;
    BuffConn->DMADataWr.sg_list = &BuffConn->DMADataSgl;
    BuffConn->DMADataWr.num_sge = 1;

    BuffConn->DMADataSplitSgl.addr = (uint64_t)BuffConn->DMADataBuff;
    BuffConn->DMADataSplitSgl.length = BACKEND_MAX_DMA_SIZE;
    BuffConn->DMADataSplitSgl.lkey = BuffConn->DMADataMr->lkey;
    BuffConn->DMADataSplitWr.send_flags = IBV_SEND_SIGNALED;
    BuffConn->DMADataSplitWr.sg_list = &BuffConn->DMADataSplitSgl;
    BuffConn->DMADataSplitWr.num_sge = 1;
#else
#error "Unknown ring buffer implementation"
#endif

    BuffConn->DMAReadMetaSgl.addr = (uint64_t)BuffConn->DMAReadMetaBuff;
    BuffConn->DMAReadMetaSgl.length = RING_BUFFER_META_DATA_SIZE;
    BuffConn->DMAReadMetaSgl.lkey = BuffConn->DMAReadMetaMr->lkey;
    BuffConn->DMAReadMetaWr.opcode = IBV_WR_RDMA_READ;
    BuffConn->DMAReadMetaWr.send_flags = IBV_SEND_SIGNALED;
    BuffConn->DMAReadMetaWr.sg_list = &BuffConn->DMAReadMetaSgl;
    BuffConn->DMAReadMetaWr.num_sge = 1;
    BuffConn->DMAReadMetaWr.wr_id = BUFF_READ_META_WR_ID;

    BuffConn->DMAWriteMetaSgl.addr = (uint64_t)BuffConn->DMAWriteMetaBuff;
    BuffConn->DMAWriteMetaSgl.length = sizeof(int);
    BuffConn->DMAWriteMetaSgl.lkey = BuffConn->DMAWriteMetaMr->lkey;
    BuffConn->DMAWriteMetaWr.opcode = IBV_WR_RDMA_WRITE;
    BuffConn->DMAWriteMetaWr.send_flags = IBV_SEND_SIGNALED;
    BuffConn->DMAWriteMetaWr.sg_list = &BuffConn->DMAWriteMetaSgl;
    BuffConn->DMAWriteMetaWr.num_sge = 1;
    BuffConn->DMAWriteMetaWr.wr_id = BUFF_WRITE_META_WR_ID;

    return 0;

DeregisterBuffReadMetaMrReturn:
    ibv_dereg_mr(BuffConn->DMAReadMetaMr);

#if RING_BUFFER_IMPL == RING_BUFFER_IMPL_PROGRESSIVE
DeregisterBuffReadDataMrReturn:
    ibv_dereg_mr(BuffConn->DMAReadDataMr);

FreeBuffDMAReadDataBuffReturn:
    free(BuffConn->DMAReadDataBuff);
#elif RING_BUFFER_IMPL == RING_BUFFER_IMPL_PROGRESSIVE_NOTALIGNED
DeregisterBuffReadDataMrReturn:
    ibv_dereg_mr(BuffConn->DMAReadDataMr);

FreeBuffDMAReadDataBuffReturn:
    free(BuffConn->DMAReadDataBuff);
#elif RING_BUFFER_IMPL == RING_BUFFER_IMPL_LOCKBASED
DeregisterBuffReadDataMrReturn:
    ibv_dereg_mr(BuffConn->DMAReadDataMr);

FreeBuffDMAReadDataBuffReturn:
    free(BuffConn->DMAReadDataBuff);
#elif RING_BUFFER_IMPL == RING_BUFFER_IMPL_FARMSTYLE
DeregisterBuffDataMrReturn:
    ibv_dereg_mr(BuffConn->DMADataMr);

FreeBuffDMADataBuffReturn:
    free(BuffConn->DMADataBuff);
#else
#error "Unknown ring buffer implementation"
#endif

DeregisterBuffSendMrReturn:
    ibv_dereg_mr(BuffConn->SendMr);

DeregisterBuffRecvMrReturn:
    ibv_dereg_mr(BuffConn->RecvMr);

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
    ibv_dereg_mr(BuffConn->DMAWriteMetaMr);
    ibv_dereg_mr(BuffConn->DMAReadMetaMr);
#if RING_BUFFER_IMPL == RING_BUFFER_IMPL_PROGRESSIVE
    free(BuffConn->DMAReadDataBuff);
    ibv_dereg_mr(BuffConn->DMAReadDataMr);
#elif RING_BUFFER_IMPL == RING_BUFFER_IMPL_PROGRESSIVE_NOTALIGNED
    free(BuffConn->DMAReadDataBuff);
    ibv_dereg_mr(BuffConn->DMAReadDataMr);
#elif RING_BUFFER_IMPL == RING_BUFFER_IMPL_LOCKBASED
    free(BuffConn->DMAReadDataBuff);
    ibv_dereg_mr(BuffConn->DMAReadDataMr);
#elif RING_BUFFER_IMPL == RING_BUFFER_IMPL_FARMSTYLE
    free(BuffConn->DMADataBuff);
    ibv_dereg_mr(BuffConn->DMADataMr);
#else
#error "Unknown ring buffer implementation"
#endif
    ibv_dereg_mr(BuffConn->SendMr);
    ibv_dereg_mr(BuffConn->RecvMr);

    memset(&BuffConn->DMAWriteMetaSgl, 0, sizeof(BuffConn->DMAReadMetaSgl));
    memset(&BuffConn->DMAReadMetaSgl, 0, sizeof(BuffConn->DMAReadMetaSgl));
    memset(&BuffConn->DMAWriteMetaWr, 0, sizeof(BuffConn->DMAWriteMetaWr));
    memset(&BuffConn->DMAReadMetaWr, 0, sizeof(BuffConn->DMAReadMetaWr));
#if RING_BUFFER_IMPL == RING_BUFFER_IMPL_PROGRESSIVE
    memset(&BuffConn->DMAReadDataSgl, 0, sizeof(BuffConn->DMAReadDataSgl));
    memset(&BuffConn->DMAReadDataSplitSgl, 0, sizeof(BuffConn->DMAReadDataSplitSgl));
    memset(&BuffConn->DMAReadDataWr, 0, sizeof(BuffConn->DMAReadDataWr));
    memset(&BuffConn->DMAReadDataSplitWr, 0, sizeof(BuffConn->DMAReadDataSplitWr));
#elif RING_BUFFER_IMPL == RING_BUFFER_IMPL_PROGRESSIVE_NOTALIGNED
    memset(&BuffConn->DMAReadDataSgl, 0, sizeof(BuffConn->DMAReadDataSgl));
    memset(&BuffConn->DMAReadDataSplitSgl, 0, sizeof(BuffConn->DMAReadDataSplitSgl));
    memset(&BuffConn->DMAReadDataWr, 0, sizeof(BuffConn->DMAReadDataWr));
    memset(&BuffConn->DMAReadDataSplitWr, 0, sizeof(BuffConn->DMAReadDataSplitWr));
#elif RING_BUFFER_IMPL == RING_BUFFER_IMPL_LOCKBASED
    memset(&BuffConn->DMAReadDataSgl, 0, sizeof(BuffConn->DMAReadDataSgl));
    memset(&BuffConn->DMAReadDataSplitSgl, 0, sizeof(BuffConn->DMAReadDataSplitSgl));
    memset(&BuffConn->DMAReadDataWr, 0, sizeof(BuffConn->DMAReadDataWr));
    memset(&BuffConn->DMAReadDataSplitWr, 0, sizeof(BuffConn->DMAReadDataSplitWr));
#elif RING_BUFFER_IMPL == RING_BUFFER_IMPL_FARMSTYLE
    memset(&BuffConn->DMADataSgl, 0, sizeof(BuffConn->DMADataSgl));
    memset(&BuffConn->DMADataSplitSgl, 0, sizeof(BuffConn->DMADataSplitSgl));
    memset(&BuffConn->DMADataWr, 0, sizeof(BuffConn->DMADataWr));
    memset(&BuffConn->DMADataSplitWr, 0, sizeof(BuffConn->DMADataSplitWr));
#else
#error "Unknown ring buffer implementation"
#endif
    memset(&BuffConn->SendSgl, 0, sizeof(BuffConn->SendSgl));
    memset(&BuffConn->RecvSgl, 0, sizeof(BuffConn->RecvSgl));
    memset(&BuffConn->SendWr, 0, sizeof(BuffConn->SendWr));
    memset(&BuffConn->RecvWr, 0, sizeof(BuffConn->RecvWr));
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
static int inline
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
                        ctrlConn->InUse = 1;
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
                        buffConn->InUse = 1;
                        buffConn->Prefetching = Config->Prefetching;
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
                            ctrlConn->InUse = 0;
                        }
#ifdef DDS_STORAGE_FILE_BACKEND_VERBOSE
                        fprintf(stderr, "CM: RDMA_CM_EVENT_DISCONNECTED for Control Conn#%d\n", connId);
#endif
                }
                else {
                        if (Config->BuffConns[connId].InUse) {
                            struct BuffConnConfig *buffConn = &Config->BuffConns[connId];
                            DestroyBuffRegionsAndBuffers(buffConn);
                            DestroyBuffQPair(buffConn);
                            buffConn->InUse = 0;
                        }
#ifdef DDS_STORAGE_FILE_BACKEND_VERBOSE
                    fprintf(stderr, "CM: RDMA_CM_EVENT_DISCONNECTED for Buffer Conn#%d\n", connId);
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
        case CTRL_MSG_F2B_REQUEST_ID: {
            CtrlMsgB2FRespondId *resp = (CtrlMsgB2FRespondId *)(msgOut + 1);
            struct ibv_send_wr *badSendWr = NULL;
            struct ibv_recv_wr *badRecvWr = NULL;

            //
            // Post a receiv first
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
        case CTRL_MSG_F2B_TERMINATE: {
            CtrlMsgF2BTerminate *req = (CtrlMsgF2BTerminate *)(msgIn + 1);

            if (req->ClientId == CtrlConn->CtrlId) {
                DestroyCtrlRegionsAndBuffers(CtrlConn);
                DestroyCtrlQPair(CtrlConn);
                CtrlConn->InUse = 0;
#ifdef DDS_STORAGE_FILE_BACKEND_VERBOSE
                fprintf(stdout, "%s [info]: Control Conn#%d is disconnected\n", __func__, req->ClientId);
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
            // Post a receiv first
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
#if RING_BUFFER_IMPL == RING_BUFFER_IMPL_PROGRESSIVE
            InitializeRequestRingBufferBackEnd(
                &BuffConn->RequestRing,
                req->BufferAddress,
                req->AccessToken,
                req->Capacity
            );
#elif  RING_BUFFER_IMPL == RING_BUFFER_IMPL_PROGRESSIVE_NOTALIGNED
            InitializeRequestRingBufferNotAlignedBackEnd(
                &BuffConn->RequestRing,
                req->BufferAddress,
                req->AccessToken,
                req->Capacity
            );
#elif  RING_BUFFER_IMPL == RING_BUFFER_IMPL_LOCKBASED
            InitializeRequestRingBufferLockBasedBackEnd(
                &BuffConn->RequestRing,
                req->BufferAddress,
                req->AccessToken,
                req->Capacity
            );
#elif RING_BUFFER_IMPL == RING_BUFFER_IMPL_FARMSTYLE
            InitializeRequestRingBufferFaRMStyleBackEnd(
                &BuffConn->RequestRing,
                req->BufferAddress,
                req->AccessToken,
                req->Capacity
            );
#else
#error "Unknown ring buffer implementation"
#endif

            BuffConn->DMAReadMetaWr.wr.rdma.remote_addr = BuffConn->RequestRing.ReadMetaAddr;
            BuffConn->DMAReadMetaWr.wr.rdma.rkey = BuffConn->RequestRing.AccessToken;
            BuffConn->DMAWriteMetaWr.wr.rdma.remote_addr = BuffConn->RequestRing.WriteMetaAddr;
            BuffConn->DMAWriteMetaWr.wr.rdma.rkey = BuffConn->RequestRing.AccessToken;
#if RING_BUFFER_IMPL == RING_BUFFER_IMPL_PROGRESSIVE
            BuffConn->DMAReadDataWr.wr.rdma.rkey = BuffConn->RequestRing.AccessToken;
            BuffConn->DMAReadDataSplitWr.wr.rdma.rkey = BuffConn->RequestRing.AccessToken;
#elif RING_BUFFER_IMPL == RING_BUFFER_IMPL_PROGRESSIVE_NOTALIGNED
            BuffConn->DMAReadDataWr.wr.rdma.rkey = BuffConn->RequestRing.AccessToken;
            BuffConn->DMAReadDataSplitWr.wr.rdma.rkey = BuffConn->RequestRing.AccessToken;
#elif RING_BUFFER_IMPL == RING_BUFFER_IMPL_LOCKBASED
            BuffConn->DMAReadDataWr.wr.rdma.rkey = BuffConn->RequestRing.AccessToken;
            BuffConn->DMAReadDataSplitWr.wr.rdma.rkey = BuffConn->RequestRing.AccessToken;
#elif RING_BUFFER_IMPL == RING_BUFFER_IMPL_FARMSTYLE
            BuffConn->DMADataWr.wr.rdma.rkey = BuffConn->RequestRing.AccessToken;
            BuffConn->DMADataSplitWr.wr.rdma.rkey = BuffConn->RequestRing.AccessToken;
#else
#error "Unknown ring buffer implementation"
#endif

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
#endif
            //
            // Start polling
            //
            //
#if RING_BUFFER_IMPL == RING_BUFFER_IMPL_PROGRESSIVE
            ret = ibv_post_send(BuffConn->QPair, &BuffConn->DMAReadMetaWr, &badSendWr);
            if (ret) {
                fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                ret = -1;
            }
#elif RING_BUFFER_IMPL == RING_BUFFER_IMPL_PROGRESSIVE_NOTALIGNED
            ret = ibv_post_send(BuffConn->QPair, &BuffConn->DMAReadMetaWr, &badSendWr);
            if (ret) {
                fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                ret = -1;
            }
#elif RING_BUFFER_IMPL == RING_BUFFER_IMPL_LOCKBASED
            ret = ibv_post_send(BuffConn->QPair, &BuffConn->DMAReadMetaWr, &badSendWr);
            if (ret) {
                fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                ret = -1;
            }
#elif RING_BUFFER_IMPL == RING_BUFFER_IMPL_FARMSTYLE
            BuffConn->DMAReadMetaWr.wr.rdma.remote_addr = BuffConn->RequestRing.DataBaseAddr + BuffConn->RequestRing.Head;
            ret = ibv_post_send(BuffConn->QPair, &BuffConn->DMAReadMetaWr, &badSendWr);
            if (ret) {
                fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                ret = -1;
            }
#else
#error "Unknown ring buffer implementation"
#endif
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
// Process communication channel events for buffer connections
//
//
static int inline
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
                fprintf(stderr, "%s [error]: ibv_poll_cq failed status %d (%s)\n", __func__, wc.status, ibv_wc_status_str(wc.status));
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
                    case BUFF_READ_META_WR_ID: {
#if RING_BUFFER_IMPL == RING_BUFFER_IMPL_PROGRESSIVE
                        //
                        // Process a meta read
                        //
                        //
                        int* pointers = (int*)buffConn->DMAReadMetaBuff;
                        int progress = pointers[0];
                        int tail = pointers[DDS_CACHE_LINE_SIZE_BY_INT];
                        if (tail == buffConn->RequestRing.Head || tail != progress) {
                            //
                            // Not ready to read, poll again
                            //
                            //
                            ret = ibv_post_send(buffConn->QPair, &buffConn->DMAReadMetaWr, &badSendWr);
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
                                buffConn->DMAReadDataSize = availBytes;
                                sourceBuffer1 = buffConn->RequestRing.DataBaseAddr + head;
                            }
                            else {
                                availBytes = DDS_REQUEST_RING_BYTES - head;
                                buffConn->DMAReadDataSize = availBytes + progress;
                                sourceBuffer1 = buffConn->RequestRing.DataBaseAddr + head;
                                sourceBuffer2 = buffConn->RequestRing.DataBaseAddr;
                            }

                            //
                            // Post a DMA read
                            //
                            //
                            buffConn->DMAReadDataWr.wr.rdma.remote_addr = sourceBuffer1;
                            buffConn->DMAReadDataWr.sg_list->length = availBytes;

                            if (sourceBuffer2) {
                                buffConn->DMAReadDataSplitState = BUFF_READ_DATA_SPLIT_STATE_SPLIT;

                                ret = ibv_post_send(buffConn->QPair, &buffConn->DMAReadDataWr, &badSendWr);
                                if (ret) {
                                    fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                                    ret = -1;
                                }

                                buffConn->DMAReadDataSplitWr.sg_list->addr = (uint64_t)(buffConn->DMAReadDataBuff + availBytes);
                                buffConn->DMAReadDataSplitWr.sg_list->length = progress;
                                buffConn->DMAReadDataSplitWr.wr.rdma.remote_addr = sourceBuffer2;

                                ret = ibv_post_send(buffConn->QPair, &buffConn->DMAReadDataSplitWr, &badSendWr);
                                if (ret) {
                                    fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                                    ret = -1;
                                }
                            }
                            else {
                                buffConn->DMAReadDataSplitState = BUFF_READ_DATA_SPLIT_STATE_NOT_SPLIT;

                                ret = ibv_post_send(buffConn->QPair, &buffConn->DMAReadDataWr, &badSendWr);
                                if (ret) {
                                    fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                                    ret = -1;
                                }
                            }

                            buffConn->RequestRing.Head = progress;
                            // TODO: immediately update remote head
                            ret = ibv_post_send(buffConn->QPair, &buffConn->DMAWriteMetaWr, &badSendWr);
                            if (ret) {
                                fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                                ret = -1;
                            }
                        }
#elif RING_BUFFER_IMPL == RING_BUFFER_IMPL_PROGRESSIVE_NOTALIGNED
                        //
                        // Process a meta read
                        //
                        //
                        int* pointers = (int*)buffConn->DMAReadMetaBuff;
                        int progress = pointers[0];
                        int tail = pointers[1];
                        if (tail == buffConn->RequestRing.Head || tail != progress) {
                            //
                            // Not ready to read, poll again
                            //
                            //
                            ret = ibv_post_send(buffConn->QPair, &buffConn->DMAReadMetaWr, &badSendWr);
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
                                buffConn->DMAReadDataSize = availBytes;
                                sourceBuffer1 = buffConn->RequestRing.DataBaseAddr + head;
                            }
                            else {
                                availBytes = DDS_REQUEST_RING_BYTES - head;
                                buffConn->DMAReadDataSize = availBytes + progress;
                                sourceBuffer1 = buffConn->RequestRing.DataBaseAddr + head;
                                sourceBuffer2 = buffConn->RequestRing.DataBaseAddr;
                            }

                            //
                            // Post a DMA read
                            //
                            //
                            buffConn->DMAReadDataWr.wr.rdma.remote_addr = sourceBuffer1;
                            buffConn->DMAReadDataWr.sg_list->length = availBytes;

                            if (sourceBuffer2) {
                                buffConn->DMAReadDataSplitState = BUFF_READ_DATA_SPLIT_STATE_SPLIT;

                                ret = ibv_post_send(buffConn->QPair, &buffConn->DMAReadDataWr, &badSendWr);
                                if (ret) {
                                    fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                                    ret = -1;
                                }

                                buffConn->DMAReadDataSplitWr.sg_list->addr = (uint64_t)(buffConn->DMAReadDataBuff + availBytes);
                                buffConn->DMAReadDataSplitWr.sg_list->length = progress;
                                buffConn->DMAReadDataSplitWr.wr.rdma.remote_addr = sourceBuffer2;

                                ret = ibv_post_send(buffConn->QPair, &buffConn->DMAReadDataSplitWr, &badSendWr);
                                if (ret) {
                                    fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                                    ret = -1;
                                }
                            }
                            else {
                                buffConn->DMAReadDataSplitState = BUFF_READ_DATA_SPLIT_STATE_NOT_SPLIT;

                                ret = ibv_post_send(buffConn->QPair, &buffConn->DMAReadDataWr, &badSendWr);
                                if (ret) {
                                    fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                                    ret = -1;
                                }
                            }

                            buffConn->RequestRing.Head = progress;
                            // TODO: immediately update remote head
                        }
#elif RING_BUFFER_IMPL == RING_BUFFER_IMPL_LOCKBASED
                        //
                        // Process a meta read
                        //
                        //
                        int* pointers = (int*)buffConn->DMAReadMetaBuff;
                        int tail = pointers[0];
                        if (tail == buffConn->RequestRing.Head) {
                            //
                            // Not ready to read, poll again
                            //
                            //
                            ret = ibv_post_send(buffConn->QPair, &buffConn->DMAReadMetaWr, &badSendWr);
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

                            if (tail > head) {
                                availBytes = tail - head;
                                buffConn->DMAReadDataSize = availBytes;
                                sourceBuffer1 = buffConn->RequestRing.DataBaseAddr + head;
                            }
                            else {
                                availBytes = DDS_REQUEST_RING_BYTES - head;
                                buffConn->DMAReadDataSize = availBytes + tail;
                                sourceBuffer1 = buffConn->RequestRing.DataBaseAddr + head;
                                sourceBuffer2 = buffConn->RequestRing.DataBaseAddr;
                            }

                            //
                            // Post a DMA read
                            //
                            //
                            buffConn->DMAReadDataWr.wr.rdma.remote_addr = sourceBuffer1;
                            buffConn->DMAReadDataWr.sg_list->length = availBytes;

                            if (sourceBuffer2) {
                                buffConn->DMAReadDataSplitState = BUFF_READ_DATA_SPLIT_STATE_SPLIT;

                                ret = ibv_post_send(buffConn->QPair, &buffConn->DMAReadDataWr, &badSendWr);
                                if (ret) {
                                    fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                                    ret = -1;
                                }

                                buffConn->DMAReadDataSplitWr.sg_list->addr = (uint64_t)(buffConn->DMAReadDataBuff + availBytes);
                                buffConn->DMAReadDataSplitWr.sg_list->length = tail;
                                buffConn->DMAReadDataSplitWr.wr.rdma.remote_addr = sourceBuffer2;

                                ret = ibv_post_send(buffConn->QPair, &buffConn->DMAReadDataSplitWr, &badSendWr);
                                if (ret) {
                                    fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                                    ret = -1;
                                }
                            }
                            else {
                                buffConn->DMAReadDataSplitState = BUFF_READ_DATA_SPLIT_STATE_NOT_SPLIT;

                                ret = ibv_post_send(buffConn->QPair, &buffConn->DMAReadDataWr, &badSendWr);
                                if (ret) {
                                    fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                                    ret = -1;
                                }
                            }

                            buffConn->RequestRing.Head = tail;
                            // TODO: immediately update remote head
                        }
#elif RING_BUFFER_IMPL == RING_BUFFER_IMPL_FARMSTYLE
                        //
                        // Process a meta read
                        //
                        //
                        int* pointers = (int*)buffConn->DMAReadMetaBuff;
                        int reqSize = pointers[0];
                        if (reqSize == 0) {
                            //
                            // Not ready to read, poll again
                            //
                            //
                            ret = ibv_post_send(buffConn->QPair, &buffConn->DMAReadMetaWr, &badSendWr);
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
                            int tail = (head + reqSize) % DDS_REQUEST_RING_BYTES;

                            if (tail > head) {
                                availBytes = tail - head;
                                buffConn->DMADataSize = availBytes;
                                sourceBuffer1 = buffConn->RequestRing.DataBaseAddr + head;
                            }
                            else {
                                availBytes = DDS_REQUEST_RING_BYTES - head;
                                buffConn->DMADataSize = availBytes + tail;
                                sourceBuffer1 = buffConn->RequestRing.DataBaseAddr + head;
                                sourceBuffer2 = buffConn->RequestRing.DataBaseAddr;
                            }

                            //
                            // Post a DMA read
                            //
                            //
                            buffConn->DMADataWr.wr.rdma.remote_addr = sourceBuffer1;
                            buffConn->DMADataWr.sg_list->length = availBytes;
                            buffConn->DMADataWr.opcode = IBV_WR_RDMA_READ;
                            buffConn->DMADataWr.wr_id = BUFF_READ_DATA_WR_ID;
                            buffConn->DMADataSplitWr.wr.rdma.remote_addr = sourceBuffer2;

                            if (sourceBuffer2) {
                                buffConn->DMADataSplitState = BUFF_READ_DATA_SPLIT_STATE_SPLIT;

                                ret = ibv_post_send(buffConn->QPair, &buffConn->DMADataWr, &badSendWr);
                                if (ret) {
                                    fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                                    ret = -1;
                                }

                                buffConn->DMADataSplitWr.sg_list->addr = (uint64_t)(buffConn->DMADataBuff + availBytes);
                                buffConn->DMADataSplitWr.sg_list->length = tail;
                                buffConn->DMADataSplitWr.opcode = IBV_WR_RDMA_READ;
                                buffConn->DMADataSplitWr.wr_id = BUFF_READ_DATA_SPLIT_WR_ID;

                                ret = ibv_post_send(buffConn->QPair, &buffConn->DMADataSplitWr, &badSendWr);
                                if (ret) {
                                    fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                                    ret = -1;
                                }
                            }
                            else {
                                buffConn->DMADataSplitState = BUFF_READ_DATA_SPLIT_STATE_NOT_SPLIT;

                                ret = ibv_post_send(buffConn->QPair, &buffConn->DMADataWr, &badSendWr);
                                if (ret) {
                                    fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                                    ret = -1;
                                }
                            }
                        }
#else
#error "Unknown ring buffer implementation"
#endif
                    }
                        break;
                    case BUFF_READ_DATA_WR_ID: {
#if RING_BUFFER_IMPL == RING_BUFFER_IMPL_PROGRESSIVE
                        //
                        // Check splitting and update the head
                        //
                        //
                        if (buffConn->DMAReadDataSplitState == BUFF_READ_DATA_SPLIT_STATE_NOT_SPLIT) {
                            // TODO: parse requests
                        }
                        else {
                            buffConn->DMAReadDataSplitState++;
                        }
#elif RING_BUFFER_IMPL == RING_BUFFER_IMPL_PROGRESSIVE_NOTALIGNED
                        //
                        // Check splitting and update the head
                        //
                        //
                        if (buffConn->DMAReadDataSplitState == BUFF_READ_DATA_SPLIT_STATE_NOT_SPLIT) {
                            // TODO: parse requests
                            ret = ibv_post_send(buffConn->QPair, &buffConn->DMAWriteMetaWr, &badSendWr);
                            if (ret) {
                                fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                                ret = -1;
                            }
                        }
                        else {
                            buffConn->DMAReadDataSplitState++;
                        }
#elif RING_BUFFER_IMPL == RING_BUFFER_IMPL_LOCKBASED
                        //
                        // Check splitting and update the head
                        //
                        //
                        if (buffConn->DMAReadDataSplitState == BUFF_READ_DATA_SPLIT_STATE_NOT_SPLIT) {
                            // TODO: parse requests
                            ret = ibv_post_send(buffConn->QPair, &buffConn->DMAWriteMetaWr, &badSendWr);
                            if (ret) {
                                fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                                ret = -1;
                            }
                        }
                        else {
                            buffConn->DMAReadDataSplitState++;
                        }
#elif RING_BUFFER_IMPL == RING_BUFFER_IMPL_FARMSTYLE
                        //
                        // Check splitting and update the head
                        //
                        //
                        if (buffConn->DMADataSplitState == BUFF_READ_DATA_SPLIT_STATE_NOT_SPLIT) {
                            if (buffConn->DMADataBuff[buffConn->DMADataSize - 1]) {
                                //
                                // The request is ready
                                //
                                //

                                // TODO: parse requests

                                //
                                // Zero out the request
                                //
                                //
                                memset(buffConn->DMADataBuff, 0, buffConn->DMADataSize);
                                if (buffConn->DMADataSplitWr.wr.rdma.remote_addr) {
                                    buffConn->DMADataSplitState = BUFF_READ_DATA_SPLIT_STATE_SPLIT;
                                    buffConn->DMADataWr.opcode = IBV_WR_RDMA_WRITE;
                                    buffConn->DMADataWr.wr_id = BUFF_WRITE_DATA_WR_ID;
                                    buffConn->DMADataSplitWr.opcode = IBV_WR_RDMA_WRITE;
                                    buffConn->DMADataSplitWr.wr_id = BUFF_WRITE_DATA_SPLIT_WR_ID;

                                    ret = ibv_post_send(buffConn->QPair, &buffConn->DMADataWr, &badSendWr);
                                    if (ret) {
                                        fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                                        ret = -1;
                                    }

                                    ret = ibv_post_send(buffConn->QPair, &buffConn->DMADataSplitWr, &badSendWr);
                                    if (ret) {
                                        fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                                        ret = -1;
                                    }
                                }
                                else {
                                    buffConn->DMADataSplitState = BUFF_READ_DATA_SPLIT_STATE_NOT_SPLIT;
                                    buffConn->DMADataWr.opcode = IBV_WR_RDMA_WRITE;
                                    buffConn->DMADataWr.wr_id = BUFF_WRITE_DATA_WR_ID;
                                    ret = ibv_post_send(buffConn->QPair, &buffConn->DMADataWr, &badSendWr);
                                    if (ret) {
                                        fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                                        ret = -1;
                                    }
                                }
                            }
                            else {
                                //
                                // The request is not ready
                                // Read again
                                //
                                //
                                if (buffConn->DMADataSplitWr.wr.rdma.remote_addr) {
                                    buffConn->DMADataSplitState = BUFF_READ_DATA_SPLIT_STATE_SPLIT;

                                    ret = ibv_post_send(buffConn->QPair, &buffConn->DMADataWr, &badSendWr);
                                    if (ret) {
                                        fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                                        ret = -1;
                                    }

                                    ret = ibv_post_send(buffConn->QPair, &buffConn->DMADataSplitWr, &badSendWr);
                                    if (ret) {
                                        fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                                        ret = -1;
                                    }
                                }
                                else {
                                    buffConn->DMADataSplitState = BUFF_READ_DATA_SPLIT_STATE_NOT_SPLIT;
                                    ret = ibv_post_send(buffConn->QPair, &buffConn->DMADataWr, &badSendWr);
                                    if (ret) {
                                        fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                                        ret = -1;
                                    }
                                }
                            }
                        }
                        else {
                            buffConn->DMADataSplitState++;
                        }
#else
#error "Unknown ring buffer implementation"
#endif
                    }
                        break;
                    case BUFF_READ_DATA_SPLIT_WR_ID: {
#if RING_BUFFER_IMPL == RING_BUFFER_IMPL_PROGRESSIVE
                        //
                        // Check splitting and update the head
                        //
                        //
                        if (buffConn->DMAReadDataSplitState == BUFF_READ_DATA_SPLIT_STATE_NOT_SPLIT) {
                            // TODO: parse requests
                            ret = ibv_post_send(buffConn->QPair, &buffConn->DMAWriteMetaWr, &badSendWr);
                            if (ret) {
                                fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                                ret = -1;
                            }
                        }
                        else {
                            buffConn->DMAReadDataSplitState++;
                        }
#elif RING_BUFFER_IMPL == RING_BUFFER_IMPL_PROGRESSIVE_NOTALIGNED
                        //
                        // Check splitting and update the head
                        //
                        //
                        if (buffConn->DMAReadDataSplitState == BUFF_READ_DATA_SPLIT_STATE_NOT_SPLIT) {
                            // TODO: parse requests
                            ret = ibv_post_send(buffConn->QPair, &buffConn->DMAWriteMetaWr, &badSendWr);
                            if (ret) {
                                fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                                ret = -1;
                            }
                        }
                        else {
                            buffConn->DMAReadDataSplitState++;
                        }
#elif RING_BUFFER_IMPL == RING_BUFFER_IMPL_LOCKBASED
                        //
                        // Check splitting and update the head
                        //
                        //
                        if (buffConn->DMAReadDataSplitState == BUFF_READ_DATA_SPLIT_STATE_NOT_SPLIT) {
                            // TODO: parse requests
                            ret = ibv_post_send(buffConn->QPair, &buffConn->DMAWriteMetaWr, &badSendWr);
                            if (ret) {
                                fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                                ret = -1;
                            }
                        }
                        else {
                            buffConn->DMAReadDataSplitState++;
                        }
#elif RING_BUFFER_IMPL == RING_BUFFER_IMPL_FARMSTYLE
                        //
                        // Check splitting and update the head
                        //
                        //
                        if (buffConn->DMADataSplitState == BUFF_READ_DATA_SPLIT_STATE_NOT_SPLIT) {
                            if (buffConn->DMADataBuff[buffConn->DMADataSize - 1]) {
                                //
                                // The request is ready
                                //
                                //

                                // TODO: parse requests

                                //
                                // Zero out the request
                                //
                                //
                                memset(buffConn->DMADataBuff, 0, buffConn->DMADataSize);
                                if (buffConn->DMADataSplitWr.wr.rdma.remote_addr) {
                                    buffConn->DMADataSplitState = BUFF_READ_DATA_SPLIT_STATE_SPLIT;
                                    buffConn->DMADataWr.opcode = IBV_WR_RDMA_WRITE;
                                    buffConn->DMADataWr.wr_id = BUFF_WRITE_DATA_WR_ID;
                                    buffConn->DMADataSplitWr.opcode = IBV_WR_RDMA_WRITE;
                                    buffConn->DMADataSplitWr.wr_id = BUFF_WRITE_DATA_SPLIT_WR_ID;

                                    ret = ibv_post_send(buffConn->QPair, &buffConn->DMADataWr, &badSendWr);
                                    if (ret) {
                                        fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                                        ret = -1;
                                    }

                                    ret = ibv_post_send(buffConn->QPair, &buffConn->DMADataSplitWr, &badSendWr);
                                    if (ret) {
                                        fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                                        ret = -1;
                                    }
                                }
                                else {
                                    buffConn->DMADataSplitState = BUFF_READ_DATA_SPLIT_STATE_NOT_SPLIT;
                                    buffConn->DMADataWr.opcode = IBV_WR_RDMA_WRITE;
                                    buffConn->DMADataWr.wr_id = BUFF_WRITE_DATA_WR_ID;
                                    ret = ibv_post_send(buffConn->QPair, &buffConn->DMADataWr, &badSendWr);
                                    if (ret) {
                                        fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                                        ret = -1;
                                    }
                                }
                            }
                            else {
                                //
                                // The request is not ready
                                // Read again
                                //
                                //
                                if (buffConn->DMADataSplitWr.wr.rdma.remote_addr) {
                                    buffConn->DMADataSplitState = BUFF_READ_DATA_SPLIT_STATE_SPLIT;

                                    ret = ibv_post_send(buffConn->QPair, &buffConn->DMADataWr, &badSendWr);
                                    if (ret) {
                                        fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                                        ret = -1;
                                    }

                                    ret = ibv_post_send(buffConn->QPair, &buffConn->DMADataSplitWr, &badSendWr);
                                    if (ret) {
                                        fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                                        ret = -1;
                                    }
                                }
                                else {
                                    buffConn->DMADataSplitState = BUFF_READ_DATA_SPLIT_STATE_NOT_SPLIT;
                                    ret = ibv_post_send(buffConn->QPair, &buffConn->DMADataWr, &badSendWr);
                                    if (ret) {
                                        fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                                        ret = -1;
                                    }
                                }
                            }
                        }
                        else {
                            buffConn->DMADataSplitState++;
                        }
#else
#error "Unknown ring buffer implementation"
#endif
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
#if RING_BUFFER_IMPL == RING_BUFFER_IMPL_PROGRESSIVE
                    case BUFF_WRITE_META_WR_ID: {
                        //
                        // Ready to poll
                        //
                        //
                        ret = ibv_post_send(buffConn->QPair, &buffConn->DMAReadMetaWr, &badSendWr);
                        if (ret) {
                            fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                            ret = -1;
                        }
                    }
                        break;
#elif RING_BUFFER_IMPL == RING_BUFFER_IMPL_PROGRESSIVE_NOTALIGNED
                    case BUFF_WRITE_META_WR_ID: {
                        //
                        // Ready to poll
                        //
                        //
                        ret = ibv_post_send(buffConn->QPair, &buffConn->DMAReadMetaWr, &badSendWr);
                        if (ret) {
                            fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                            ret = -1;
                        }
                    }
                        break;
#elif RING_BUFFER_IMPL == RING_BUFFER_IMPL_LOCKBASED
                    case BUFF_WRITE_META_WR_ID: {
                        //
                        // Ready to poll
                        //
                        //
                        ret = ibv_post_send(buffConn->QPair, &buffConn->DMAReadMetaWr, &badSendWr);
                        if (ret) {
                            fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                            ret = -1;
                        }
                    }
                        break;
#elif RING_BUFFER_IMPL == RING_BUFFER_IMPL_FARMSTYLE
                    case BUFF_WRITE_META_WR_ID: {
                        //
                        // Ready to poll
                        //
                        //
                        buffConn->DMAReadMetaWr.wr.rdma.remote_addr = buffConn->RequestRing.DataBaseAddr + buffConn->RequestRing.Head;
                        ret = ibv_post_send(buffConn->QPair, &buffConn->DMAReadMetaWr, &badSendWr);
                        if (ret) {
                            fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                            ret = -1;
                        }
                    }
                        break;
                    case BUFF_WRITE_DATA_WR_ID: {
                        if (buffConn->DMADataSplitState == BUFF_READ_DATA_SPLIT_STATE_NOT_SPLIT) {
                            //
                            // Update head
                            //
                            //
                            buffConn->RequestRing.Head = (buffConn->RequestRing.Head + buffConn->DMADataSize) % DDS_REQUEST_RING_BYTES;
                            ret = ibv_post_send(buffConn->QPair, &buffConn->DMAWriteMetaWr, &badSendWr);
                            if (ret) {
                                fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                                ret = -1;
                            }
                        }
                        else {
                            buffConn->DMADataSplitState++;
                        }
                    }
                        break;
                    case BUFF_WRITE_DATA_SPLIT_WR_ID: {
                        if (buffConn->DMADataSplitState == BUFF_READ_DATA_SPLIT_STATE_NOT_SPLIT) {
                            //
                            // Update head
                            //
                            //
                            buffConn->RequestRing.Head = (buffConn->RequestRing.Head + buffConn->DMADataSize) % DDS_REQUEST_RING_BYTES;
                            ret = ibv_post_send(buffConn->QPair, &buffConn->DMAWriteMetaWr, &badSendWr);
                            if (ret) {
                                fprintf(stderr, "%s [error]: ibv_post_send failed: %d\n", __func__, ret);
                                ret = -1;
                            }
                        }
                        else {
                            buffConn->DMADataSplitState++;
                        }
                    }
                        break;
#endif
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
) {
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
    config.Prefetching = Prefetching;

    //
    // Initialize DMA
    //
    //
    ret = InitDMA(&config.DMAConf, config.ServerIp, config.ServerPort);
    if (ret) {
        fprintf(stderr, "InitDMA failed with %d\n", ret);
        return ret;
    }

    //
    // Allocate connections
    //
    //
    ret = AllocConns(&config);
    if (ret) {
        fprintf(stderr, "AllocConns failed with %d\n", ret);
        TermDMA(&config.DMAConf);
        return ret;
    }

    //
    // Listen for incoming connections
    //
    //
    ret = rdma_listen(config.DMAConf.CmId, LISTEN_BACKLOG);
    if (ret) {
        ret = errno;
        fprintf(stderr, "rdma_listen error %d\n", ret);
        return ret;
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
    }

    //
    // Clean up
    //
    //
    DeallocConns(&config);
    TermDMA(&config.DMAConf);

    return ret;
}

int main() {
    return RunFileBackEnd(DDS_BACKEND_ADDR, DDS_BACKEND_PORT, 32, 32, TRUE);
}
