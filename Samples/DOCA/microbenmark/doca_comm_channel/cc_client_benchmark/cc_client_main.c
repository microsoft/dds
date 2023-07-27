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

#include <doca_argp.h>
#include <doca_log.h>
#include <doca_dev.h>

#include "utils.h"

#include "cc_client.h"

DOCA_LOG_REGISTER(CC_CLIENT::MAIN);

/* Sample's Logic */
int create_comm_channel_client(const char *server_name, struct doca_pci_bdf *dev_pci_addr, const char *text, int msg_size, int test_rounds);

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
	struct cc_config cfg = {0};
	const char *server_name = "cc_sample_server";
	doca_error_t result;
	struct doca_pci_bdf dev_pcie = {0};

	strcpy(cfg.cc_dev_pci_addr, "3b:00.0");
	strcpy(cfg.text, "Message from the client");
	cfg.msg_size = 8;
	cfg.test_rounds = 1000;

	/* Parse cmdline/json arguments */
	result = doca_argp_init("cc_client", &cfg);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to init ARGP resources: %s", doca_get_error_string(result));
		return EXIT_FAILURE;
	}

	result = register_cc_params();
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to register Comm Channel client sample parameters: %s",
			     doca_get_error_string(result));
		return EXIT_FAILURE;
	}

	result = doca_argp_start(argc, argv);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to parse sample input: %s", doca_get_error_string(result));
		return EXIT_FAILURE;
	}

	/* Convert the PCI address into the matching struct */
	result = parse_pci_addr(cfg.cc_dev_pci_addr, &dev_pcie);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to parse the device PCI address: %s", doca_get_error_string(result));
		doca_argp_destroy();
		return EXIT_FAILURE;
	}

	/* Start the client */
	result = create_comm_channel_client(server_name, &dev_pcie, cfg.text, cfg.msg_size, cfg.test_rounds);
	if (result != DOCA_SUCCESS) {
		doca_argp_destroy();
		return EXIT_FAILURE;
	}

	/* ARGP cleanup */
	doca_argp_destroy();

	return EXIT_SUCCESS;
}
