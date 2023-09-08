#include "../Include/bdev.h"


/*
 * Callback function for read io completion.
 */
void read_complete_dummy(struct spdk_bdev_io *bdev_io, bool success, void *cb_arg)
{
	SPDKContextT *spdkContext = cb_arg;

	if (success) {
		SPDK_NOTICELOG("Read string from bdev : %s\n", spdkContext->buff);
	} else {
		SPDK_ERRLOG("bdev io read error\n");
	}

	/* Complete the bdev io and close the channel */
	spdk_bdev_free_io(bdev_io);
	spdk_put_io_channel(spdkContext->bdev_io_channel);
	spdk_bdev_close(spdkContext->bdev_desc);
	SPDK_NOTICELOG("Stopping app\n");
	spdk_app_stop(success ? 0 : -1);
}

int bdev_read(
    void *arg,
    char* DstBuffer,
    uint64_t offset,
    uint64_t nbytes,
	spdk_bdev_io_completion_cb cb,
	void *cb_arg,
    bool zeroCopy,
	int position
){
	SPDKContextT *spdkContext = arg;
	int rc = 0; 
	char* buffer;
	if (zeroCopy){
		buffer = DstBuffer;
	}
	else{
		buffer = spdkContext->buff[position];
	}

	SPDK_NOTICELOG("Reading io\n");
	rc = spdk_bdev_read(spdkContext->bdev_desc, spdkContext->bdev_io_channel,
			    buffer, offset, nbytes, cb, cb_arg);

	if (rc == -ENOMEM) {
		SPDK_NOTICELOG("Queueing io\n");
		/* In case we cannot perform I/O now, queue I/O */
		spdkContext->bdev_io_wait.bdev = spdkContext->bdev;
		spdkContext->bdev_io_wait.cb_fn = bdev_read;
		spdkContext->bdev_io_wait.cb_arg = spdkContext;
		spdk_bdev_queue_io_wait(spdkContext->bdev, spdkContext->bdev_io_channel,
					&spdkContext->bdev_io_wait);
		return 0;  // eventually this IO should finish successfully
	} else if (rc == -EINVAL) {
		SPDK_ERRLOG("offset and/or nbytes are not aligned or out of range\n");
		return rc;
	} else if (rc < 0) {  // unknown error?
		SPDK_ERRLOG("%s error while reading from bdev: %d\n", spdk_strerror(-rc), rc);
		spdk_put_io_channel(spdkContext->bdev_io_channel);
		spdk_bdev_close(spdkContext->bdev_desc);
		spdk_app_stop(-1);
		return rc;
	}
	else
		return 0;  // success
}

/*
 * Callback function for write io completion.
 */
void write_complete(struct spdk_bdev_io *bdev_io, bool success, void *cb_arg)
{
	struct hello_context_t *hello_context = cb_arg;

	/* Complete the I/O */
	spdk_bdev_free_io(bdev_io);

	if (success) {
		SPDK_NOTICELOG("bdev io write completed successfully\n");
	} else {
		SPDK_ERRLOG("bdev io write error: %d\n", EIO);
		spdk_put_io_channel(hello_context->bdev_io_channel);
		spdk_bdev_close(hello_context->bdev_desc);
		spdk_app_stop(-1);
		return;
	}

	/* Zero the buffer so that we can use it for reading */
	//memset(hello_context->buff, 0, hello_context->buff_size);

	//hello_read(hello_context);
}

int bdev_write(
    void *arg,
    char* SrcBuffer,
    uint64_t offset,
    uint64_t nbytes,
	spdk_bdev_io_completion_cb cb,
	void *cb_arg,
    bool zeroCopy,
	int position
){
	SPDKContextT *spdkContext = arg;
	int rc = 0;
    char* buffer;
	if (zeroCopy){
		buffer = SrcBuffer;
	}
	else{
		buffer = spdkContext->buff[position + ONE_GB];
	}

	SPDK_NOTICELOG("Writing to the bdev\n");
	rc = spdk_bdev_write(spdkContext->bdev_desc, spdkContext->bdev_io_channel,
			     buffer, offset, nbytes, cb, cb_arg);

	if (rc == -ENOMEM) {
		SPDK_NOTICELOG("Queueing io\n");
		/* In case we cannot perform I/O now, queue I/O */
		spdkContext->bdev_io_wait.bdev = spdkContext->bdev;
		spdkContext->bdev_io_wait.cb_fn = bdev_write;
		spdkContext->bdev_io_wait.cb_arg = spdkContext;
		spdk_bdev_queue_io_wait(spdkContext->bdev, spdkContext->bdev_io_channel,
					&spdkContext->bdev_io_wait);
		return 0;  // eventually this IO should finish successfully
	} else if (rc == -EINVAL) {
		SPDK_ERRLOG("offset and/or nbytes are not aligned or out of range\n");
		return rc;
	} else if (rc < 0) {  // unknown error?
		SPDK_ERRLOG("%s error while writing to bdev: %d\n", spdk_strerror(-rc), rc);
		spdk_put_io_channel(spdkContext->bdev_io_channel);
		spdk_bdev_close(spdkContext->bdev_desc);
		spdk_app_stop(-1);
		return rc;
	}
	else
		return 0;  // success
}

static void
hello_bdev_event_cb(enum spdk_bdev_event_type type, struct spdk_bdev *bdev,
		    void *event_ctx)
{
	SPDK_NOTICELOG("Unsupported bdev event: type %d\n", type);
}

static void
reset_zone_complete(struct spdk_bdev_io *bdev_io, bool success, void *cb_arg)
{
	struct hello_context_t *hello_context = cb_arg;

	/* Complete the I/O */
	spdk_bdev_free_io(bdev_io);

	if (!success) {
		SPDK_ERRLOG("bdev io reset zone error: %d\n", EIO);
		spdk_put_io_channel(hello_context->bdev_io_channel);
		spdk_bdev_close(hello_context->bdev_desc);
		spdk_app_stop(-1);
		return;
	}

	hello_write(hello_context);
}

static void
hello_reset_zone(void *arg)
{
	struct hello_context_t *hello_context = arg;
	int rc = 0;

	rc = spdk_bdev_zone_management(hello_context->bdev_desc, hello_context->bdev_io_channel,
				       0, SPDK_BDEV_ZONE_RESET, reset_zone_complete, hello_context);

	if (rc == -ENOMEM) {
		SPDK_NOTICELOG("Queueing io\n");
		/* In case we cannot perform I/O now, queue I/O */
		hello_context->bdev_io_wait.bdev = hello_context->bdev;
		hello_context->bdev_io_wait.cb_fn = hello_reset_zone;
		hello_context->bdev_io_wait.cb_arg = hello_context;
		spdk_bdev_queue_io_wait(hello_context->bdev, hello_context->bdev_io_channel,
					&hello_context->bdev_io_wait);
	} else if (rc) {
		SPDK_ERRLOG("%s error while resetting zone: %d\n", spdk_strerror(-rc), rc);
		spdk_put_io_channel(hello_context->bdev_io_channel);
		spdk_bdev_close(hello_context->bdev_desc);
		spdk_app_stop(-1);
	}
}

void dds_bdev_event_cb(enum spdk_bdev_event_type type, struct spdk_bdev *bdev,
		    void *event_ctx)
{
	SPDK_NOTICELOG("Unsupported bdev event: type %d\n", type);
}