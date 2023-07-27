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
#include <doca_compress.h>
#include <doca_dev.h>
#include <doca_error.h>
#include <doca_log.h>

#include <utils.h>

DOCA_LOG_REGISTER(COMPRESS_DEFLATE::MAIN);

#define MAX_PCI_ADDRESS_LEN 32			/* max PCI string length */
#define MAX_FILE_NAME 255			/* max user data length */
#define MAX_FILE_SIZE (1024 * 1024 * 1024)	/* compress files up to 1GB */

/* Configuration struct */
struct compress_cfg {
	char file_path[MAX_FILE_NAME];		/* file to compress/decompress */
	int  bytes_to_compress;			/* how many bytes to compress */
	char pci_address[MAX_PCI_ADDRESS_LEN];	/* device PCI address */
};

/* Sample's Logic */
doca_error_t compress_deflate(struct doca_pci_bdf *pci_dev, char *src_buffer, size_t file_size, enum doca_compress_job_types job_type);

/*
 * ARGP Callback - Handle PCI device address parameter
 *
 * @param [in]: Input parameter
 * @config [in/out]: Program configuration context
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
pci_address_callback(void *param, void *config)
{
	struct compress_cfg *compress_cfg = (struct compress_cfg *)config;
	char *pci_address = (char *)param;
	int len;

	len = strnlen(pci_address, MAX_PCI_ADDRESS_LEN);
	if (len == MAX_PCI_ADDRESS_LEN) {
		DOCA_LOG_ERR("Pci address too long, max %d", MAX_PCI_ADDRESS_LEN - 1);
		return DOCA_ERROR_INVALID_VALUE;
	}
	printf("Using device %s...\n", pci_address);
	strcpy(compress_cfg->pci_address, pci_address);
	return DOCA_SUCCESS;
}

/*
 * ARGP Callback - Handle user file parameter
 *
 * @param [in]: Input parameter
 * @config [in/out]: Program configuration context
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
file_callback(void *param, void *config)
{
	struct compress_cfg *compress_cfg = (struct compress_cfg *)config;
	char *file = (char *)param;
	int len;

	len = strnlen(file, MAX_FILE_NAME);
	if (len == MAX_FILE_NAME) {
		DOCA_LOG_ERR("Invalid file name length, max %d", MAX_FILE_NAME - 1);
		return DOCA_ERROR_INVALID_VALUE;
	}
	printf("Compressing %s...\n", file);
	strcpy(compress_cfg->file_path, file);
	return DOCA_SUCCESS;
}

/*
 * ARGP Callback - Handle bytes to compress parameter
 *
 * @param [in]: Input parameter
 * @config [in/out]: Program configuration context
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
bytes_to_compress_callback(void *param, void *config)
{
	struct compress_cfg *compress_cfg = (struct compress_cfg *)config;
	char *bytes_to_compress_str = (char *)param;

	int bytes_to_compress = atoi(bytes_to_compress_str);
	printf("Compressing %d bytes...\n", bytes_to_compress);
	compress_cfg->bytes_to_compress = bytes_to_compress;
	return DOCA_SUCCESS;
}

/*
 * Register the command line parameters for the sample.
 *
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
register_compress_params()
{
	doca_error_t result;
	struct doca_argp_param *pci_param, *file_param, *bytes_to_compress_param;

	result = doca_argp_param_create(&pci_param);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to create ARGP param: %s", doca_get_error_string(result));
		return result;
	}
	doca_argp_param_set_short_name(pci_param, "p");
	doca_argp_param_set_long_name(pci_param, "pci-addr");
	doca_argp_param_set_description(pci_param, "PCI device address");
	doca_argp_param_set_callback(pci_param, pci_address_callback);
	doca_argp_param_set_type(pci_param, DOCA_ARGP_TYPE_STRING);
	result = doca_argp_register_param(pci_param);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to register program param: %s", doca_get_error_string(result));
		return result;
	}

	result = doca_argp_param_create(&file_param);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to create ARGP param: %s", doca_get_error_string(result));
		return result;
	}
	doca_argp_param_set_short_name(file_param, "f");
	doca_argp_param_set_long_name(file_param, "file");
	doca_argp_param_set_description(file_param, "input file to compress/decompress");
	doca_argp_param_set_callback(file_param, file_callback);
	doca_argp_param_set_type(file_param, DOCA_ARGP_TYPE_STRING);
	result = doca_argp_register_param(file_param);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to register program param: %s", doca_get_error_string(result));
		return result;
	}

	result = doca_argp_param_create(&bytes_to_compress_param);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to create ARGP param: %s", doca_get_error_string(result));
		return result;
	}
	doca_argp_param_set_short_name(bytes_to_compress_param, "b");
	doca_argp_param_set_long_name(bytes_to_compress_param, "bytes-to-compress");
	doca_argp_param_set_description(bytes_to_compress_param, "how many bytes to compress");
	doca_argp_param_set_callback(bytes_to_compress_param, bytes_to_compress_callback);
	doca_argp_param_set_type(bytes_to_compress_param, DOCA_ARGP_TYPE_STRING);
	result = doca_argp_register_param(bytes_to_compress_param);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to register program param: %s", doca_get_error_string(result));
		return result;
	}
	return DOCA_SUCCESS;
}

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
	doca_error_t result;
	int exit_status = EXIT_SUCCESS;
	struct compress_cfg compress_cfg = {0};
	struct doca_pci_bdf pcie_dev;
	char *file_data;
	size_t file_size;
	size_t bytes_to_compress;

	strcpy(compress_cfg.pci_address, "03:00.0");
	strcpy(compress_cfg.file_path, "data_to_compress.txt");
	compress_cfg.bytes_to_compress = 1024;

	result = doca_argp_init("compress_deflate", &compress_cfg);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to init ARGP resources: %s", doca_get_error_string(result));
		return EXIT_FAILURE;
	}

	result = register_compress_params();
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to register ARGP params: %s", doca_get_error_string(result));
		doca_argp_destroy();
		return EXIT_FAILURE;
	}

	result = doca_argp_start(argc, argv);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to parse sample input: %s", doca_get_error_string(result));
		doca_argp_destroy();
		return EXIT_FAILURE;
	}

	result = parse_pci_addr(compress_cfg.pci_address, &pcie_dev);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to parse PCI address: %s", doca_get_error_string(result));
		doca_argp_destroy();
		return EXIT_FAILURE;
	}

	result = read_file(compress_cfg.file_path, &file_data, &file_size);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to read file: %s", doca_get_error_string(result));
		doca_argp_destroy();
		return EXIT_FAILURE;
	}
	if (file_size > MAX_FILE_SIZE) {
		DOCA_LOG_ERR("Invalid file size. Should be smaller then %d", MAX_FILE_SIZE);
		free(file_data);
		doca_argp_destroy();
		return EXIT_FAILURE;
	}

	bytes_to_compress = (size_t)compress_cfg.bytes_to_compress;
	if (bytes_to_compress > file_size) {
		DOCA_LOG_ERR("Invalid compression size %ld. Should be smaller than %ld", bytes_to_compress, file_size);
		free(file_data);
		doca_argp_destroy();
		return EXIT_FAILURE;
	}

	printf("Compressing %ld bytes of %s using %s...\n", bytes_to_compress, compress_cfg.file_path, compress_cfg.pci_address);

	result = compress_deflate(&pcie_dev, file_data, file_size, bytes_to_compress);
	if (result != DOCA_SUCCESS)
		exit_status = EXIT_FAILURE;

	free(file_data);
	doca_argp_destroy();
	return exit_status;
}
