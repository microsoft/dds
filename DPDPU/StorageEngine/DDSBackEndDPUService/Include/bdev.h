#pragma once

#include "DPUBackEnd.h"
#include <stdint.h>
//
// We'll use this struct to gather housekeeping hello_context to pass between
// our events and callbacks.
//
//
struct hello_context_t {
	struct spdk_bdev *bdev;
	struct spdk_bdev_desc *bdev_desc;
	struct spdk_io_channel *bdev_io_channel;
	char *buff;
	uint32_t buff_size;
	char *bdev_name;
	struct spdk_bdev_io_wait_entry bdev_io_wait;
};

// char *G_BDEV_NAME;

typedef struct SPDKContext {
	struct spdk_bdev *bdev;
	struct spdk_bdev_desc *bdev_desc;
	struct spdk_io_channel *bdev_io_channel;  // channel would be per thread unique, meaning this whole context should also be
	char *buff;
	unsigned long long buff_size;
	char *bdev_name;
	struct spdk_bdev_io_wait_entry bdev_io_wait;
    void *cookie;  // just in case, a completion cookie that could be anything
    int *SPDKSpace; // an int array to record the status of each block memory inside buff. 0 = free, 1 = in use
} SPDKContextT;

//
// Dummy Callback function for read io completion.
//
//
void read_complete_dummy(
    struct spdk_bdev_io *bdev_io, 
    bool success, 
    void *cb_arg
);

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
	void *cb_arg,
    bool zeroCopy,
    int position
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
// [0, ONE_GB). But the acutal position will be added ONE_GB to 
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
	void *cb_arg,
    bool zeroCopy,
    int position
);

//
// This function is used to initialize BDEV
//
//
static struct hello_context_t* Init();

void dds_bdev_event_cb(enum spdk_bdev_event_type type, struct spdk_bdev *bdev,
		    void *event_ctx);
