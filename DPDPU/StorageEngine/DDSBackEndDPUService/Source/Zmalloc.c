#include "../Include/Zmalloc.h"

void AllocateSpace(void *arg){
    SPDKContextT *SPDKContext = arg;
    SPDKContext->buff_size = DDS_MAX_OUTSTANDING_IO * DDS_BACKEND_SPDK_BUFF_BLOCK_SPACE;//DDS_MAX_OUTSTANDING_IO
	uint32_t buf_align = spdk_bdev_get_buf_align(SPDKContext->bdev);
	// SPDKContext->buff = spdk_dma_zmalloc(SPDKContext->buff_size, buf_align, NULL); //TODO: CAN'T spdk malloc
    SPDKContext->buff = malloc(SPDKContext->buff_size);
	if (!SPDKContext->buff) {
		SPDK_ERRLOG("Failed to allocate buffer\n");
		/* spdk_put_io_channel(SPDKContext->bdev_io_channel);
		spdk_bdev_close(SPDKContext->bdev_desc);
		spdk_app_stop(-1); */
        exit(-1);
		return;
	}
    SPDKContext->SPDKSpace = malloc(DDS_MAX_OUTSTANDING_IO * sizeof(struct PerSlotContext));
    for (int i = 0; i < DDS_MAX_OUTSTANDING_IO; i++){
        SPDKContext->SPDKSpace[i].Available = true;
        SPDKContext->SPDKSpace[i].Position = i;
    }
    if(spdk_bdev_is_zoned(SPDKContext->bdev)){
        bdev_reset_zone(arg);
        return;
    }
    pthread_mutex_init(&SPDKContext->SpaceMutex, NULL);

}


void FreeSingleSpace(
    struct PerSlotContext* Ctx
){
    Ctx->Available = true;

}

void FreeAllSpace(void *arg){
    SPDKContextT *SPDKContext = arg;
    spdk_dma_free(SPDKContext->buff);
    free(SPDKContext->SPDKSpace);
}

struct PerSlotContext* FindFreeSpace(
    SPDKContextT *SPDKContext,
    DataPlaneRequestContext* Context
){
    pthread_mutex_lock(&SPDKContext->SpaceMutex);
    for (int i = 0; i < DDS_FRONTEND_MAX_OUTSTANDING; i++){
        if(SPDKContext->SPDKSpace[i].Available){
            SPDKContext->SPDKSpace[i].Available = false;
            SPDKContext->SPDKSpace[i].Ctx = Context;
            pthread_mutex_unlock(&SPDKContext->SpaceMutex);
            return &SPDKContext->SPDKSpace[i];
        }
    }
    pthread_mutex_unlock(&SPDKContext->SpaceMutex);
    return NULL;
}

//
// Puts the Request Context into a slot context, and returns the slot ctx ptr
// Note: should be always called from app thread.
// Due to the fact that we have `DDS_MAX_OUTSTANDING_IO`, the index of IO request can be used as the index
// for per slot context, since they have a 1 to 1 relation, therefore no need for searching and freeing;
//
//
struct PerSlotContext* GetFreeSpace(
    SPDKContextT *SPDKContext,
    DataPlaneRequestContext* Context,
    RequestIdT index
){
    /* for (int i = 0; i < DDS_FRONTEND_MAX_OUTSTANDING; i++){
        if(SPDKContext->SPDKSpace[i].Available){
            SPDKContext->SPDKSpace[i].Available = false;
            SPDKContext->SPDKSpace[i].Ctx = Context;
            return &SPDKContext->SPDKSpace[i];
        }
    }
    return NULL; */
    SPDKContext->SPDKSpace[index].Ctx = Context;
    return &SPDKContext->SPDKSpace[index];
}
