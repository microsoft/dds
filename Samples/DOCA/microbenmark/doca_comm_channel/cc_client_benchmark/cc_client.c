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
#include <doca_dev.h>

#include "cc_client.h"

DOCA_LOG_REGISTER(CC_CLIENT_BENCHMARK);

/*
 * ARGP Callback - Handle Comm Channel DOCA device PCI address parameter
 *
 * @param [in]: Input parameter
 * @config [in/out]: Program configuration context
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
pci_addr_callback(void *param, void *config)
{
	struct cc_config *cfg = (struct cc_config *)config;
	const char *dev_pci_addr = (char *)param;

	if (strnlen(dev_pci_addr, PCI_ADDR_LEN) == PCI_ADDR_LEN) {
		DOCA_LOG_ERR("Comm Channel DOCA device PCI address exceeding the maximum size of %d", PCI_ADDR_LEN - 1);
		return DOCA_ERROR_INVALID_VALUE;
	}

	strlcpy(cfg->cc_dev_pci_addr, dev_pci_addr, PCI_ADDR_LEN);

	return DOCA_SUCCESS;
}

/*
 * ARGP Callback - Handle Comm Channel DOCA device representor PCI address parameter
 *
 * @param [in]: Input parameter
 * @config [in/out]: Program configuration context
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
rep_pci_addr_callback(void *param, void *config)
{
	struct cc_config *cfg = (struct cc_config *)config;
	const char *rep_pci_addr = (char *)param;

	if (strnlen(rep_pci_addr, PCI_ADDR_LEN) == PCI_ADDR_LEN) {
		DOCA_LOG_ERR("Comm Channel DOCA device representor PCI address exceeding the maximum size of %d",
			     PCI_ADDR_LEN - 1);
		return DOCA_ERROR_INVALID_VALUE;
	}

	strlcpy(cfg->cc_dev_rep_pci_addr, rep_pci_addr, PCI_ADDR_LEN);

	return DOCA_SUCCESS;
}

/*
 * ARGP Callback - Handle msg size parameter
 *
 * @param [in]: Input parameter
 * @config [in/out]: Program configuration context
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
msg_size_callback(void *param, void *config)
{
	struct cc_config *conf = (struct cc_config *)config;
	const char *txt = (char *)param;
	char* end;
	conf->msg_size = (int)strtol(txt, &end, 10);

	return DOCA_SUCCESS;
}

/*
 * ARGP Callback - Handle test rounds parameter
 *
 * @param [in]: Input parameter
 * @config [in/out]: Program configuration context
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
test_rounds_callback(void *param, void *config)
{
	struct cc_config *conf = (struct cc_config *)config;
	const char *txt = (char *)param;
	char* end;
	conf->test_rounds = (int)strtol(txt, &end, 10);

	return DOCA_SUCCESS;
}

doca_error_t
register_cc_params()
{
	doca_error_t result;

	struct doca_argp_param *dev_pci_addr_param, *msg_size_param, *test_rounds_param, *rep_pci_addr_param;

	/* Create and register Comm Channel DOCA device PCI address */
	result = doca_argp_param_create(&dev_pci_addr_param);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to create ARGP param: %s", doca_get_error_string(result));
		return result;
	}
	doca_argp_param_set_short_name(dev_pci_addr_param, "d");
	doca_argp_param_set_long_name(dev_pci_addr_param, "dev-pci");
	doca_argp_param_set_description(dev_pci_addr_param,
					"Comm Channel DOCA device PCI address");
	doca_argp_param_set_callback(dev_pci_addr_param, pci_addr_callback);
	doca_argp_param_set_type(dev_pci_addr_param, DOCA_ARGP_TYPE_STRING);
	result = doca_argp_register_param(dev_pci_addr_param);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to register program param: %s", doca_get_error_string(result));
		return result;
	}

	/* Create and register Comm Channel DOCA device representor PCI address */
	result = doca_argp_param_create(&rep_pci_addr_param);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to create ARGP param: %s", doca_get_error_string(result));
		return result;
	}
	doca_argp_param_set_short_name(rep_pci_addr_param, "r");
	doca_argp_param_set_long_name(rep_pci_addr_param, "rep-pci");
	doca_argp_param_set_description(rep_pci_addr_param,
					"Comm Channel DOCA device representor PCI address (needed only on DPU)");
	doca_argp_param_set_callback(rep_pci_addr_param, rep_pci_addr_callback);
	doca_argp_param_set_type(rep_pci_addr_param, DOCA_ARGP_TYPE_STRING);
	result = doca_argp_register_param(rep_pci_addr_param);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to register program param: %s", doca_get_error_string(result));
		return result;
	}

	/* Create and register msg size param */
	result = doca_argp_param_create(&msg_size_param);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to create ARGP param: %s", doca_get_error_string(result));
		return result;
	}
	doca_argp_param_set_short_name(msg_size_param, "s");
	doca_argp_param_set_long_name(msg_size_param, "msg_size");
	doca_argp_param_set_description(msg_size_param, "Benchmark message size");
	doca_argp_param_set_callback(msg_size_param, msg_size_callback);
	doca_argp_param_set_type(msg_size_param, DOCA_ARGP_TYPE_STRING);
	result = doca_argp_register_param(msg_size_param);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to register program param: %s", doca_get_error_string(result));
		return result;
	}
	
	/* Create and register test rounds param */
	result = doca_argp_param_create(&test_rounds_param);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to create ARGP param: %s", doca_get_error_string(result));
		return result;
	}
	doca_argp_param_set_short_name(test_rounds_param, "n");
	doca_argp_param_set_long_name(test_rounds_param, "test_rounds");
	doca_argp_param_set_description(test_rounds_param, "Total rounds of test");
	doca_argp_param_set_callback(test_rounds_param, test_rounds_callback);
	doca_argp_param_set_type(test_rounds_param, DOCA_ARGP_TYPE_STRING);
	result = doca_argp_register_param(test_rounds_param);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to register program param: %s", doca_get_error_string(result));
		return result;
	}

	return DOCA_SUCCESS;
}
