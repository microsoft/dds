/*
 * Copyright (c) 2022 NVIDIA CORPORATION & AFFILIATES, ALL RIGHTS RESERVED.
 *
 * This software product is a proprietary product of NVIDIA CORPORATION &
 * AFFILIATES (the "Company") and all right, title, and interest in and to the
 * software product, including all associated intellectual property rights, are
 * and shall remain exclusively with the Company.
 *
 * This software product is governed by the End User License Agreement
 * provided with the software product.
 *
 */

#ifndef DMA_COMMON_H_
#define DMA_COMMON_H_

#include <unistd.h>

#include <doca_dma.h>
#include <doca_error.h>

#include "common.h"

#define MAX_ARG_SIZE 256   		/* Maximum size of input argument */
#define MAX_TXT_SIZE 1048576  		/* Maximum size of input text */
#define PAGE_SIZE sysconf(_SC_PAGESIZE) /* Page size */
#define WORKQ_DEPTH 32	   		/* Work queue depth */

/* Configuration struct */
struct dma_config {
	char pci_address[MAX_ARG_SIZE];	     /* PCI device address */
	char cpy_txt[MAX_TXT_SIZE];	     /* Text to copy between the two local buffers */
	char export_desc_path[MAX_ARG_SIZE]; /* Path to save/read the exported descriptor file */
	char buf_info_path[MAX_ARG_SIZE];    /* Path to save/read the buffer information file */
};

/*
 * Register the command line parameters for the DOCA DMA samples
 *
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
doca_error_t register_dma_params(void);

/*
 * Initiates all DOCA core structures needed by the Host.
 *
 * @state [in]: Structure containing all DOCA core structures
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
doca_error_t host_init_core_objects(struct program_core_objects *state);

/*
 * Destroys all DOCA core structures
 *
 * @state [in]: Structure containing all DOCA core structures
 */
void host_destroy_core_objects(struct program_core_objects *state);

/*
 * Removes all DOCA core structures
 *
 * @state [in]: Structure containing all DOCA core structures
 * @dma_ctx [in]: DMA context
 */
void dma_cleanup(struct program_core_objects *state, struct doca_dma *dma_ctx);

/**
 * Check if given device is capable of excuting a DOCA_DMA_JOB_MEMCPY.
 *
 * @devinfo [in]: The DOCA device information
 * @return: DOCA_SUCCESS if the device supports DOCA_DMA_JOB_MEMCPY and DOCA_ERROR otherwise.
 */
doca_error_t dma_jobs_is_supported(struct doca_devinfo *devinfo);

#endif
