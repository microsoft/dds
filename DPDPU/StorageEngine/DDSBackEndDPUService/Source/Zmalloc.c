#include "../Include/Zmalloc.h"

void AllocateSpace(void *arg){
    SPDKContextT *SPDKContext = arg;
    SPDKContext->buff_size = DDS_FRONTEND_MAX_OUTSTANDING * DDS_BACKEND_SPDK_BUFF_BLOCK_SPACE;
	uint32_t buf_align = spdk_bdev_get_buf_align(SPDKContext->bdev);
	SPDKContext->buff = spdk_dma_zmalloc(SPDKContext->buff_size, buf_align, NULL);
	if (!SPDKContext->buff) {
		SPDK_ERRLOG("Failed to allocate buffer\n");
		spdk_put_io_channel(SPDKContext->bdev_io_channel);
		spdk_bdev_close(SPDKContext->bdev_desc);
		spdk_app_stop(-1);
		return;
	}
    SPDKContext->SPDKSpace = malloc(DDS_FRONTEND_MAX_OUTSTANDING * sizeof(int));
    for (int i = 0; i < DDS_FRONTEND_MAX_OUTSTANDING; i++){
        SPDKContext->SPDKSpace[i] = 0;
    }
}

void AllocateSingleSpace(
    void *arg, 
    int Position
){
    SPDKContextT *SPDKContext = arg;
    SPDKContext->SPDKSpace[Position] = 1;
}

void FreeSingleSpace(
    void *arg, 
    int Position
){
    SPDKContextT *SPDKContext = arg;
    memset(SPDKContext->buff[Position * DDS_BACKEND_SPDK_BUFF_BLOCK_SPACE], 0, DDS_BACKEND_SPDK_BUFF_BLOCK_SPACE);
    SPDKContext->SPDKSpace[Position] = 0;

}

void FreeAllSpace(void *arg){
    SPDKContextT *SPDKContext = arg;
    spdk_dma_free(SPDKContext->buff);
    free(SPDKContext->SPDKSpace);
}

int FindFreeSpace(void *arg){
    SPDKContextT *SPDKContext = arg;
    for (int i = 0; i < DDS_FRONTEND_MAX_OUTSTANDING; i++){
        if(SPDKContext->SPDKSpace[i] == 0){
            return i;
        }
    }
    return -1;
}
