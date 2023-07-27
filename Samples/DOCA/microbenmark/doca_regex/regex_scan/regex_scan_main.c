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

#include <bsd/string.h>
#include <stdlib.h>

#include <doca_argp.h>
#include <doca_log.h>
#include <doca_regex.h>

#include <utils.h>

DOCA_LOG_REGISTER(REGEX_SCAN::MAIN);

#define MAX_FILE_NAME 255			/* Maximal length of file path */
#define MAX_ARG_SIZE 256			/* Maximum size of input argument */

/* Sample's Logic */
int regex_scan(char *data_buffer, size_t data_buffer_len, struct doca_pci_bdf *pci_addr, char *rules_buffer,
	size_t rules_buffer_len);

/* Configuration struct */
struct regex_config {
	char *rules_buffer;			/* Buffer holds the RegEx rules */
	size_t rules_buffer_len;		/* Rules buffer size */
	char pci_address[MAX_ARG_SIZE];		/* RegEx PCI address to use */
	char data[MAX_FILE_NAME];		/* Data to scan file path */
};

/*
 * ARGP Callback - Handle RegEx PCI address parameter
 *
 * @param [in]: Input parameter
 * @config [in/out]: Program configuration context
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
pci_address_callback(void *param, void *config)
{
	struct regex_config *rgx_cfg = (struct regex_config *)config;
	char *pci_address = (char *)param;
	int len;

	len = strnlen(pci_address, MAX_ARG_SIZE);
	if (len == MAX_ARG_SIZE) {
		DOCA_LOG_ERR("PCI address is too long max %d", MAX_ARG_SIZE - 1);
		return DOCA_ERROR_INVALID_VALUE;
	}
	strlcpy(rgx_cfg->pci_address, pci_address, MAX_ARG_SIZE);
	return DOCA_SUCCESS;
}

/*
 * ARGP Callback - Handle RegEx rules path parameter
 *
 * @param [in]: Input parameter
 * @config [in/out]: Program configuration context
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
rules_callback(void *param, void *config)
{
	struct regex_config *rgx_cfg = (struct regex_config *)config;
	char *rules_path = (char *)param;

	/* Read rules file into the rules buffer */
	return read_file(rules_path, &rgx_cfg->rules_buffer, &rgx_cfg->rules_buffer_len);
}

/*
 * ARGP Callback - Handle data to scan path parameter
 *
 * @param [in]: Input parameter
 * @config [in/out]: Program configuration context
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
data_callback(void *param, void *config)
{
	struct regex_config *rgx_cfg = (struct regex_config *)config;
	char *data_path = (char *)param;
	int len;

	len = strnlen(data_path, MAX_FILE_NAME);
	if (len == MAX_FILE_NAME) {
		DOCA_LOG_ERR("Data path is too long max %d", MAX_FILE_NAME - 1);
		return DOCA_ERROR_INVALID_VALUE;
	}
	strlcpy(rgx_cfg->data, data_path, MAX_FILE_NAME);
	return DOCA_SUCCESS;

}

/*
 * Register the command line parameters for the sample.
 *
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
register_regex_scan_params()
{
	doca_error_t result = DOCA_SUCCESS;
	struct doca_argp_param *pci_param, *rules_param, *data_param;

	/* Create and register PCI address of RegEx device param */
	result = doca_argp_param_create(&pci_param);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to create ARGP param: %s", doca_get_error_string(result));
		return result;
	}
	doca_argp_param_set_short_name(pci_param, "p");
	doca_argp_param_set_long_name(pci_param, "pci-addr");
	doca_argp_param_set_arguments(pci_param, "<PCI-ADDRESS>");
	doca_argp_param_set_description(pci_param, "RegEx device PCI address");
	doca_argp_param_set_callback(pci_param, pci_address_callback);
	doca_argp_param_set_type(pci_param, DOCA_ARGP_TYPE_STRING);
	result = doca_argp_register_param(pci_param);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to register program param: %s", doca_get_error_string(result));
		return result;
	}

	/* Create and register RegEx rules param */
	result = doca_argp_param_create(&rules_param);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to create ARGP param: %s", doca_get_error_string(result));
		return result;
	}
	doca_argp_param_set_short_name(rules_param, "r");
	doca_argp_param_set_long_name(rules_param, "rules");
	doca_argp_param_set_arguments(rules_param, "<path>");
	doca_argp_param_set_description(rules_param, "Path to compiled rules file (rof2.binary)");
	doca_argp_param_set_callback(rules_param, rules_callback);
	doca_argp_param_set_type(rules_param, DOCA_ARGP_TYPE_STRING);
	doca_argp_param_set_mandatory(rules_param);
	result = doca_argp_register_param(rules_param);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to register program param: %s", doca_get_error_string(result));
		return result;
	}

	/* Create and register data to scan param*/
	result = doca_argp_param_create(&data_param);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to create ARGP param: %s", doca_get_error_string(result));
		return result;
	}
	doca_argp_param_set_short_name(data_param, "d");
	doca_argp_param_set_long_name(data_param, "data");
	doca_argp_param_set_arguments(data_param, "<path>");
	doca_argp_param_set_description(data_param, "Path to data file");
	doca_argp_param_set_callback(data_param, data_callback);
	doca_argp_param_set_type(data_param, DOCA_ARGP_TYPE_STRING);
	result = doca_argp_register_param(data_param);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to register program param: %s", doca_get_error_string(result));
		return result;
	}

	return result;
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
	int ret;
	doca_error_t result;
	char *data_buffer = NULL;
	size_t data_buffer_len = 0;
	struct regex_config cfg = {0};
	struct doca_pci_bdf pcie_dev = {0};

	strcpy(cfg.pci_address, "03:00.0");
	strcpy(cfg.data, "/opt/mellanox/doca/samples/doca_regex/regex_scan/data_to_scan.txt");

	/* Parse cmdline/json arguments */
	result = doca_argp_init("regex_scan", &cfg);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to init ARGP resources: %s", doca_get_error_string(result));
		return EXIT_FAILURE;
	}

	/* Register RegEx scan params */
	result = register_regex_scan_params();
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to register sample parameters: %s", doca_get_error_string(result));
		doca_argp_destroy();
		return EXIT_FAILURE;
	}

	/* Start parsing sample arguments */
	result = doca_argp_start(argc, argv);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to parse sample input: %s", doca_get_error_string(result));
		if (cfg.rules_buffer != NULL)
			free(cfg.rules_buffer);
		doca_argp_destroy();
		return EXIT_FAILURE;
	}

	/* Read data file */
	result = read_file(cfg.data, &data_buffer, &data_buffer_len);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to load data buffer: %s", doca_get_error_string(result));
		free(cfg.rules_buffer);
		doca_argp_destroy();
		return EXIT_FAILURE;
	}

	result = parse_pci_addr(cfg.pci_address, &pcie_dev);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to parse PCI address: %s", doca_get_error_string(result));
		return EXIT_FAILURE;
	}

	/* Run RegEx sample */
	ret = regex_scan(data_buffer, data_buffer_len, &pcie_dev, cfg.rules_buffer, cfg.rules_buffer_len);
	if (ret < 0) {
		DOCA_LOG_ERR("Sample function has failed: %s", strerror(abs(ret)));
		free(data_buffer);
		free(cfg.rules_buffer);
		doca_argp_destroy();
		return EXIT_FAILURE;
	}

	/* Cleanup */
	if (data_buffer != NULL)
		free(data_buffer);
	if (cfg.rules_buffer != NULL)
		free(cfg.rules_buffer);

	/* ARGP cleanup */
	doca_argp_destroy();

	return EXIT_SUCCESS;
}
