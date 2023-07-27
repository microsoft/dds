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

#include <stdlib.h>
#include <string.h>

#include <doca_argp.h>
#include <doca_dev.h>
#include <doca_log.h>

#include <utils.h>

#include "dma_common.h"

DOCA_LOG_REGISTER(DMA_COPY_DPU::MAIN);

/* Sample's Logic */
doca_error_t dma_copy_dpu(const char *export_desc_file_path, const char *buffer_info_file_path,
				 struct doca_pci_bdf *pcie_addr, int msg_size, int test_rounds);

/*
 * Sample main function
 *
 * @argc [in]: command line arguments size
 * @argv [in]: array of command line arguments
 * @return: EXIT_SUCCESS on success and EXIT_FAILURE otherwise
 */
int
main(int argc, char **argv)
{
	struct dma_config dma_conf = {0};
	struct doca_pci_bdf pcie_dev;
	int result;

	strcpy(dma_conf.pci_address, "03:00.0");
	strcpy(dma_conf.export_desc_path, "/tmp/export_desc.txt");
	strcpy(dma_conf.buf_info_path, "/tmp/buffer_info.txt");
	dma_conf.msg_size = 8;
	dma_conf.test_rounds = 1000;

	result = doca_argp_init("dma_copy_dpu", &dma_conf);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to init ARGP resources: %s", doca_get_error_string(result));
		return EXIT_FAILURE;
	}
	result = register_dma_params();
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to register DMA sample parameters: %s", doca_get_error_string(result));
		return EXIT_FAILURE;
	}

	result = doca_argp_start(argc, argv);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to parse sample input: %s", doca_get_error_string(result));
		return EXIT_FAILURE;
	}
#ifndef DOCA_ARCH_DPU
	DOCA_LOG_ERR("Sample can run only on the DPU");
	doca_argp_destroy();
	return EXIT_FAILURE;
#endif
	result = parse_pci_addr(dma_conf.pci_address, &pcie_dev);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to parse pci address: %s", doca_get_error_string(result));
		return EXIT_FAILURE;
	}

	result = dma_copy_dpu(dma_conf.export_desc_path, dma_conf.buf_info_path, &pcie_dev, dma_conf.msg_size, dma_conf.test_rounds);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Sample function has failed: %s", doca_get_error_string(result));
		doca_argp_destroy();
		return EXIT_FAILURE;
	}

	doca_argp_destroy();

	return EXIT_SUCCESS;
}
