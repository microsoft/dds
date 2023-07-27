#pragma once

#define MSG_CTXT ((void *) 0x5000)

#define CTRL_CONN_PRIV_DATA ((void *) 0xcc)
#define BUFF_CONN_PRIV_DATA ((void *) 0xcb)

#define CTRL_MSG_SIZE 64
#define BUFF_MSG_SIZE 64

#define BUFF_MSG_SETUP (sizeof(int) + sizeof(uint64_t) + sizeof(uint32_t))
#define BUFF_MSG_RELEASE (sizeof(int))

#define CTRL_MSG_F2B_REQUEST_ID 0
#define CTRL_MSG_F2B_TERMINATE 1
#define CTRL_MSG_B2F_RESPOND_ID 2
#define BUFF_MSG_F2B_REQUEST_ID 3
#define BUFF_MSG_B2F_RESPOND_ID 4
#define BUFF_MSG_F2B_RELEASE 5

typedef struct {
    int MsgId;
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
} BuffMsgF2BRequestId;

typedef struct {
    int BufferId;
} BuffMsgB2FRespondId;

typedef struct {
    int ClientId;
    int BufferId;
} BuffMsgF2BRelease;