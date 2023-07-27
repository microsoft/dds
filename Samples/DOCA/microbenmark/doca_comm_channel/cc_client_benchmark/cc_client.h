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

#define MAX_TXT_SIZE 4096					/* Maximum size of input text */
#define PCI_ADDR_LEN 8						/* PCI address string length */

struct cc_config {
	char cc_dev_pci_addr[PCI_ADDR_LEN];			/* Comm Channel DOCA device PCI address */
	char cc_dev_rep_pci_addr[PCI_ADDR_LEN];			/* Comm Channel DOCA device representor PCI address */
	char text[MAX_TXT_SIZE];				/* Text to send to Comm Channel server */
	int msg_size;
	int test_rounds;
};

/*
 * Register the command line parameters for the DOCA CC samples
 *
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
doca_error_t register_cc_params(void);
