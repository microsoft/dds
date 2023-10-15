#pragma once

#include "DPUBackEnd.h"
#include <stdint.h>
// #include "Zmalloc.h"
#include <pthread.h>

extern char *G_BDEV_NAME;

typedef struct SPDKContext {
	struct spdk_bdev *bdev;
	struct spdk_bdev_desc *bdev_desc;
	struct spdk_io_channel *bdev_io_channel;  // channel would be per thread unique, meaning this whole context should also be
	char *buff;
	unsigned long long buff_size;
	char *bdev_name;
	struct spdk_bdev_io_wait_entry bdev_io_wait;
    void *cookie;  // just in case, a completion cookie that could be anything
    struct PerSlotContext *SPDKSpace; // an array to record the status of each slot memory inside buff.
    pthread_mutex_t SpaceMutex; // used for SPDKSpace
} SPDKContextT;


//
// this is already deprecated, DO NOT USE
//
//
// extern SPDKContextT *SPDKContext;

//
// Bdev read function, zeroCopy is used to provide different buffer
// when it is 1, it means we will use DstBuffer in spdk_bdev_read()
// when it is 0, it means we will use context->buff as buffer
// position is used for async only and should belongs 
// [0, ONE_GB). Sync read should set position = 0
//
//
int bdev_read(
    void *arg,
    char* DstBuffer,
    uint64_t offset,
    uint64_t nbytes,
    spdk_bdev_io_completion_cb cb,
	void *cb_arg
);

int bdev_readv(
	void *arg,
    struct iovec *iov,
	int iovcnt,
    uint64_t offset,
    uint64_t nbytes,
	spdk_bdev_io_completion_cb cb,
	void *cb_arg
);

//
// Callback function for read io completion.
//
//
void write_complete(
    struct spdk_bdev_io *bdev_io, 
    bool success, 
    void *cb_arg
);

//
// Bdev write function
// position is used for async only and should between belongs
// [0, ONE_GB). But the actual position will be added ONE_GB to 
// separate space from read.
// Sync write should set position = 0
//
//
int bdev_write(
    void *arg,
    char* SrcBuffer,
    uint64_t offset,
    uint64_t nbytes,
    spdk_bdev_io_completion_cb cb,
	void *cb_arg
);

int bdev_writev(
    void *arg,
    struct iovec *iov,
    int iovcnt,
    uint64_t offset,
    uint64_t nbytes,
	spdk_bdev_io_completion_cb cb,
	void *cb_arg
);

//
// This function is used to initialize BDEV
//
//
struct hello_context_t* Init();

void dds_bdev_event_cb(enum spdk_bdev_event_type type, struct spdk_bdev *bdev,
		    void *event_ctx);

void bdev_reset_zone(void *arg);
