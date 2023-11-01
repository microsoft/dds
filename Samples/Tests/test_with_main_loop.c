#include "../../DPDPU/StorageEngine/DDSBackEndDPUService/Include/DPUBackEndStorage.h"
#include "../../DPDPU/StorageEngine/DDSBackEndDPUService/Include/bdev.h"

static void
dds_custom_args_usage(void)
{
	printf("if there is any custom cmdline params, add a description for it here\n");
}

//
// This function is called to parse the parameters that are specific to this application
//
//
static int
dds_parse_arg(int ch, char *arg)
{
	printf("parse custom arg func called, currently doing nothing...\n");
}

struct mythread_ctx {
    SPDKContextT *SPDKContext;
    SyncRWCompletionStatus cookie;
};


void io1_callback_func(struct spdk_bdev_io *bdev_io, bool success, void *cb_arg) {
    SPDK_NOTICELOG("IO 1 Callback running...\n");
    SyncRWCompletionStatus *status = cb_arg;
    *status = success ? 1 : 2;
    SPDK_NOTICELOG("IO 1 Callback finished, set status %p to %d\n", status, *status);
}

void io2_callback_func(struct spdk_bdev_io *bdev_io, bool success, void *cb_arg) {
    SPDK_NOTICELOG("IO 2 Callback running...\n");
    SyncRWCompletionStatus *status = cb_arg;
    *status = success ? 3 : 4;
    SPDK_NOTICELOG("IO 2 Callback finished, set status %p to %d\n", status, *status);
}


void TestBackEnd(
    void *args
) {
    //
    // Initialize SPDK stuff
    //
    //
    SPDKContextT *spdkContext = malloc(sizeof(SPDKContextT));
    char *BDEV_NAME = "Malloc0";
    spdkContext->bdev_name = BDEV_NAME;

    SPDK_NOTICELOG("Successfully started the application\n");
    int ret = spdk_bdev_open_ext(spdkContext->bdev_name, true, dds_bdev_event_cb, NULL,
				&spdkContext->bdev_desc);
	if (ret) {
		SPDK_ERRLOG("Could not open bdev: %s\n", spdkContext->bdev_name);
		spdk_app_stop(-1);
		return;
	}

    /* A bdev pointer is valid while the bdev is opened. */
	spdkContext->bdev = spdk_bdev_desc_get_bdev(spdkContext->bdev_desc);
    SPDK_NOTICELOG("Opening io channel\n");
	/* Open I/O channel */
	spdkContext->bdev_io_channel = spdk_bdev_get_io_channel(spdkContext->bdev_desc);
	if (spdkContext->bdev_io_channel == NULL) {
		SPDK_ERRLOG("Could not create bdev I/O channel!!\n");
		spdk_bdev_close(spdkContext->bdev_desc);
		spdk_app_stop(-1);
		return;
	}
    
    struct mythread_ctx *myctx = malloc(sizeof(*myctx));
    myctx->SPDKContext = spdkContext;
    myctx->cookie = 999;
    printf("myctx->cookie ptr: %p, value: %hu\n", &(myctx->cookie), myctx->cookie);

    SyncRWCompletionStatus *cb_arg = &(myctx->cookie);
    printf("cb_arg ptr: %p, value: %hu\n", cb_arg, *cb_arg);

    while (1) {
        SPDK_NOTICELOG("DOING WORK A...\n");

        SPDK_NOTICELOG("DOING IO 1..., status ptr: %p, value: %hu\n", &(myctx->cookie), myctx->cookie);
        void *tmpbuf = malloc(4096);
        int rc = bdev_read(myctx->SPDKContext, tmpbuf, 0, 1024, io1_callback_func, &(myctx->cookie));
        SPDK_NOTICELOG("bdev_read() rc: %d\n", rc);
        SPDK_NOTICELOG("FINISHED IO 1..., status ptr: %p, value: %hu\n", &(myctx->cookie), myctx->cookie);

        SPDK_NOTICELOG("DOING WORK B...\n");

        SPDK_NOTICELOG("DOING IO 2..., status ptr: %p, value: %hu\n", &(myctx->cookie), myctx->cookie);
        rc = bdev_read(myctx->SPDKContext, tmpbuf, 0, 1024, io2_callback_func, &(myctx->cookie));
        SPDK_NOTICELOG("bdev_read() rc: %d\n", rc);
        SPDK_NOTICELOG("FINISHED IO 2..., status ptr: %p, value: %hu\n", &(myctx->cookie), myctx->cookie);

        SPDK_NOTICELOG("DOING WORK C...\n");
    }
    


    


    SPDK_NOTICELOG("ALL WORK DONE, STOPPING...\n");
    spdk_bdev_close(spdkContext->bdev_desc);
    spdk_put_io_channel(spdkContext->bdev_io_channel);
    spdk_app_stop(0);
}


int main(int argc, char **argv) {
    int rc;
    struct spdk_app_opts opts = {};
    /* Set default values in opts structure. */
	spdk_app_opts_init(&opts, sizeof(opts));
	opts.name = "hello_bdev";
    /*
	 * Parse built-in SPDK command line parameters as well
	 * as our custom one(s).
	 */
	if ((rc = spdk_app_parse_args(argc, argv, &opts, "b:", NULL, dds_parse_arg,
				      dds_custom_args_usage)) != SPDK_APP_PARSE_ARGS_SUCCESS) {
        printf("spdk_app_parse_args() failed with: %d\n", rc);
		exit(rc);
	}
    
    rc = spdk_app_start(&opts, TestBackEnd, NULL);  // block until `spdk_app_stop` is called
    SPDK_NOTICELOG("spdk_app_start returned with: %d\n", rc);
    spdk_app_fini();
}