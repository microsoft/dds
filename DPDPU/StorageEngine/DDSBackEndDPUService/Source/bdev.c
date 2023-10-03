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
	void *cb_arg
){
	SPDKContextT *spdkContext = arg;
	int rc = 0; 

	SPDK_NOTICELOG("bdev_read run on thread: %d\n", spdk_thread_get_id(spdk_get_thread()));
	SPDK_NOTICELOG("Reading io, cb_arg@%p: %hu\n", cb_arg, *((unsigned short *) cb_arg));
	rc = spdk_bdev_read(spdkContext->bdev_desc, spdkContext->bdev_io_channel,
			    DstBuffer, offset, nbytes, cb, cb_arg);
	SPDK_NOTICELOG("spdk_bdev_read returned: %d\n", rc);
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


int bdev_write(
    void *arg,
    char* SrcBuffer,
    uint64_t offset,
    uint64_t nbytes,
	spdk_bdev_io_completion_cb cb,
	void *cb_arg
){
	SPDKContextT *spdkContext = arg;
	int rc = 0;

	SPDK_NOTICELOG("Writing to the bdev\n");
	rc = spdk_bdev_write(spdkContext->bdev_desc, spdkContext->bdev_io_channel,
			     SrcBuffer, offset, nbytes, cb, cb_arg);

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

void
reset_zone_complete(struct spdk_bdev_io *bdev_io, bool success, void *cb_arg)
{
	SPDKContextT *SPDKContext = cb_arg;

	/* Complete the I/O */
	spdk_bdev_free_io(bdev_io);

	if (!success) {
		SPDK_ERRLOG("bdev io reset zone error: %d\n", EIO);
		spdk_put_io_channel(SPDKContext->bdev_io_channel);
		spdk_bdev_close(SPDKContext->bdev_desc);
		spdk_app_stop(-1);
		return;
	}

	//hello_write(hello_context);
}

void
bdev_reset_zone(void *arg)
{
	SPDKContextT *SPDKContext = arg;
	int rc = 0;

	rc = spdk_bdev_zone_management(SPDKContext->bdev_desc, SPDKContext->bdev_io_channel,
				       0, SPDK_BDEV_ZONE_RESET, reset_zone_complete, SPDKContext);

	if (rc == -ENOMEM) {
		SPDK_NOTICELOG("Queueing io\n");
		/* In case we cannot perform I/O now, queue I/O */
		SPDKContext->bdev_io_wait.bdev = SPDKContext->bdev;
		SPDKContext->bdev_io_wait.cb_fn = bdev_reset_zone;
		SPDKContext->bdev_io_wait.cb_arg = SPDKContext;
		spdk_bdev_queue_io_wait(SPDKContext->bdev, SPDKContext->bdev_io_channel,
					&SPDKContext->bdev_io_wait);
	} else if (rc) {
		SPDK_ERRLOG("%s error while resetting zone: %d\n", spdk_strerror(-rc), rc);
		spdk_put_io_channel(SPDKContext->bdev_io_channel);
		spdk_bdev_close(SPDKContext->bdev_desc);
		spdk_app_stop(-1);
	}
}

void dds_bdev_event_cb(enum spdk_bdev_event_type type, struct spdk_bdev *bdev,
		    void *event_ctx)
{
	SPDK_NOTICELOG("Unsupported bdev event: type %d\n", type);
}