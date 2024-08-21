#include <stdlib.h>

#include "FileService.h"
#include "Zmalloc.h"
#include "CacheTable.h"

// #undef DEBUG_FILE_SERVICE
#define DEBUG_FILE_SERVICE
#ifdef DEBUG_FILE_SERVICE
#include <stdio.h>
#define DebugPrint(Fmt, ...) fprintf(stderr, Fmt, ##__VA_ARGS__)
#else
static inline void DebugPrint(const char* Fmt, ...) { }
#endif

#define SPDK_LOG_LEVEL SPDK_LOG_NOTICE

//
// The global cache table
//
//
CacheTableT* CacheTable;

struct DPUStorage *Sto;
char *G_BDEV_NAME = "malloc_delay";
FileService* FS;
extern bool G_INITIALIZATION_DONE;
extern volatile int ForceQuitStorageEngine;
int WorkerId = 0;

//
// Usage function for printing parameters that are specific to this application, needed for SPDK app start
//
//
static void
DDSCustomArgsUsage(void) {
    printf("if there is any custom cmdline params, add a description for it here\n");
}

//
// This function is called to parse the parameters that are specific to this application
//
//
static int
DDSParseArg(
    int ch,
    char *arg
) {
    printf("parse custom arg func called, currently doing nothing...\n");
    return 0;
}

//
// Allocate the file service object
//
//
FileService*
AllocateFileService() {
    DebugPrint("Allocating the file service object...\n");
    FS = (FileService*)malloc(sizeof(FileService));
    return FS;
}


void GetIOChannel(void* Ctx) {
    SPDKContextT* ctx = (SPDKContextT*)Ctx;
    SPDK_NOTICELOG("Initializing per thread IO Channel, thread %lu...\n", spdk_thread_get_id(spdk_get_thread()));
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
        SPDK_NOTICELOG("worker thread no. %lu, has poller: %d\n", i, spdk_thread_has_pollers(worker));
        SPDK_NOTICELOG("thread is idle: %d\n", spdk_thread_is_idle(worker));
        int result = spdk_thread_send_msg(worker, GetIOChannel, ctx);
        if (result) {
            SPDK_ERRLOG("failed for i %lu, result: %s\n", i, spdk_strerror(-result));
        }
        else {
            SPDK_NOTICELOG("sent GetIOChannel() to thread no. %lu, ptr: %p\n", i, worker);
        }
    }
}


//
// This is the func supplied to `spdk_app_start()`, will do the actual init work asynchronously
//
//
void StartSPDKFileService(void* Ctx) {
    //
    // Initialize worker threads and their SPDKContexts
    //
    //
    struct StartFileServiceCtx* StartCtx = (struct StartFileServiceCtx*)Ctx;
    FileService* FS = StartCtx->FS;
    FS->WorkerThreadCount = WORKER_THREAD_COUNT;
    FS->WorkerThreads = calloc(FS->WorkerThreadCount, sizeof(struct spdk_thread*));
    memset(FS->WorkerThreads, 0, FS->WorkerThreadCount * sizeof(struct spdk_thread*));
    FS->WorkerSPDKContexts = calloc(FS->WorkerThreadCount, sizeof(SPDKContextT));

    int ret;
    struct spdk_bdev_desc *bdev_desc;
    ret = spdk_bdev_open_ext(G_BDEV_NAME, true, SpdkBdevEventCb, NULL,
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

    SPDK_WARNLOG("bdev buf align = %lu\n", spdk_bdev_get_buf_align(bdev));

    SPDK_NOTICELOG("AllocateSpace()...\n");
    AllocateSpace(FS->MasterSPDKContext);  // allocated SPDKSpace will be copied/shared to all worker contexts


    struct spdk_cpuset tmp_cpumask = {};
    for (int i = 0; i < FS->WorkerThreadCount; i++) {
        SPDK_NOTICELOG("creating worker thread %d\n", i);
        char threadName[32];
        snprintf(threadName, 32, "worker_thread%d", i);
        spdk_cpuset_set_cpu(&tmp_cpumask, spdk_env_get_current_core(), true);
        FS->WorkerThreads[i] = spdk_thread_create(threadName, &tmp_cpumask);
        if (FS->WorkerThreads[i] == NULL) {
            DebugPrint("CANNOT CREATE WORKER THREAD!!! FATAL, EXITING...\n");
            exit(-1);
        }
        SPDK_NOTICELOG("thread id: %lu, thread ptr: %p\n", spdk_thread_get_id(FS->WorkerThreads[i]), FS->WorkerThreads[i]);
        
        SPDKContextT *SPDKContext = &FS->WorkerSPDKContexts[i];
        memcpy(SPDKContext, FS->MasterSPDKContext, sizeof(*SPDKContext));
    }
    FS->MasterSPDKContext->bdev_io_channel = spdk_bdev_get_io_channel(FS->MasterSPDKContext->bdev_desc);
    
    if (FS->MasterSPDKContext->bdev_io_channel == NULL) {
        DebugPrint("master context can't get IO channel!!! Exiting...\n");
        exit(-1);
    }
    FS->AppThread = spdk_get_thread();
    SPDK_NOTICELOG("FS->AppThread id: %lu\n", spdk_thread_get_id(FS->AppThread));
    SPDK_NOTICELOG("FS->AppThread : %lu\n", spdk_thread_get_id(FS->AppThread));
    SPDK_NOTICELOG("master thread has poller: %d\n", spdk_thread_has_pollers(FS->AppThread));
    SPDK_NOTICELOG("master thread is idle: %d\n", spdk_thread_is_idle(FS->AppThread));

    SPDK_NOTICELOG("Calling InitializeWorkerThreadIOChannel()...\n");
    InitializeWorkerThreadIOChannel(FS);

    //
    // Initialize Storage async, we are now using a global DPUStorage *, potentially cross thread
    //
    //
    Sto = BackEndStorage();

    ErrorCodeT result = Initialize(Sto, FS->MasterSPDKContext);
    if (result != DDS_ERROR_CODE_SUCCESS){
        fprintf(stderr, "InitStorage failed with %d\n", result);
        
        //
        // FS app stop
        //
        //
        exit(-1);
        return;
    }

    //
    // `Initialize` will be completed async, returning here doesn't mean it's actually started, but assume it's ok.
    //
    //
    DebugPrint("File service started\n");
}

//
// On the top level, this will call spdk_app_start(), supplying the func `StartSPDKFileService`
// which will initialize threads and storage
//
//
void *
StartFileServiceWrapper(
    void *Ctx
) {
    printf("StartFileServiceWrapper() running...\n");
    struct StartFileServiceCtx *StartCtx = Ctx;

    int argc = StartCtx->Argc;
    char **argv = StartCtx->Argv;

    int rc;

    spdk_log_set_level(SPDK_LOG_LEVEL);

    struct spdk_app_opts opts = {};
    /* Set default values in opts structure. */
    spdk_app_opts_init(&opts, sizeof(opts));
    opts.name = "dds_bdev";
    
    //
    // Parse built-in SPDK command line parameters as well
    // as our custom one(s).
    //
    //
    if (
        (rc = spdk_app_parse_args(argc, argv, &opts, "b:", NULL, DDSParseArg, DDSCustomArgsUsage))
        != SPDK_APP_PARSE_ARGS_SUCCESS
    ) {
        printf("spdk_app_parse_args() failed with: %d\n", rc);
        exit(rc);
    }

    printf("starting FS, StartSPDKFileService()...\n");
    spdk_app_start(&opts, StartSPDKFileService, StartCtx);
    printf("spdk_app_start returns\n");
    return NULL;
}

//
// Start the file service (called by main thread), this will then call pthread_create.
//
//
void
StartFileService(
    int Argc,
    char **Argv,
    FileService *FS
) {
    G_INITIALIZATION_DONE = false;
    struct StartFileServiceCtx *StartCtx = malloc(sizeof(*StartCtx));
    StartCtx->Argc = Argc;
    StartCtx->Argv = Argv;
    StartCtx->FS = FS;
    
    StartFileServiceWrapper(StartCtx);
}

//
// Call this on app thread
//
//
void AppThreadExit(void *Ctx) {
    FileService *FS = Ctx;
    spdk_put_io_channel(FS->MasterSPDKContext->bdev_io_channel);
    spdk_bdev_close(FS->MasterSPDKContext->bdev_desc);
    
    //
    // will get "spdk_app_stop() called twice" if we call it here (after Ctrl-C)
    //
    //
    SPDK_NOTICELOG("App thread exited\n");
}

//
// Call on each of all worker threads
//
//
void WorkerThreadExit(void *Ctx) {
    struct WorkerThreadExitCtx *ExitCtx = Ctx;
    spdk_put_io_channel(ExitCtx->Channel);
    spdk_thread_exit(spdk_get_thread());
    SPDK_NOTICELOG("Worker thread %d exited!\n", ExitCtx->i);
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
    printf("Worker thread count = %d\n", FS->WorkerThreadCount);
    for (size_t i = 0; i < FS->WorkerThreadCount; i++) {
        struct WorkerThreadExitCtx *ExitCtx = malloc(sizeof(*ExitCtx));
        ExitCtx->i = i;
        ExitCtx->Channel = FS->WorkerSPDKContexts[i].bdev_io_channel;
        spdk_thread_send_msg(FS->WorkerThreads[i], WorkerThreadExit, ExitCtx);
    }

    spdk_thread_send_msg(FS->AppThread, AppThreadExit, FS);

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
    //
    // We only need a single worker thread, though this leaves room for multi-threading
    //
    //
    struct spdk_thread *worker = FS->WorkerThreads[WorkerId];
    Context->SPDKContext = &FS->WorkerSPDKContexts[WorkerId];

    int ret = spdk_thread_send_msg(worker, ControlPlaneHandler, Context);

    if (ret) {
        fprintf(stderr, "SubmitControlPlaneRequest() initial thread send msg failed with %d, retrying\n", ret);
        
        //
        // busy retry, should be rare if it happens at all (at extremely high throughput)
        //
        //
        while (1) {
            ret = spdk_thread_send_msg(worker, ControlPlaneHandler, Context);
            if (ret == 0) {
                fprintf(stderr, "SubmitControlPlaneRequest() retry finished\n");
                return;
            }
        }
    }
}

#ifdef OPT_FILE_SERVICE_BATCHING
void
SubmitDataPlaneRequest(
    FileService* FS,
    DataPlaneRequestContext* ContextArray,
    RequestIdT Index,
    RequestIdT BatchSize,
    RequestIdT IoSlotBase
) {
    //
    // We don't need multi threading in DDS, but do proper thread selection if doing multi threading
    //
    //
    struct spdk_thread *worker = FS->WorkerThreads[WorkerId];
    
    //
    // Get the head of batch slot
    //
    //
    SPDKContextT *SPDKContext = &FS->WorkerSPDKContexts[WorkerId];
    struct PerSlotContext *headSlotContext = GetFreeSpace(SPDKContext, &ContextArray[Index - IoSlotBase], Index);
    headSlotContext->BatchSize = BatchSize;
    headSlotContext->IndexBase = IoSlotBase;
    headSlotContext->SPDKContext = SPDKContext;
    
    //
    // Put the rest into each following slot context, handles wrap around indices
    //
    //
    for (RequestIdT i = 1; i < BatchSize; i++) {
        RequestIdT curIndex = (Index - IoSlotBase + i) % DDS_MAX_OUTSTANDING_IO;
        RequestIdT selfIndex = IoSlotBase + curIndex;
        struct PerSlotContext *curSlotContext = &headSlotContext->SPDKContext->SPDKSpace[selfIndex];
        curSlotContext->Ctx = &ContextArray[curIndex];
        curSlotContext->SPDKContext = SPDKContext;
        curSlotContext->BatchSize = BatchSize;
        curSlotContext->IndexBase = IoSlotBase;
    }
    
    //
    // Send the head of batch RequestHandler, which will call ReadHandler and WriteHandler accordingly
    //
    //
    int ret = spdk_thread_send_msg(worker, DataPlaneRequestHandler, headSlotContext);
    if (ret) {
        fprintf(stderr, "SubmitDataPlaneRequest() initial thread send msg failed with %d, retrying\n", ret);
        while (1) {
            ret = spdk_thread_send_msg(worker, DataPlaneRequestHandler, headSlotContext);
            if (ret == 0) {
                fprintf(stderr, "SubmitDataPlaneRequest() read retry finished\n");
                return;
            }
        }
    }
}
#else
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
    struct spdk_thread *worker = FS->WorkerThreads[WorkerId];

    struct PerSlotContext *SlotContext = GetFreeSpace(&FS->WorkerSPDKContexts[WorkerId], Context, Index);
    SlotContext->SPDKContext = &FS->WorkerSPDKContexts[WorkerId];

    int ret;
    if (IsRead) {
        ret = spdk_thread_send_msg(worker, ReadHandler, SlotContext);
        
        if (ret) {
            fprintf(stderr, "SubmitDataPlaneRequest() initial thread send msg failed with %d, retrying\n", ret);
            while (1) {
                ret = spdk_thread_send_msg(worker, ReadHandler, SlotContext);
                if (ret == 0) {
                    fprintf(stderr, "SubmitDataPlaneRequest() read retry finished\n");
                    return;
                }
            }
        }
    }
    else {
        ret = spdk_thread_send_msg(worker, WriteHandler, SlotContext);

        if (ret) {
            fprintf(stderr, "SubmitDataPlaneRequest() initial thread send msg failed with %d, retrying\n", ret);
            while (1) {
                ret = spdk_thread_send_msg(worker, WriteHandler, SlotContext);
                if (ret == 0) {
                    fprintf(stderr, "SubmitDataPlaneRequest() write retry finished\n");
                    return;
                }
            }
        }
    }
}
#endif