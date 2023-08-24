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
    // Every time we poll progess, we fetch two cache lines
    // One for the progress, one for the tail
    // Also, we separate pointers and content to enable prefetching
    //
    //
    struct ibv_send_wr RequestDMAReadDataWr;
    struct ibv_sge RequestDMAReadDataSgl;
    struct ibv_mr *RequestDMAReadDataMr;
    char* RequestDMAReadDataBuff;
    struct ibv_send_wr RequestDMAReadDataSplitWr;
    struct ibv_sge RequestDMAReadDataSplitSgl;
    uint32_t RequestDMAReadDataSize;
    uint8_t RequestDMAReadDataSplitState;
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
    // Every time we poll progess, we fetch two cache lines
    // One for the progress, one for the tail
    // Also, we separate pointers and content to enable prefetching
    //
    //
    struct ibv_send_wr ResponseDMAWriteDataWr;
    struct ibv_sge ResponseDMAWriteDataSgl;
    struct ibv_mr *ResponseDMAWriteDataMr;
    char* ResponseDMAWriteDataBuff;
    struct ibv_send_wr ResponseDMAWriteDataSplitWr;
    struct ibv_sge ResponseDMAWriteDataSplitSgl;
    uint32_t ResponseDMAWriteDataSize;
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

//
// Types used in DPU
//
//
typedef char* BufferT;
typedef void* ContextT;
typedef int DirIdT;
typedef int ErrorCodeT;
typedef unsigned long FileAttributesT;
typedef int FileIdT;
typedef unsigned long FileIOSizeT;
typedef unsigned long long FileSizeT;
typedef unsigned long long DiskSizeT;
typedef int SegmentIdT;
typedef unsigned int SegmentSizeT;
typedef unsigned int FileNumberT;
using Mutex = std::mutex;

//
// Persistent properties of DPU dir
//
//
typedef struct DPUDirProperties {
        DirIdT Id;
        DirIdT Parent;
        FileNumberT NumFiles;
        char Name[DDS_MAX_FILE_PATH];
        FileIdT Files[DDS_MAX_FILES_PER_DIR];
}DPUDirPropertiesT;

//
// DPU dir, highly similar with BackEndDir
//
//
typedef struct DPUDir {
    DPUDirPropertiesT Properties;
    Mutex ModificationMutex;
    const SegmentSizeT AddressOnSegment;
};

//
// DPU segment, highly similar with Segement but has formatted mark
//
//
typedef struct DPUSegment {
    SegmentIdT Id;
    FileIdT FileId;
    FileSizeT DiskAddress;
    bool Allocatable;
    bool Formatted;
} DPUSegmentT;

//
// Persistent properties of DPU file
//
//
typedef struct DPUFileProperties {
        FileIdT Id;
        FileProperties FProperties;
        SegmentIdT Segments[DDS_BACKEND_MAX_SEGMENTS_PER_FILE];
} DPUFilePropertiesT;

//
// DPU file, highly similar with BackEndFile
//
//
typedef struct DPUFile {
    DPUFilePropertiesT Properties;
    SegmentIdT NumSegments;
    const SegmentSizeT AddressOnSegment;
};

//
// DPU storage, highly similar with BackEndStorage
//
//
typedef struct DPUStorage {
    SegmentT* AllSegments;
    const SegmentIdT TotalSegments;
    SegmentIdT AvailableSegments;
    
    struct DPUDir* AllDirs[DDS_MAX_DIRS];
    struct DPUFile* AllFiles[DDS_MAX_FILES];
    int TotalDirs;
    int TotalFiles;

    Mutex SectorModificationMutex;
    Mutex SegmentAllocationMutex;
    
    // Atomic<size_t> TargetProgress;
    // Atomic<size_t> CurrentProgress;
};