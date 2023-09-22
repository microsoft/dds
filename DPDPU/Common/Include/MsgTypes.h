#pragma once

#include <stdint.h>

#include "DDSTypes.h"

#define MSG_CTXT ((void *) 0x5000)

#define CTRL_CONN_PRIV_DATA 42
#define BUFF_CONN_PRIV_DATA 24

#define CTRL_MSG_SIZE 256
#define BUFF_MSG_SIZE 64

#define CTRL_MSG_F2B_REQUEST_ID 0
#define CTRL_MSG_F2B_TERMINATE 1
#define CTRL_MSG_B2F_RESPOND_ID 2
#define CTRL_MSG_F2B_REQ_CREATE_DIR 3
#define CTRL_MSG_B2F_ACK_CREATE_DIR 4
#define CTRL_MSG_F2B_REQ_REMOVE_DIR 5
#define CTRL_MSG_B2F_ACK_REMOVE_DIR 6
#define CTRL_MSG_F2B_REQ_CREATE_FILE 7
#define CTRL_MSG_B2F_ACK_CREATE_FILE 8
#define CTRL_MSG_F2B_REQ_DELETE_FILE 9
#define CTRL_MSG_B2F_ACK_DELETE_FILE 10
#define CTRL_MSG_F2B_REQ_CHANGE_FILE_SIZE 11
#define CTRL_MSG_B2F_ACK_CHANGE_FILE_SIZE 12
#define CTRL_MSG_F2B_REQ_GET_FILE_SIZE 13
#define CTRL_MSG_B2F_ACK_GET_FILE_SIZE 14
#define CTRL_MSG_F2B_REQ_GET_FILE_INFO 15
#define CTRL_MSG_B2F_ACK_GET_FILE_INFO 16
#define CTRL_MSG_F2B_REQ_GET_FILE_ATTR 17
#define CTRL_MSG_B2F_ACK_GET_FILE_ATTR 18
#define CTRL_MSG_F2B_REQ_GET_FREE_SPACE 19
#define CTRL_MSG_B2F_ACK_GET_FREE_SPACE 20
#define CTRL_MSG_F2B_REQ_MOVE_FILE 21
#define CTRL_MSG_B2F_ACK_MOVE_FILE 22

#define BUFF_MSG_F2B_REQUEST_ID 100
#define BUFF_MSG_B2F_RESPOND_ID 101
#define BUFF_MSG_F2B_RELEASE 102

typedef struct {
    RequestIdT MsgId;
} MsgHeader;

typedef struct {
    int Dummy;
} CtrlMsgF2BRequestId;

typedef struct {
    int ClientId;
} CtrlMsgF2BTerminate;

typedef struct {
    int ClientId;
} CtrlMsgB2FRespondId;

typedef struct {
    int ClientId;
    uint64_t BufferAddress;
    uint32_t AccessToken;
    uint32_t Capacity;
} BuffMsgF2BRequestId;

typedef struct {
    int BufferId;
} BuffMsgB2FRespondId;

typedef struct {
    int ClientId;
    int BufferId;
} BuffMsgF2BRelease;

typedef struct {
    DirIdT DirId;
    DirIdT ParentDirId;
    char PathName[DDS_MAX_FILE_PATH];
} CtrlMsgF2BReqCreateDirectory;

typedef struct {
    ErrorCodeT Result;
} CtrlMsgB2FAckCreateDirectory;

typedef struct {
    DirIdT DirId;
} CtrlMsgF2BReqRemoveDirectory;

typedef struct {
    ErrorCodeT Result;
} CtrlMsgB2FAckRemoveDirectory;

typedef struct {
    FileIdT FileId;
    DirIdT DirId;
    FileAttributesT FileAttributes;
    char FileName[DDS_MAX_FILE_PATH];
} CtrlMsgF2BReqCreateFile;

typedef struct {
    ErrorCodeT Result;
} CtrlMsgB2FAckCreateFile;

typedef struct {
    FileIdT FileId;
    DirIdT DirId;
} CtrlMsgF2BReqDeleteFile;

typedef struct {
    ErrorCodeT Result;
} CtrlMsgB2FAckDeleteFile;

typedef struct {
    FileIdT FileId;
    FileSizeT NewSize;
} CtrlMsgF2BReqChangeFileSize;

typedef struct {
    ErrorCodeT Result;
} CtrlMsgB2FAckChangeFileSize;

typedef struct {
    FileIdT FileId;
} CtrlMsgF2BReqGetFileSize;

typedef struct {
    ErrorCodeT Result;
    FileSizeT FileSize;
} CtrlMsgB2FAckGetFileSize;

typedef struct {
    FileIdT FileId;
} CtrlMsgF2BReqGetFileInfo;

typedef struct {
    ErrorCodeT Result;
    FilePropertiesT FileInfo;
} CtrlMsgB2FAckGetFileInfo;

typedef struct {
    FileIdT FileId;
} CtrlMsgF2BReqGetFileAttr;

typedef struct {
    ErrorCodeT Result;
    FileAttributesT FileAttr;
} CtrlMsgB2FAckGetFileAttr;

typedef struct {
    int Dummy;
} CtrlMsgF2BReqGetFreeSpace;

typedef struct {
    ErrorCodeT Result;
    FileSizeT FreeSpace;
} CtrlMsgB2FAckGetFreeSpace;

typedef struct {
    FileIdT FileId;
    DirIdT OldDirId;
    DirIdT NewDirId;
    char NewFileName[DDS_MAX_FILE_PATH];
} CtrlMsgF2BReqMoveFile;

typedef struct {
    ErrorCodeT Result;
} CtrlMsgB2FAckMoveFile;

typedef struct {
    RequestIdT RequestId;
    FileIdT FileId;
    FileIOSizeT Bytes;
    FileSizeT Offset;
} BuffMsgF2BReqHeader;

typedef struct {
    RequestIdT RequestId;
    ErrorCodeT Result;
    FileIOSizeT BytesServiced;
} BuffMsgB2FAckHeader;

//
// Check a few parameters at the compile time
//
//
#define assert_static_msg_type(e, num) \
    enum { assert_static_msg_type__##num = 1/(e) }

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result" 
#else
#pragma warning(push)
#pragma warning (disable: 4804)
#endif
//
// Ring space is allocated at the |size of a request/response| + |header size|
// The alignment is enforced once a request/response is inserted into the ring
//
//
assert_static_msg_type(DDS_REQUEST_RING_BYTES % (sizeof(BuffMsgF2BReqHeader) + sizeof(FileIOSizeT)) == 0, 0);
assert_static_msg_type(DDS_RESPONSE_RING_BYTES % (sizeof(BuffMsgB2FAckHeader) + sizeof(FileIOSizeT)) == 0, 1);
#ifdef __GNUC__
#pragma GCC diagnostic pop
#else
#pragma warning(pop)
#endif