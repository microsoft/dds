//// test thread send msg to other thread
// #include "../../DPDPU/StorageEngine/DDSBackEndDPUService/Include/DPUBackEndStorage.h"
// #include "../../DPDPU/StorageEngine/DDSBackEndDPUService/Include/bdev.h"
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

atomic_bool G_WAIT_COND = false;
struct spdk_thread *worker = NULL;
struct spdk_app_opts opts = {};

typedef struct SPDKContext {
	struct spdk_bdev *bdev;
	struct spdk_bdev_desc *bdev_desc;
	struct spdk_io_channel *bdev_io_channel;  // channel would be per thread unique, meaning this whole context should also be
	char *buff;
	unsigned long long buff_size;
	char *bdev_name;
	struct spdk_bdev_io_wait_entry bdev_io_wait;
    void *cookie;  // just in case, a completion cookie that could be anything
    bool *SPDKSpace; // a bool array to record the status of each block memory inside buff. true = free, false = in use
} SPDKContextT;

void dds_bdev_event_cb(enum spdk_bdev_event_type type, struct spdk_bdev *bdev,
		    void *event_ctx)
{
	SPDK_NOTICELOG("Unsupported bdev event: type %d\n", type);
}



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


void worker_func(void *Ctx) {
    SPDK_NOTICELOG("worker doing work...\n");
    sleep(3);
    G_WAIT_COND = true;
    SPDK_NOTICELOG("worker finished work...\n");
}

void worker_print(void *ctx) {
    SPDK_NOTICELOG("worker_print()\n");
}
void create_worker(void **ctx) {
    *ctx = spdk_thread_create("worker", NULL);
    if (*ctx == NULL) {
        printf("CANNOT CREATE THREAD FOR WORKER\n");
    }
    printf("create_worker() finished, id: %llu\n", spdk_thread_get_id(*ctx));
    spdk_thread_send_msg(*ctx, worker_print, NULL);
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

    // struct spdk_thread *worker = NULL;
    pthread_t worker_pthread;
    pthread_create(&worker_pthread, NULL, create_worker, &worker);
    while (worker == NULL) {
        printf("waiting for worker thread to be created......\n");
        sleep(1);
    }
    printf("worker thread has been created\n");

    spdk_thread_send_msg(worker, worker_func, NULL);
    SPDK_NOTICELOG("msg sent to worker\n");

    // TODO: let this func finish, but send a msg to run an inf loop
    /* while (1) {
        if (G_WAIT_COND) {
            SPDK_NOTICELOG("worker finished, master can continue!!\n");
            break;
        }
        SPDK_NOTICELOG("master waiting for worker...\n");
        sleep(1);
    }

    SPDK_NOTICELOG("ALL WORK DONE, STOPPING...\n");
    spdk_bdev_close(spdkContext->bdev_desc);
    spdk_put_io_channel(spdkContext->bdev_io_channel);
    spdk_app_stop(0); */
}

void main_app_start(void *ctx) {
    printf("main_app_start()\n");
    int rc = spdk_app_start(&opts, TestBackEnd, NULL);
}



int main(int argc, char **argv) {
    
   /*  pthread_t worker_pthread;
    printf("worker thread: %p\n", worker);
    pthread_create(&worker_pthread, NULL, create_worker, &worker);
    printf("worker thread after create worker: %p\n", worker); */

    int rc;
    // struct spdk_app_opts opts = {};
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
    
    pthread_t main_thread;
    pthread_create(&main_thread, NULL, main_app_start, NULL);
    void *ret;
    pthread_join(main_thread, NULL);
    // rc = spdk_app_start(&opts, TestBackEnd, NULL);  // block until `spdk_app_stop` is called
    SPDK_NOTICELOG("spdk_app_start returned with: %d\n", rc);
    spdk_app_fini();
}