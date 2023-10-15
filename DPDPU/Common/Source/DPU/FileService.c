#include <stdlib.h>

#include "FileService.h"
#include "Zmalloc.h"

// #undef DEBUG_FILE_SERVICE
#define DEBUG_FILE_SERVICE
#ifdef DEBUG_FILE_SERVICE
#include <stdio.h>
#define DebugPrint(Fmt, ...) fprintf(stderr, Fmt, ##__VA_ARGS__)
#else
static inline void DebugPrint(const char* Fmt, ...) { }
#endif

struct DPUStorage *Sto;
char *G_BDEV_NAME = "malloc_delay";
FileService* FS;
extern bool G_INITIALIZATION_DONE;
uint64_t submit_count = 0;

//
// Allocate the file service object
//
//
FileService*
AllocateFileService() {
    DebugPrint("Allocating the file service object...\n");
    FS = (FileService*)malloc(sizeof(FileService));
    // TODO: anything to do here?
    return FS;
}


void GetIOChannel(SPDKContextT *ctx) {
    SPDK_NOTICELOG("Initializing per thread IO Channel, thread %d...\n", spdk_thread_get_id(spdk_get_thread()));
    ctx->bdev_io_channel = spdk_bdev_get_io_channel(ctx->bdev_desc);
    if (ctx->bdev_io_channel == NULL) {
		SPDK_ERRLOG("Could not create bdev I/O channel!!\n");
		exit(-1);
		return;
	}
}

//
// This is also on app thread, can we wait in here??
//
//
void InitializeWorkerThreadIOChannel(FileService *FS) {
    for (size_t i = 0; i < FS->WorkerThreadCount; i++)
    {
        struct spdk_thread *worker = FS->WorkerThreads[i];
        SPDKContextT *ctx = &FS->WorkerSPDKContexts[i];
        int result = spdk_thread_send_msg(worker, GetIOChannel, ctx);
        if (result) {
            SPDK_ERRLOG("failed for i %d, result: %s\n", i, spdk_strerror(-result));
        }
        else {
            SPDK_NOTICELOG("sent GetIOChannel() to thread no. %d, ptr: %p\n", i, worker);
        }
    }

    // TODO: does this work?? wait for all workers to finish
    /* int i = 0;
    bool allDone = true;
    while (i < 10) {
        SPDK_NOTICELOG("waiting for all worker threads...\n");
        sleep(1);
        for (size_t i = 0; i < FS->WorkerThreadCount; i++) {
            if (FS->WorkerSPDKContexts[i].bdev_io_channel == NULL) {
                allDone = false;
                SPDK_NOTICELOG("thread %d hasn't yet got io channel!\n", i);
                break;
            }
        }
        if (allDone) {
            SPDK_NOTICELOG("all threads got io channel!\n");
            break;
        }
        i++;
    }

    if (!allDone) {
        // TODO: FS app stop
        SPDK_ERRLOG("waiting for all worker threads timed out, EXITING...\n");
        exit(-1);
    } */
}


void writev_cb(
    struct spdk_bdev_io *bdev_io,
    bool Success,
    struct iovec *dummy_iov) {
    SPDK_NOTICELOG("writev_cb ran\n");
    SPDK_NOTICELOG("len1: %u, %s\n", dummy_iov[0].iov_len, dummy_iov[0].iov_base);
}
//
// This is the func supplied to `spdk_app_start()`, will do the actual init work asynchronously
//
//
void StartSPDKFileService(struct StartFileServiceCtx *StartCtx) {
    //
    // Initialize worker threads and their SPDKContexts
    //
    //
    FileService *FS = StartCtx->FS;
    FS->WorkerThreadCount = WORKER_THREAD_COUNT;
    FS->WorkerThreads = calloc(FS->WorkerThreadCount, sizeof(struct spdk_thread *));
    FS->WorkerSPDKContexts = calloc(FS->WorkerThreadCount, sizeof(SPDKContextT));

    int ret;
    struct spdk_bdev_desc *bdev_desc;
    ret = spdk_bdev_open_ext(G_BDEV_NAME, true, dds_bdev_event_cb, NULL,
            &bdev_desc);
    if (ret) {
        DebugPrint("spdk_bdev_open_ext FAILED!!! FATAL, EXITING...\n");
        exit(-1);
    }
    struct spdk_bdev *bdev = spdk_bdev_desc_get_bdev(bdev_desc);

    FS->MasterSPDKContext = malloc(sizeof(*FS->MasterSPDKContext));
    FS->MasterSPDKContext->bdev_io_channel = NULL;
    FS->MasterSPDKContext->bdev_name = G_BDEV_NAME;
    FS->MasterSPDKContext->bdev_desc = bdev_desc;
    FS->MasterSPDKContext->bdev = bdev;

    SPDK_WARNLOG("bdev buf align = %d\n", spdk_bdev_get_buf_align(bdev));

    SPDK_NOTICELOG("AllocateSpace() for Zmalloc...\n");
    AllocateSpace(FS->MasterSPDKContext);  // allocated SPDKSpace will be copied/shared to all worker contexts


    for (int i = 0; i < FS->WorkerThreadCount; i++)
    {
        SPDK_NOTICELOG("creating worker thread %d\n", i);
        char threadName[32];
        snprintf(threadName, 16, "worker_thread%d", i);
        FS->WorkerThreads[i] = spdk_thread_create(threadName, NULL);
        if (FS->WorkerThreads[i] == NULL) {
            DebugPrint("CANNOT CREATE WORKER THREAD!!! FATAL, EXITING...\n");
            exit(-1);  // TODO: FS app stop
        }
        SPDK_NOTICELOG("thread id: %llu, thread ptr: %p\n", spdk_thread_get_id(FS->WorkerThreads[i]), FS->WorkerThreads[i]);
        
        SPDKContextT *SPDKContext = &FS->WorkerSPDKContexts[i];
        memcpy(SPDKContext, FS->MasterSPDKContext, sizeof(*SPDKContext));
    }
    FS->MasterSPDKContext->bdev_io_channel = spdk_bdev_get_io_channel(FS->MasterSPDKContext->bdev_desc);
    
    if (FS->MasterSPDKContext->bdev_io_channel == NULL) {
        DebugPrint("master context can't get IO channel!!! Exiting...\n");
        exit(-1);
    }
    FS->AppThread = spdk_get_thread();
    SPDK_NOTICELOG("FS->AppThread id: %llu, is app thread: %d\n",
        spdk_thread_get_id(FS->AppThread), spdk_thread_is_app_thread(FS->AppThread));

    SPDK_NOTICELOG("Calling InitializeWorkerThreadIOChannel()...\n");
    InitializeWorkerThreadIOChannel(FS);

    /* // readv writev work?
    sleep(2);
    struct iovec *dummy_iov = malloc(2*sizeof(struct iovec));
    dummy_iov[0].iov_base = malloc(4096);
    dummy_iov[0].iov_len = 4096;
    dummy_iov[1].iov_base = malloc(4096);
    dummy_iov[1].iov_len = 4096;
    ret = spdk_bdev_readv(FS->MasterSPDKContext->bdev_desc, FS->MasterSPDKContext->bdev_io_channel, &dummy_iov, 2, 0, 8192, writev_cb, dummy_iov);
    if (ret) {
        SPDK_ERRLOG("writev failed: %d\n", ret);
    }
    else {
        SPDK_ERRLOG("writev success\n");
    } */
    
    //
    // Initialize Storage async, we are now using a global DPUStorage *, potentially cross thread
    //
    //
    Sto = BackEndStorage();

    ErrorCodeT result = Initialize(Sto, FS->MasterSPDKContext);
    if (result != DDS_ERROR_CODE_SUCCESS){
        fprintf(stderr, "InitStorage failed with %d\n", result);
        // FS app stop
        exit(-1);
        return;
    }

    // `Initialize` will be completed async, returning here doesn't mean it's actually started, but assume it's ok.

    DebugPrint("File service started\n");
}

//
// On the top level, this will call spdk_app_start(), supplying the func `StartSPDKFileService`
// which will initialize threads and storage
//
void *
StartFileServiceWrapper(
    void *Ctx
) {
    printf("StartFileServiceWrapper() running...\n");
    struct StartFileServiceCtx *StartCtx = Ctx;

    FileService *FS = StartCtx->FS;
    int argc = StartCtx->argc;
    char **argv = StartCtx->argv;

    int rc;
    struct spdk_app_opts opts = {};
    /* Set default values in opts structure. */
	spdk_app_opts_init(&opts, sizeof(opts));
	opts.name = "dds_bdev";
    /*
	 * Parse built-in SPDK command line parameters as well
	 * as our custom one(s).
	 */
	if ((rc = spdk_app_parse_args(argc, argv, &opts, "b:", NULL, dds_parse_arg,
				      dds_custom_args_usage)) != SPDK_APP_PARSE_ARGS_SUCCESS) {
        printf("spdk_app_parse_args() failed with: %d\n", rc);
		exit(rc);
	}

    printf("starting FS, StartSPDKFileService()...\n");
    spdk_app_start(&opts, StartSPDKFileService, StartCtx);
}

//
// Start the file service (called by main thread), this will then call pthread_create.
//
//
void
StartFileService(
    int argc,
    char **argv,
    FileService *FS,
    pthread_t *AppPthread
) {
    G_INITIALIZATION_DONE = false;
    struct StartFileServiceCtx *StartCtx = malloc(sizeof(*StartCtx));
    StartCtx->argc = argc;
    StartCtx->argv = argv;
    StartCtx->FS = FS;
    printf("calling pthread_create()\n");
    pthread_create(AppPthread, NULL, StartFileServiceWrapper, StartCtx);
}

//
// Call this on app thread
//
//
void AppThreadExit(void *Ctx) {
    FileService *FS = Ctx;
    spdk_put_io_channel(FS->MasterSPDKContext->bdev_io_channel);
    spdk_bdev_close(FS->MasterSPDKContext->bdev_desc);
    SPDK_NOTICELOG("calling spdk_app_stop(0);...\n");
    spdk_app_stop(0);
}

//
// Call on each of all worker threads
//
//
void WorkerThreadExit(void *Ctx) {
    struct WorkerThreadExitCtx *ExitCtx = Ctx;
    spdk_put_io_channel(ExitCtx->Channel);
    spdk_thread_exit(spdk_get_thread());
    SPDK_NOTICELOG("Worker Thread %d exited!\n", ExitCtx->i);
    free(Ctx);
}

//
// Stop the file service, called on agent thread; this will eventually make app thread call spdk_app_stop()
//
//
void
StopFileService(
    FileService* FS
) {
    //
    // clean up and free contexts, then worker threads exit
    //
    //

    for (size_t i = 0; i < FS->WorkerThreadCount; i++) {
        struct spdk_thread *worker = FS->WorkerThreads[i];
        SPDKContextT ctx = FS->WorkerSPDKContexts[i];
        struct WorkerThreadExitCtx *ExitCtx = malloc(sizeof(*ExitCtx));
        ExitCtx->i = i;
        ExitCtx->Channel = FS->WorkerSPDKContexts[i].bdev_io_channel;
        spdk_thread_send_msg(FS->WorkerThreads[i], WorkerThreadExit, ExitCtx);
    }

    spdk_thread_send_msg(FS->AppThread, AppThreadExit, FS);
    // TODO: maybe do sth more, maybe thread exit for app thread?
    DebugPrint("File service stopped\n");
}

//
// Stop, then Deallocate the file service object, called on agent thread
//
//
void
DeallocateFileService(
    FileService* FS
) {
    DebugPrint("File service stopping...\n");
    // StopFileService(FS);
    free(FS);
    DebugPrint("File service object deallocated\n");
}

//
// Send a control plane request to the file service, called from app thread
//
//
void
SubmitControlPlaneRequest(
    FileService* FS,
    ControlPlaneRequestContext* Context
) {
    SPDK_NOTICELOG("Submitting a control plane request, id: %d, req: %p, resp: %p\n",
        Context->RequestId, Context->Request, Context->Response);

    // TODO: thread selection?
    struct spdk_thread *worker = FS->WorkerThreads[1];
    Context->SPDKContext = &FS->WorkerSPDKContexts[1];

    int ret = spdk_thread_send_msg(worker, ControlPlaneHandler, Context);

    if (ret) {
        fprintf(stderr, "SubmitControlPlaneRequest() initial thread send msg failed with %d, retrying\n", ret);
        // TODO: maybe limited retries?
        while (1) {
            ret = spdk_thread_send_msg(worker, ControlPlaneHandler, Context);
            if (ret == 0) {
                fprintf(stderr, "SubmitControlPlaneRequest() retry finished\n", ret);
                return;
            }
        }
    }
}

//
// Send a data plane request to the file service, called from app thread
//
//
void
SubmitDataPlaneRequest(
    FileService* FS,
    DataPlaneRequestContext* Context,
    bool IsRead,
    RequestIdT Index
) {
    submit_count += 1;
    
    /* if (submit_count % 1 == 0) {
        SPDK_NOTICELOG("Submitting a data plane request, count: %llu\n", submit_count);
    } */
    
    
    
    // TODO: do proper thread selection
    struct spdk_thread *worker = FS->WorkerThreads[0];
    // Context->SPDKContext = FS->WorkerSPDKContexts[0];

    struct PerSlotContext *SlotContext = GetFreeSpace(&FS->WorkerSPDKContexts[0], Context, Index); // TODO: no need for thread spdk ctx?
    SlotContext->SPDKContext = &FS->WorkerSPDKContexts[0];
    

    int ret;  // TODO: limited retries?
    if (IsRead) {
        ret = spdk_thread_send_msg(worker, ReadHandler, SlotContext);
        
        if (ret) {
            fprintf(stderr, "SubmitDataPlaneRequest() initial thread send msg failed with %d, retrying\n", ret);
            // exit(-1);
            while (1) {
                ret = spdk_thread_send_msg(worker, ReadHandler, SlotContext);
                if (ret == 0) {
                    fprintf(stderr, "SubmitDataPlaneRequest() read retry finished\n", ret);
                    return;
                }
            }
        }
    }
    else {
        ret = spdk_thread_send_msg(worker, WriteHandler, SlotContext);

        if (ret) {
            fprintf(stderr, "SubmitDataPlaneRequest() initial thread send msg failed with %d, retrying\n", ret);
            // exit(-1);
            while (1) {
                ret = spdk_thread_send_msg(worker, WriteHandler, SlotContext);
                if (ret == 0) {
                    fprintf(stderr, "SubmitDataPlaneRequest() write retry finished\n", ret);
                    return;
                }
            }
        }
    }
    
}


//
// Usage function for printing parameters that are specific to this application, needed for SPDK app start
//
//
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