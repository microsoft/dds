#include "../Include/bdev.h"


static char *g_bdev_name = "Malloc0";

/*
 * Usage function for printing parameters that are specific to this application
 */
static void
hello_bdev_usage(void)
{
	printf(" -b <bdev>                 name of the bdev to use\n");
}

/*
 * This function is called to parse the parameters that are specific to this application
 */
static int
hello_bdev_parse_arg(int ch, char *arg)
{
	switch (ch) {
	case 'b':
		g_bdev_name = arg;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

/*
 * Callback function for read io completion.
 */
void read_complete_dummy(struct spdk_bdev_io *bdev_io, bool success, void *cb_arg)
{
	spdkContextT *spdkContext = cb_arg;

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
	spdkContextT *spdkContext = arg;
	int rc = 0; 
	char* buffer;
	if (zeroCopy){
		buffer = DstBuffer;
	}
	else{
		buffer = spdkContext->buff[position * ONE_MB];
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
	spdkContextT *spdkContext = arg;
	int rc = 0;
    char* buffer;
	if (zeroCopy){
		buffer = SrcBuffer;
	}
	else{
		buffer = spdkContext->buff[position * ONE_MB];
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

/*
 * Our initial event that kicks off everything from main().
 */
static void
hello_start(void *arg1)
{
	struct hello_context_t *hello_context = arg1;
	uint32_t buf_align;
	int rc = 0;
	hello_context->bdev = NULL;
	hello_context->bdev_desc = NULL;

	SPDK_NOTICELOG("Successfully started the application\n");

	/*
	 * There can be many bdevs configured, but this application will only use
	 * the one input by the user at runtime.
	 *
	 * Open the bdev by calling spdk_bdev_open_ext() with its name.
	 * The function will return a descriptor
	 */
	SPDK_NOTICELOG("Opening the bdev %s\n", hello_context->bdev_name);
	rc = spdk_bdev_open_ext(hello_context->bdev_name, true, hello_bdev_event_cb, NULL,
				&hello_context->bdev_desc);
	if (rc) {
		SPDK_ERRLOG("Could not open bdev: %s\n", hello_context->bdev_name);
		spdk_app_stop(-1);
		return;
	}

	/* A bdev pointer is valid while the bdev is opened. */
	hello_context->bdev = spdk_bdev_desc_get_bdev(hello_context->bdev_desc);


	SPDK_NOTICELOG("Opening io channel\n");
	/* Open I/O channel */
	hello_context->bdev_io_channel = spdk_bdev_get_io_channel(hello_context->bdev_desc);
	if (hello_context->bdev_io_channel == NULL) {
		SPDK_ERRLOG("Could not create bdev I/O channel!!\n");
		spdk_bdev_close(hello_context->bdev_desc);
		spdk_app_stop(-1);
		return;
	}

	/* Allocate memory for the write buffer.
	 * Initialize the write buffer with the string "Hello World!"
	 */
	hello_context->buff_size = spdk_bdev_get_block_size(hello_context->bdev) *
				   spdk_bdev_get_write_unit_size(hello_context->bdev);
	buf_align = spdk_bdev_get_buf_align(hello_context->bdev);
	hello_context->buff = spdk_dma_zmalloc(hello_context->buff_size, buf_align, NULL);
	if (!hello_context->buff) {
		SPDK_ERRLOG("Failed to allocate buffer\n");
		spdk_put_io_channel(hello_context->bdev_io_channel);
		spdk_bdev_close(hello_context->bdev_desc);
		spdk_app_stop(-1);
		return;
	}
	snprintf(hello_context->buff, hello_context->buff_size, "%s", "Hello World!\n");

	if (spdk_bdev_is_zoned(hello_context->bdev)) {
		hello_reset_zone(hello_context);
		/* If bdev is zoned, the callback, reset_zone_complete, will call hello_write() */
		return;
	}

	hello_write(hello_context);
}

void dds_bdev_event_cb(enum spdk_bdev_event_type type, struct spdk_bdev *bdev,
		    void *event_ctx)
{
	SPDK_NOTICELOG("Unsupported bdev event: type %d\n", type);
}