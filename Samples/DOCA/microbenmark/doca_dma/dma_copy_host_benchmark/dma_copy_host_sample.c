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

#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <doca_dma.h>
#include <doca_error.h>
#include <doca_log.h>
#include <doca_mmap.h>

#include "dma_common.h"

DOCA_LOG_REGISTER(DMA_COPY_HOST);

bool is_dma_done_on_dpu;	/* Shared variable to allow for a proper shutdown */

/*
 * Signal handler
 *
 * @signum [in]: Signal number to handle
 */
static void
signal_handler(int signum)
{
	if (signum == SIGINT || signum == SIGTERM) {
		DOCA_LOG_INFO("Signal %d received, preparing to exit...", signum);
		is_dma_done_on_dpu = true;
	}
}

/*
 * Saves export descriptor and buffer information into two separate files
 *
 * @export_desc [in]: Export descriptor to write into a file
 * @export_desc_len [in]: Export descriptor length
 * @src_buffer [in]: Source buffer
 * @src_buffer_len [in]: Source buffer length
 * @export_desc_file_path [in]: Export descriptor file path
 * @buffer_info_file_path [in]: Buffer information file path
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
save_config_info_to_files(char *export_desc, size_t export_desc_len, const char *src_buffer, size_t src_buffer_len,
			  char *export_desc_file_path, char *buffer_info_file_path)
{
	FILE *fp;
	uint64_t buffer_addr = (uintptr_t)src_buffer;
	uint64_t buffer_len = (uint64_t)src_buffer_len;

	fp = fopen(export_desc_file_path, "wb");
	if (fp == NULL) {
		DOCA_LOG_ERR("Failed to create the DMA copy file");
		return DOCA_ERROR_IO_FAILED;
	}

	if (fwrite(export_desc, 1, export_desc_len, fp) != export_desc_len) {
		DOCA_LOG_ERR("Failed to write all data into the file");
		fclose(fp);
		return DOCA_ERROR_IO_FAILED;
	}

	fclose(fp);

	fp = fopen(buffer_info_file_path, "w");
	if (fp == NULL) {
		DOCA_LOG_ERR("Failed to create the DMA copy file");
		return DOCA_ERROR_IO_FAILED;
	}

	fprintf(fp, "%" PRIu64 "\n", buffer_addr);
	fprintf(fp, "%" PRIu64 "", buffer_len);

	fclose(fp);

	return DOCA_SUCCESS;
}

/*
 * Run DOCA DMA Host copy sample
 *
 * @pcie_addr [in]: Device PCI address
 * @src_buffer [in]: Source buffer to copy
 * @src_buffer_size [in]: Buffer size
 * @export_desc_file_path [in]: Export descriptor file path
 * @buffer_info_file_path [in]: Buffer info file path
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
doca_error_t
dma_copy_host(struct doca_pci_bdf *pcie_addr, char *src_buffer, size_t src_buffer_size,
		     char *export_desc_file_path, char *buffer_info_file_name)
{
	struct program_core_objects state = {0};
	doca_error_t result;
	char *export_desc;
	size_t export_desc_len;

	/* Signal the while loop to stop and destroy the memory map */
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	/* Open the relevant DOCA device */
	result = open_doca_device_with_pci(pcie_addr, &dma_jobs_is_supported, &state.dev);
	if (result != DOCA_SUCCESS)
		return result;

	/* Init all DOCA core objects */
	result = host_init_core_objects(&state);
	if (result != DOCA_SUCCESS) {
		host_destroy_core_objects(&state);
		return result;
	}

	/* Populate the memory map with the allocated memory */
	result = doca_mmap_populate(state.mmap, src_buffer, src_buffer_size, PAGE_SIZE, NULL, NULL);
	if (result != DOCA_SUCCESS) {
		host_destroy_core_objects(&state);
		return result;
	}

	/* Export DOCA mmap to enable DMA on Host*/
	result = doca_mmap_export(state.mmap, state.dev, (void **)&export_desc, &export_desc_len);
	if (result != DOCA_SUCCESS) {
		host_destroy_core_objects(&state);
		return result;
	}

	DOCA_LOG_INFO("Please copy %s and %s to the DPU and run DMA Copy DPU sample before closing", export_desc_file_path, buffer_info_file_name);

	/* Saves the export desc and buffer info to files, it is the user responsibility to transfer them to the dpu */
	result = save_config_info_to_files(export_desc, export_desc_len, src_buffer, src_buffer_size,
					   export_desc_file_path, buffer_info_file_name);
	if (result != DOCA_SUCCESS) {
		free(export_desc);
		host_destroy_core_objects(&state);
		return result;
	}

	/* Wait until DMA copy on the DPU is over */
	while (!is_dma_done_on_dpu)
		sleep(1);

	/* Destroy all relevant DOCA core objects */
	host_destroy_core_objects(&state);

	/* Free API pre-allocated exported string */
	free(export_desc);

	return result;
}
