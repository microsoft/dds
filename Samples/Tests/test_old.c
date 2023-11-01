#include "../../DPDPU/StorageEngine/DDSBackEndDPUService/Include/DPUBackEndStorage.h"
#include "../../DPDPU/StorageEngine/DDSBackEndDPUService/Include/bdev.h"

#define DDS_BACKEND_ADDR "127.0.0.1"
#define DDS_BACKEND_PORT 4242

static void
DDSCustomArgsUsage(void)
{
	printf("if there is any custom cmdline params, add a description for it here\n");
}

//
// This function is called to parse the parameters that are specific to this application
//
//
static int
DDSParseArg(int ch, char *arg)
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
    ErrorCodeT result = Initialize(Sto, spdkContext);
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
    printf("read: %d, result string: %s\n", rc, DstBuf);

    // cleanup before spdk_app_stop(), otherwise it will error out
    printf("cleaning up before app_stop...\n");
    spdk_put_io_channel(spdkContext->bdev_io_channel);
    spdk_bdev_close(spdkContext->bdev_desc);
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
	if ((rc = spdk_app_parse_args(argc, argv, &opts, "b:", NULL, DDSParseArg,
				      DDSCustomArgsUsage)) != SPDK_APP_PARSE_ARGS_SUCCESS) {
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