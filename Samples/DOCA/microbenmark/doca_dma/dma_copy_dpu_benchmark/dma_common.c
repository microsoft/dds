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

#include <string.h>
#include <bsd/string.h>
#include <unistd.h>
#include <stdlib.h>

#include <doca_buf_inventory.h>
#include <doca_dev.h>
#include <doca_dma.h>
#include <doca_error.h>
#include <doca_log.h>
#include <doca_mmap.h>
#include <doca_argp.h>

#include "dma_common.h"

DOCA_LOG_REGISTER(DMA_COMMON);

/*
 * ARGP Callback - Handle PCI device address parameter
 *
 * @param [in]: Input parameter
 * @config [in/out]: Program configuration context
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
pci_callback(void *param, void *config)
{
	struct dma_config *conf = (struct dma_config *)config;
	const char *addr = (char *)param;
	int addr_len = strnlen(addr, MAX_ARG_SIZE);

	if (addr_len == MAX_ARG_SIZE) {
		DOCA_LOG_ERR("Entered pci address exceeded buffer size of: %d", MAX_ARG_SIZE - 1);
		return DOCA_ERROR_INVALID_VALUE;
	}

	strlcpy(conf->pci_address, addr, MAX_ARG_SIZE);

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
        struct dma_config *conf = (struct dma_config *)config;
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
        struct dma_config *conf = (struct dma_config *)config;
        const char *txt = (char *)param;
        char* end;
        conf->test_rounds = (int)strtol(txt, &end, 10);

        return DOCA_SUCCESS;
}

/*
 * ARGP Callback - Handle exported descriptor file path parameter
 *
 * @param [in]: Input parameter
 * @config [in/out]: Program configuration context
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
descriptor_path_callback(void *param, void *config)
{
	struct dma_config *conf = (struct dma_config *)config;
	const char *path = (char *)param;
	int path_len = strnlen(path, MAX_ARG_SIZE);

	if (path_len == MAX_ARG_SIZE) {
		DOCA_LOG_ERR("Entered path exceeded buffer size: %d", MAX_ARG_SIZE - 1);
		return DOCA_ERROR_INVALID_VALUE;
	}

#ifdef DOCA_ARCH_DPU
	if (access(path, F_OK | R_OK) != 0) {
		DOCA_LOG_ERR("Failed to find file path pointed by export descriptor: %s", path);
		return DOCA_ERROR_INVALID_VALUE;
	}
#endif

	strlcpy(conf->export_desc_path, path, MAX_ARG_SIZE);

	return DOCA_SUCCESS;
}

/*
 * ARGP Callback - Handle buffer information file path parameter
 *
 * @param [in]: Input parameter
 * @config [in/out]: Program configuration context
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
buf_info_path_callback(void *param, void *config)
{
	struct dma_config *conf = (struct dma_config *)config;
	const char *path = (char *)param;
	int path_len = strnlen(path, MAX_ARG_SIZE);

	if (path_len == MAX_ARG_SIZE) {
		DOCA_LOG_ERR("Entered path exceeded buffer size: %d", MAX_ARG_SIZE - 1);
		return DOCA_ERROR_INVALID_VALUE;
	}

#ifdef DOCA_ARCH_DPU
	if (access(path, F_OK | R_OK) != 0) {
		DOCA_LOG_ERR("Failed to find file path pointed by buffer information: %s", path);
		return DOCA_ERROR_INVALID_VALUE;
	}
#endif

	strlcpy(conf->buf_info_path, path, MAX_ARG_SIZE);

	return DOCA_SUCCESS;
}

doca_error_t
register_dma_params()
{
	doca_error_t result;
	struct doca_argp_param *pci_address_param, *msg_size_param, *test_rounds_param, *export_desc_path_param, *buf_info_path_param;

	/* Create and register PCI address param */
	result = doca_argp_param_create(&pci_address_param);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to create ARGP param: %s", doca_get_error_string(result));
		return result;
	}
	doca_argp_param_set_short_name(pci_address_param, "p");
	doca_argp_param_set_long_name(pci_address_param, "pci");
	doca_argp_param_set_description(pci_address_param, "DOCA DMA device PCI address");
	doca_argp_param_set_callback(pci_address_param, pci_callback);
	doca_argp_param_set_type(pci_address_param, DOCA_ARGP_TYPE_STRING);
	result = doca_argp_register_param(pci_address_param);
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

	/* Create and register exported descriptor file path param */
	result = doca_argp_param_create(&export_desc_path_param);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to create ARGP param: %s", doca_get_error_string(result));
		return result;
	}
	doca_argp_param_set_short_name(export_desc_path_param, "d");
	doca_argp_param_set_long_name(export_desc_path_param, "descriptor-path");
	doca_argp_param_set_description(export_desc_path_param,
					"Exported descriptor file path to save (Host) or to read from (DPU)");
	doca_argp_param_set_callback(export_desc_path_param, descriptor_path_callback);
	doca_argp_param_set_type(export_desc_path_param, DOCA_ARGP_TYPE_STRING);
	result = doca_argp_register_param(export_desc_path_param);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to register program param: %s", doca_get_error_string(result));
		return result;
	}

	/* Create and register buffer information file param */
	result = doca_argp_param_create(&buf_info_path_param);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to create ARGP param: %s", doca_get_error_string(result));
		return result;
	}
	doca_argp_param_set_short_name(buf_info_path_param, "b");
	doca_argp_param_set_long_name(buf_info_path_param, "buffer-path");
	doca_argp_param_set_description(buf_info_path_param,
					"Buffer information file path to save (Host) or to read from (DPU)");
	doca_argp_param_set_callback(buf_info_path_param, buf_info_path_callback);
	doca_argp_param_set_type(buf_info_path_param, DOCA_ARGP_TYPE_STRING);
	result = doca_argp_register_param(buf_info_path_param);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to register program param: %s", doca_get_error_string(result));
		return result;
	}

	return DOCA_SUCCESS;
}

doca_error_t
host_init_core_objects(struct program_core_objects *state)
{
	doca_error_t res;

	res = doca_mmap_create(NULL, &state->mmap);
	if (res != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Unable to create mmap: %s", doca_get_error_string(res));
		return res;
	}

	res = doca_mmap_start(state->mmap);
	if (res != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Unable to start memory map: %s", doca_get_error_string(res));
		return res;
	}

	res = doca_mmap_dev_add(state->mmap, state->dev);
	if (res != DOCA_SUCCESS)
		DOCA_LOG_ERR("Unable to add device to mmap: %s", doca_get_error_string(res));

	return res;
}

void
dma_cleanup(struct program_core_objects *state, struct doca_dma *dma_ctx)
{
	doca_error_t res;

	destroy_core_objects(state);

	res = doca_dma_destroy(dma_ctx);
	if (res != DOCA_SUCCESS)
		DOCA_LOG_ERR("Failed to destroy dma: %s", doca_get_error_string(res));

	state->ctx = NULL;
}

void
host_destroy_core_objects(struct program_core_objects *state)
{
	doca_error_t res;

	res = doca_mmap_destroy(state->mmap);
	if (res != DOCA_SUCCESS)
		DOCA_LOG_ERR("Failed to destroy mmap: %s", doca_get_error_string(res));
	state->mmap = NULL;

	res = doca_dev_close(state->dev);
	if (res != DOCA_SUCCESS)
		DOCA_LOG_ERR("Failed to close device: %s", doca_get_error_string(res));
	state->dev = NULL;
}

doca_error_t
dma_jobs_is_supported(struct doca_devinfo *devinfo)
{
	return doca_dma_job_get_supported(devinfo, DOCA_DMA_JOB_MEMCPY);
}
