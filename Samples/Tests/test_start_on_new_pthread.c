/* try running sdpk_app_start() on a new pthread, it works */
#include <time.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdatomic.h>

#include "spdk/stdinc.h"
#include "spdk/thread.h"
#include "spdk/bdev.h"
#include "spdk/env.h"
#include "spdk/event.h"
#include "spdk/log.h"
#include "spdk/string.h"
#include "spdk/bdev_zone.h"
// #include "../../DPDPU/StorageEngine/DDSBackEndDPUService/Include/DPUBackEndStorage.h"
#include "../../DPDPU/StorageEngine/DDSBackEndDPUService/Include/bdev.h"

#include <pthread.h>
#include "spdk/thread.h"

// SPDKContextT *SPDKContext;

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


typedef atomic_ushort SyncRWCompletionStatus;

enum SyncRWCompletion {
    SyncRWCompletion_NOT_COMPLETED = 0,
    SyncRWCompletionSUCCESS = 1,
    SyncRWCompletionFAILED = 2
};


SPDKContextT *SPDKContext;


struct mythread_ctx {
    SPDKContextT *SPDKContext;
    SyncRWCompletionStatus cookie;
};

struct app_start_args {
    struct spdk_app_opts *opts;
    void (*app_start_wrapper_func) (void *);
    void *ctx;
};

struct app_thread_ctx {
    struct spdk_thread **spdk_threads;
};



void io1_callback_func(struct spdk_bdev_io *bdev_io, bool success, void *cb_arg) {
    SPDK_NOTICELOG("IO 1 Callback ran...\n");
    SPDK_NOTICELOG("callback run on ID %llu\n", spdk_thread_get_id(spdk_get_thread()));
    // SyncRWCompletionStatus *status = cb_arg;
    // *status = success ? 1 : 2;
    // SPDK_NOTICELOG("IO 1 Callback finished, set status %p to %d\n", status, *status);
    // SPDK_NOTICELOG("thread work finished... stopping app...\n");
    // XXX: MUST release resources, or thread exit will be broken, and app WON'T stop!!
    // spdk_bdev_close(SPDKContext->bdev_desc);
    // spdk_put_io_channel(SPDKContext->bdev_io_channel);

    // spdk_thread_exit(spdk_get_thread());
    // spdk_app_stop(0);
}

void io2_callback_func(struct spdk_bdev_io *bdev_io, bool success, void *cb_arg) {
    SPDK_NOTICELOG("IO 2 Callback running...\n");
    SyncRWCompletionStatus *status = cb_arg;
    *status = success ? 3 : 4;
    SPDK_NOTICELOG("IO 2 Callback finished, set status %p to %d\n", status, *status);
}


void thread_work_func(void *thread_ctx) {
    SPDK_NOTICELOG("thread work run on ID %llu\n", spdk_thread_get_id(spdk_get_thread()));
    SPDK_NOTICELOG("DOING WORK A...\n");

    void *tmpbuf = malloc(4096);
    SyncRWCompletionStatus cookie = 777;
    int rc = bdev_read(SPDKContext, tmpbuf, 0, 1024, io1_callback_func, &(cookie));
    SPDK_NOTICELOG("bdev_read() rc: %d\n", rc);

    SPDK_NOTICELOG("DOING WORK B...\n");
    // SPDK_NOTICELOG("thread work finished... stopping app...\n");
    // spdk_app_stop(0);
}

void TestBackEnd(
    void *args
) {
    SPDK_NOTICELOG("TestBackEnd: run on ID %llu\n", spdk_thread_get_id(spdk_get_thread()));
    struct app_thread_ctx *spdk_app_ctx = args;
    spdk_app_ctx->spdk_threads[0] = spdk_thread_create("thread0", NULL);
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

    while (0) {
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
        sleep(1);
    }

    /* SPDK_NOTICELOG("ALL WORK DONE, STOPPING...\n");
    spdk_bdev_close(spdkContext->bdev_desc);
    spdk_put_io_channel(spdkContext->bdev_io_channel);
    spdk_app_stop(0); */

    SPDKContext = spdkContext;

    int i = 0;
    while (i < 5) {
        spdk_thread_send_msg(spdk_app_ctx->spdk_threads[0], thread_work_func, NULL);
        SPDK_NOTICELOG("sent a msg to work thread...\n");
        // sleep(1);
        i += 1;
    }
    // spdk_thread_send_msg(spdk_app_ctx->spdk_threads[0], thread_work_func, NULL);
    SPDK_NOTICELOG("SPDK MAIN ended but not stopped, also has spdk threads spawned...\n");
}


void app_start_wrapper_func(void *ctx) {
    struct app_start_args *args = ctx;
    SPDK_NOTICELOG("app_start_wrapper_func() called\n");
    int rc = spdk_app_start(args->opts, args->app_start_wrapper_func, args->ctx);  // block until `spdk_app_stop` is called
    SPDK_NOTICELOG("app_start_wrapper_func() returned: %d\n", rc);
    return rc;
}

int main(int argc, char **argv) {
    int rc;
    // struct spdk_app_opts opts = {};
    /* Set default values in opts structure. */
	// spdk_app_opts_init(&opts, sizeof(opts));
	// opts.name = "hello_bdev";
    /*
	 * Parse built-in SPDK command line parameters as well
	 * as our custom one(s).
	 */
	/* if ((rc = spdk_app_parse_args(argc, argv, &opts, "b:", NULL, dds_parse_arg,
				      dds_custom_args_usage)) != SPDK_APP_PARSE_ARGS_SUCCESS) {
        printf("spdk_app_parse_args() failed with: %d\n", rc);
		exit(rc);
	} */
    
    pthread_t app_thread;
    struct spdk_app_opts opts = {};
    spdk_app_opts_init(&opts, sizeof(opts));
    opts.name = "hello_bdev";

    struct spdk_thread *spdk_app_threads[3];
    struct app_thread_ctx spdk_app_ctx = { .spdk_threads = spdk_app_threads};
    
    struct app_start_args app_start_ctx = {
        .app_start_wrapper_func = TestBackEnd,
        .opts = &opts,
        .ctx = &spdk_app_ctx
    };
    if ((rc = spdk_app_parse_args(argc, argv, &opts, "b:", NULL, dds_parse_arg,
				      dds_custom_args_usage)) != SPDK_APP_PARSE_ARGS_SUCCESS) {
        printf("spdk_app_parse_args() failed with: %d\n", rc);
		exit(rc);
	}

    pthread_create(&app_thread, NULL, app_start_wrapper_func, &app_start_ctx);
    // rc = spdk_app_start(&opts, TestBackEnd, NULL);  // block until `spdk_app_stop` is called
    // SPDK_NOTICELOG("spdk_app_start returned with: %d\n", rc);

    printf("NOTICE: joining app thread...\n");
    int *app_ret;
    int join_ret = pthread_join(app_thread, app_ret);
    printf("NOTICE: join returned: %d, app_ret: %d\n", join_ret, app_ret);

    /* while (1) {
        SPDK_NOTICELOG("main thread looping...\n");
        sleep(1);
    } */


    spdk_app_fini();
}