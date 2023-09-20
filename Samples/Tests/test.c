/* Benchmark the SPDK send msg (SPDK thread overhead) */

#include "../../DPDPU/StorageEngine/DDSBackEndDPUService/Include/DPUBackEndStorage.h"
#include "../../DPDPU/StorageEngine/DDSBackEndDPUService/Include/bdev.h"

#include <pthread.h>
#include "spdk/thread.h"
#include <time.h>

#define ITER 1000000

SPDKContextT *SPDKContext;

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

struct app_start_args {
    struct spdk_app_opts *opts;
    void (*app_start_wrapper_func) (void *);
    void *ctx;
};

struct app_thread_ctx {
    struct spdk_thread **spdk_threads;
    atomic_bool thread_created;
};

struct spdk_thread_work_ctx {
    atomic_ullong count;
};



void io1_callback_func(struct spdk_bdev_io *bdev_io, bool success, void *cb_arg) {
    SPDK_NOTICELOG("IO 1 Callback running...\n");
    SyncRWCompletionStatus *status = cb_arg;
    *status = success ? 1 : 2;
    SPDK_NOTICELOG("IO 1 Callback finished, set status %p to %d\n", status, *status);
    SPDK_NOTICELOG("thread work finished... stopping app...\n");
    // XXX: MUST release resources, or thread exit will be broken, and app WON'T stop!!
    spdk_bdev_close(SPDKContext->bdev_desc);
    spdk_put_io_channel(SPDKContext->bdev_io_channel);

    spdk_thread_exit(spdk_get_thread());
    spdk_app_stop(0);
}

void io2_callback_func(struct spdk_bdev_io *bdev_io, bool success, void *cb_arg) {
    SPDK_NOTICELOG("IO 2 Callback running...\n");
    SyncRWCompletionStatus *status = cb_arg;
    *status = success ? 3 : 4;
    SPDK_NOTICELOG("IO 2 Callback finished, set status %p to %d\n", status, *status);
}


void thread_work_func(void *thread_ctx) {
    // printf("thread_work_func() ran\n");
    struct spdk_thread_work_ctx *work_ctx = thread_ctx;
    work_ctx->count++;
    // printf("work_ctx->count = %ld\n", work_ctx->count);
    // SPDK_NOTICELOG("thread work finished... stopping app...\n");
    // spdk_app_stop(0);
}

#define NS_PER_SECOND 1000000000
void sub_timespec(struct timespec t1, struct timespec t2, struct timespec *td)
{
    td->tv_nsec = t2.tv_nsec - t1.tv_nsec;
    td->tv_sec  = t2.tv_sec - t1.tv_sec;
    if (td->tv_sec > 0 && td->tv_nsec < 0)
    {
        td->tv_nsec += NS_PER_SECOND;
        td->tv_sec--;
    }
    else if (td->tv_sec < 0 && td->tv_nsec > 0)
    {
        td->tv_nsec -= NS_PER_SECOND;
        td->tv_sec++;
    }
}

void worker_thread_start(void *ctx) {
    struct app_thread_ctx *spdk_app_ctx = ctx;
    spdk_app_ctx->spdk_threads[0] = spdk_thread_create("thread0", NULL);
}

void TestBackEnd(
    void *args
) {
    struct app_thread_ctx *spdk_app_ctx = args;

    /* pthread_t worker_thread;
    SPDK_NOTICELOG("pthread_create, worker_thread_start\n");
    pthread_create(&worker_thread, NULL, worker_thread_start, spdk_app_ctx);
    int worker_ret;
    pthread_join(worker_thread, &worker_ret);
    SPDK_NOTICELOG("pthread_join\n"); */

    // starting a new SPDK thread on same pthread will cause msg to not run until after current func return...
    SPDK_NOTICELOG("spdk_thread_create()\n");
    spdk_app_ctx->spdk_threads[0] = spdk_thread_create("thread0", NULL);
    spdk_app_ctx->thread_created = true;

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

    SPDKContext = spdkContext;

    /* struct spdk_thread_work_ctx *work_ctx = malloc(sizeof(*work_ctx));
    work_ctx->count = 0;
    struct timespec start, end, delta;
    printf("starting thread send msg...\n");
    clock_gettime(CLOCK_REALTIME, &start);
    int rc;
    for (int i = 0; i < ITER; i++) {
        rc = spdk_thread_send_msg(spdk_app_ctx->spdk_threads[0], thread_work_func, work_ctx);
        if (rc) {
            printf("rc: %d\n", rc);
            break;
        }
    }
    clock_gettime(CLOCK_REALTIME, &end);
    sub_timespec(start, end, &delta);
    printf("sendmsg called %d times, elapsed: %d.%.9ld\n", ITER, (int)delta.tv_sec, delta.tv_nsec);
    printf("TestBackEnd: work_ctx->count = %ld\n", work_ctx->count);

    SPDK_NOTICELOG("stopping...\n");
    spdk_bdev_close(SPDKContext->bdev_desc);
    spdk_put_io_channel(SPDKContext->bdev_io_channel);
    spdk_thread_exit(spdk_get_thread());
    spdk_app_stop(0); */
    
    // SPDK_NOTICELOG("SPDK MAIN ended but not stopped, also has spdk threads spawned...\n");
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
    // opts.msg_mempool_size = 255;

    struct spdk_thread *spdk_app_threads[3];
    struct app_thread_ctx spdk_app_ctx = { .spdk_threads = spdk_app_threads, .thread_created = false };
    
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

    printf("creating app thread...\n");
    pthread_create(&app_thread, NULL, app_start_wrapper_func, &app_start_ctx);
    // rc = spdk_app_start(&opts, TestBackEnd, NULL);  // block until `spdk_app_stop` is called
    // SPDK_NOTICELOG("spdk_app_start returned with: %d\n", rc);

    struct spdk_thread_work_ctx *work_ctx = malloc(sizeof(*work_ctx));
    work_ctx->count = 0;
    struct timespec start, end, delta;
    while (!spdk_app_ctx.thread_created) {
        printf("WAITING FOR SPDK THREAD CREATE... SLEEP 1\n");
        sleep(1);
    }
    printf("starting thread send msg...\n");
    clock_gettime(CLOCK_REALTIME, &start);
    int i;
    for (i = 0; i < ITER; i++) {
        rc = spdk_thread_send_msg(spdk_app_ctx.spdk_threads[0], thread_work_func, work_ctx);
        if (rc) {
            // printf("rc: %d\n", rc);
            i--;
            // break;
        }
    }
    clock_gettime(CLOCK_REALTIME, &end);
    sub_timespec(start, end, &delta);
    printf("main: work_ctx->count = %ld\n", work_ctx->count);
    printf("sendmsg should have been called %d times, actually: %d, elapsed: %d.%.9ld\n", ITER, i, (int)delta.tv_sec, delta.tv_nsec);
    printf("main: work_ctx->count = %ld\n", work_ctx->count);

    while (work_ctx->count < i) {
        SPDK_NOTICELOG("main waiting for worker thread to finish, work_ctx->count: %ld\n", work_ctx->count);
        sleep(1);
    }

    SPDK_NOTICELOG("stopping..., work_ctx->count = %ld\n", work_ctx->count);
    printf("spdk_bdev_close...\n");
    spdk_bdev_close(SPDKContext->bdev_desc);
    printf("spdk_put_io_channel...\n");
    spdk_put_io_channel(SPDKContext->bdev_io_channel);
    printf("spdk_thread_exit...\n");
    spdk_thread_exit(spdk_get_thread());
    printf("spdk_app_stop...\n");
    spdk_app_stop(0);

    printf("joining app thread...\n");
    int *app_ret;
    int join_ret = pthread_join(app_thread, app_ret);
    printf("join returned: %d, app_ret: %d\n", join_ret, app_ret);

    /* while (1) {
        SPDK_NOTICELOG("main thread looping...\n");
        sleep(1);
    } */


    spdk_app_fini();
}