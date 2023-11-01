#include "../../DPDPU/StorageEngine/DDSBackEndDPUService/Include/DPUBackEndStorage.h"
#include "../../DPDPU/StorageEngine/DDSBackEndDPUService/Include/bdev.h"

#define DDS_BACKEND_ADDR "127.0.0.1"
#define DDS_BACKEND_PORT 4242

struct spdk_thread *mainthread;
struct spdk_thread *mythread;
struct spdk_thread *waitthread;
struct spdk_thread *appthread;
struct spdk_thread *workthread;

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

struct runFileBackEndArgs
{
    const char* ServerIpStr;
    const int ServerPort;
    const uint32_t MaxClients;
    const uint32_t MaxBuffs;
};


struct mythread_ctx {
    SPDKContextT *SPDKContext;
    SyncRWCompletionStatus *cookie;
};

void readsync_func(void *ctx) {
    printf("readsync_func run on thread: %d\n", spdk_thread_get_id(spdk_get_thread()));
    struct mythread_ctx *myctx = (struct mythread_ctx *) ctx;
    void *tmpbuf = malloc(4096);
    // unsigned short *cb_arg = malloc(sizeof(*cb_arg));
    // *cb_arg = 7890;
    int rc = bdev_read(myctx->SPDKContext, tmpbuf, 0, 1024, ReadFromDiskSyncCallback, &(myctx->cookie), true, 0);
    printf("bdev_read() rc: %d\n", rc);
    // myctx->cookie = 7890;
    printf("readsync_func myctx->cookie: %d\n", myctx->cookie);
}

void app_stop_func(void *ctx) {
    struct mythread_ctx *myctx = ctx;
    printf("spdk_put_io_channel(myctx->SPDKContext->bdev_io_channel)\n");
    spdk_put_io_channel(myctx->SPDKContext->bdev_io_channel);
    spdk_thread_exit(mythread);
    spdk_thread_exit(waitthread);
    spdk_thread_exit(appthread);
    // spdk_thread_destroy(mythread);
    // spdk_thread_destroy(waitthread); XXX: will get a seg fault if called, without it everything's fine...
    printf("appthread stopping app...\n");
    spdk_app_stop(0);
}

void busy_wait_func(void *ctx) {
    struct mythread_ctx *myctx = ctx;
    SyncRWCompletionStatus *cb_arg = &(myctx->cookie);
    printf("running busy wait func, cb_arg ptr: %p, value: %hu\n", cb_arg, *cb_arg);
    while (1) {
        if (*cb_arg == 1) {
            printf("checked: success\n");
            break;
        }
        else if (*cb_arg == 2) {
            printf("checked: fail\n");
            break;
        }
        // XXX: if I don't have either of this, it loops forever, interesting??  Oh shit it should be atomic and then it works
        else {
            // printf("busy_wait_func cb_arg: %hu\n", *cb_arg);
            // usleep(1);
            
        }
    }
    // spdk_app_stop(0);  // can't call this in any thread except main thread?
    spdk_bdev_close(myctx->SPDKContext->bdev_desc);
    spdk_thread_exec_msg(appthread, app_stop_func, myctx);
}

void work_thread_func(void *ctx) {
    struct mythread_ctx *myctx = ctx;
    mythread = spdk_thread_create("mythread", NULL);
    printf("my thread id: %d", spdk_thread_get_id(mythread));
    waitthread = spdk_thread_create("waitthread", NULL);
    printf("busy wait thread id: %d", spdk_thread_get_id(waitthread));
    appthread = spdk_thread_get_app_thread();
    printf("app thread id: %d", spdk_thread_get_id(mythread));

    SPDK_NOTICELOG("work_thread: %d\n", spdk_thread_get_id(spdk_get_thread()));
    
    // myctx->SPDKContext = spdkContext;
    // myctx->cookie = 999;
    printf("myctx->cookie ptr: %p, value: %hu\n", &(myctx->cookie), myctx->cookie);
    spdk_thread_exec_msg(mythread, readsync_func, myctx);

    SyncRWCompletionStatus *cb_arg = &(myctx->cookie);
    //// XXX: THIS BUSY WAIT LOOP will cause it to get stuck (callback) not fired
    while (1) {
        if (*cb_arg == 1) {
            printf("checked: success\n");
            break;
        }
        else if (*cb_arg == 2) {
            printf("checked: fail\n");
            break;
        }
        /* sleep(1);
        printf("slept\n"); */
        /* sched_yield();
        printf("sched yielded\n"); */
    }
    //// IF WE RUN IT IN A DIFFERENT THREAD, it will be okay...
    // spdk_thread_exec_msg(waitthread, busy_wait_func, myctx);
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

    struct DPUStorage* Sto = BackEndStorage();
    /* void *tmpbuf = malloc(4096);
    unsigned short *cb_arg = malloc(sizeof(*cb_arg));
    *cb_arg = 7890;
    int rc = bdev_read(spdkContext, tmpbuf, 0, 1024, ReadFromDiskSyncCallback, cb_arg, true, 0);
    printf("bdev_read() rc: %d\n", rc); */

    mainthread = spdk_get_thread();
    printf("main thread id: %d", spdk_thread_get_id(mainthread));

    /* mythread = spdk_thread_create("mythread", NULL);
    printf("my thread id: %d", spdk_thread_get_id(mythread));
    waitthread = spdk_thread_create("waitthread", NULL);
    printf("busy wait thread id: %d", spdk_thread_get_id(waitthread));
    appthread = spdk_thread_get_app_thread();
    printf("app thread id: %d", spdk_thread_get_id(mythread)); */

    workthread = spdk_thread_create("workthread", NULL);
    
    struct mythread_ctx *myctx = malloc(sizeof(*myctx));
    myctx->SPDKContext = spdkContext;
    myctx->cookie = 999;
    printf("myctx->cookie ptr: %p, value: %hu\n", &(myctx->cookie), myctx->cookie);

    spdk_thread_exec_msg(workthread, work_thread_func, myctx);

    /* spdk_thread_exec_msg(mythread, readsync_func, myctx); */

    // printf("entering while true loop\n");
    SyncRWCompletionStatus *cb_arg = &(myctx->cookie);
    printf("cb_arg ptr: %p, value: %hu\n", cb_arg, *cb_arg);
    // main thread must not busy wait, otherwise callbacks won't even run??
    // XXX: read/write callbacks ARE ACUTALLY RUN ON THE MAIN THREAD!!!
    /* while (1) {
        if (*cb_arg == 1) {
            printf("checked: success\n");
        }
        else if (*cb_arg == 2) {
            printf("checked: fail\n");
        }
    } */
    /* spdk_thread_exec_msg(waitthread, busy_wait_func, myctx); */
    
    /* ErrorCodeT result = Initialize(Sto, spdkContext);
    if (result != DDS_ERROR_CODE_SUCCESS){
        fprintf(stderr, "InitStorage failed with %d\n", result);
        return;
    }
    printf("Sto->AvailableSegments: %d, Sto->TotalSegments: %d\n", Sto->AvailableSegments, Sto->TotalSegments);

    char SrcBuf[DDS_BACKEND_PAGE_SIZE] = DDS_BACKEND_INITIALIZATION_MARK;
    char DstBuf[DDS_BACKEND_PAGE_SIZE];
    printf("starting WriteToDiskSync\n");
    ErrorCodeT rc = WriteToDiskSync(SrcBuf, 0, 8, DDS_BACKEND_PAGE_SIZE, Sto, spdkContext);
    printf("WriteToDiskSync: %d\n", rc);
    rc = ReadFromDiskSync(DstBuf, 0, 8, DDS_BACKEND_PAGE_SIZE, Sto, spdkContext);
    printf("read: %d, result string: %s\n", rc, DstBuf); */

    // cleanup before spdk_app_stop(), otherwise it will error out
    /* printf("cleaning up before app_stop...\n");
    spdk_put_io_channel(spdkContext->bdev_io_channel);
    spdk_bdev_close(spdkContext->bdev_desc);
    spdk_app_stop(0); */
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
    /* int ret = RunFileBackEnd(DDS_BACKEND_ADDR, DDS_BACKEND_PORT, 32, 32); */
    
    struct runFileBackEndArgs args = {
        .ServerIpStr = DDS_BACKEND_ADDR,
        .ServerPort = DDS_BACKEND_PORT,
        .MaxClients = 32,
        .MaxBuffs = 32
    };
    
    rc = spdk_app_start(&opts, TestBackEnd, &args);  // block until `spdk_app_stop` is called
    printf("spdk_app_start returned with: %d\n", rc);
    spdk_app_fini();
}