#pragma once

#include "spdk/stdinc.h"
#include "spdk/thread.h"
#include "spdk/bdev.h"
#include "spdk/env.h"
#include "spdk/event.h"
#include "spdk/log.h"
#include "spdk/string.h"
#include "spdk/bdev_zone.h"
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

static char *G_BDEV_NAME = "Malloc0";

typedef struct spdkContext {
	struct spdk_bdev *bdev;
	struct spdk_bdev_desc *bdev_desc;
	struct spdk_io_channel *bdev_io_channel;
	char *buff;
	uint32_t buff_size;
	char *bdev_name;
	struct spdk_bdev_io_wait_entry bdev_io_wait;
    void *cookie;  // just in case, a completion cookie that could be anything
} spdkContextT;

//
// Dummy Callback function for read io completion.
//
//
static void read_complete_dummy(
    struct spdk_bdev_io *bdev_io, 
    bool success, 
    void *cb_arg
);

//
// Bdev read function, zeroCopy is used to provide different buffer
// when it is 1, it means we will use DstBuffer in spdk_bdev_read()
// when it is 0, it means we will use context->buff as buffer
//
//
static int bdev_read(
    void *arg,
    char* DstBuffer,
    uint64_t offset,
    uint64_t nbytes,
    spdk_bdev_io_completion_cb cb,
	void *cb_arg,
    bool zeroCopy
);

//
// Callback function for read io completion.
//
//
static void write_complete(
    struct spdk_bdev_io *bdev_io, 
    bool success, 
    void *cb_arg
);

//
// Bdev write function
//
//
static void bdev_write(
    void *arg,
    char* SrcBuffer,
    uint64_t offset,
    uint64_t nbytes,
    bool zeroCopy
);

//
// This function is used to initialize BDEV
//
//
static struct hello_context_t* Init();

static void dds_bdev_event_cb(enum spdk_bdev_event_type type, struct spdk_bdev *bdev,
		    void *event_ctx)
{
	SPDK_NOTICELOG("Unsupported bdev event: type %d\n", type);
}