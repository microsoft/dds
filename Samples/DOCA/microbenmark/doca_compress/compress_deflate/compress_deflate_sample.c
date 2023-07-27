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
#include <stdlib.h>
#include <time.h>

#include <doca_buf.h>
#include <doca_buf_inventory.h>
#include <doca_ctx.h>
#include <doca_compress.h>
#include <doca_error.h>
#include <doca_log.h>

#include "common.h"

DOCA_LOG_REGISTER(COMPRESS_DEFLATE);

#define SLEEP_IN_NANOS (10 * 1000)		/* Sample the job every 10 microseconds  */
#define MAX_FILE_SIZE (128 * 1024 * 1024)	/* compress files up to 128 MB */

/*
 * Free callback - free doca_buf allocated pointer
 *
 * @addr [in]: Memory range pointer
 * @len [in]: Memory range length
 * @opaque [in]: An opaque pointer passed to iterator
 */
void
free_cb(void *addr, size_t len, void *opaque)
{
	free(addr);
}

/*
 * Clean all the sample resources
 *
 * @state [in]: program_core_objects struct
 * @compress [in]: compress context
 */
static void
compress_cleanup(struct program_core_objects *state, struct doca_compress *compress)
{
	doca_error_t result;

	destroy_core_objects(state);

	result = doca_compress_destroy(compress);
	if (result != DOCA_SUCCESS)
		DOCA_LOG_ERR("Failed to destroy compress: %s", doca_get_error_string(result));
}

/**
 * Check if given device is capable of executing a DOCA_COMPRESS_DEFLATE_JOB.
 *
 * @devinfo [in]: The DOCA device information
 * @return: DOCA_SUCCESS if the device supports DOCA_COMPRESS_DEFLATE_JOB and DOCA_ERROR otherwise.
 */
static doca_error_t
compress_jobs_compress_is_supported(struct doca_devinfo *devinfo)
{
	return doca_compress_job_get_supported(devinfo, DOCA_COMPRESS_DEFLATE_JOB);
}

/*
 * Run compress_deflate sample
 *
 * @pci_dev [in]: pci address struct for doca device
 * @file_data [in]: file data for the compress job
 * @file_size [in]: file size
 * @bytes_to_compress [in]: number of bytes to compress
 * @return: DOCA_SUCCESS on success, DOCA_ERROR otherwise.
 */
doca_error_t
compress_deflate(struct doca_pci_bdf *pci_dev, char *file_data, size_t file_size, size_t bytes_to_compress)
{
	struct program_core_objects state = {0};
	struct doca_event event = {0};
	struct doca_compress *compress;
	struct doca_buf *src_doca_buf;
	struct doca_buf *dst_doca_buf;
	uint32_t workq_depth = 1;	/* The sample will run 1 compress job */
	uint32_t max_chunks = 2;	/* The sample will use 2 doca buffers */
	size_t pg_sz = 1024 * 4;	/* OS Page Size (4096 bytes) */
	char *dst_buffer;
	struct timespec start_time;
	struct timespec end_time;
	size_t time_ns;
	uint8_t *resp_head;
	size_t data_len;
	doca_error_t result;

	result = doca_compress_create(&compress);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Unable to create compress engine: %s", doca_get_error_string(result));
		return result;
	}

	state.ctx = doca_compress_as_ctx(compress);

	result = open_doca_device_with_pci(pci_dev, &compress_jobs_compress_is_supported, &state.dev);

	if (result != DOCA_SUCCESS) {
		result = doca_compress_destroy(compress);
		return result;
	}

	result = init_core_objects(&state, DOCA_BUF_EXTENSION_NONE, workq_depth, max_chunks);
	if (result != DOCA_SUCCESS) {
		compress_cleanup(&state, compress);
		return result;
	}

	dst_buffer = calloc(1, MAX_FILE_SIZE);
	if (dst_buffer == NULL) {
		DOCA_LOG_ERR("Failed to allocate memory");
		compress_cleanup(&state, compress);
		return DOCA_ERROR_NO_MEMORY;
	}

	if (doca_mmap_populate(state.mmap, dst_buffer, MAX_FILE_SIZE, pg_sz, &free_cb, NULL) != DOCA_SUCCESS ||
	    doca_mmap_populate(state.mmap, file_data, bytes_to_compress, pg_sz, NULL, NULL) != DOCA_SUCCESS) {
		free(dst_buffer);
		compress_cleanup(&state, compress);
		return result;
	}

	/* Construct DOCA buffer for each address range */
	result = doca_buf_inventory_buf_by_addr(state.buf_inv, state.mmap, file_data, bytes_to_compress, &src_doca_buf);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Unable to acquire DOCA buffer representing source buffer: %s", doca_get_error_string(result));
		compress_cleanup(&state, compress);
		return result;
	}

	/* Construct DOCA buffer for each address range */
	result = doca_buf_inventory_buf_by_addr(state.buf_inv, state.mmap, dst_buffer, MAX_FILE_SIZE, &dst_doca_buf);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Unable to acquire DOCA buffer representing destination buffer: %s", doca_get_error_string(result));
		doca_buf_refcount_rm(src_doca_buf, NULL);
		compress_cleanup(&state, compress);
		return result;
	}

	/* setting data length in doca buffer */
	result = doca_buf_set_data(src_doca_buf, file_data, bytes_to_compress);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Unable to set DOCA buffer data: %s", doca_get_error_string(result));
		doca_buf_refcount_rm(src_doca_buf, NULL);
		doca_buf_refcount_rm(dst_doca_buf, NULL);
		compress_cleanup(&state, compress);
		return result;
	}

	/* Construct compress job */
	const struct doca_compress_job compress_job = {
		.base = (struct doca_job) {
			.type = DOCA_COMPRESS_DEFLATE_JOB,
			.flags = DOCA_JOB_FLAGS_NONE,
			.ctx = state.ctx,
			.user_data.u64 = DOCA_COMPRESS_DEFLATE_JOB,
			},
		.dst_buff = dst_doca_buf,
		.src_buff = src_doca_buf,
	};

	/* Enqueue compress job */
	result = doca_workq_submit(state.workq, &compress_job.base);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to submit compress job: %s", doca_get_error_string(result));
		doca_buf_refcount_rm(dst_doca_buf, NULL);
		doca_buf_refcount_rm(src_doca_buf, NULL);
		compress_cleanup(&state, compress);
		return result;
	}
	
	//
	// Take the timestamp
	//
	clock_gettime(CLOCK_REALTIME, &start_time);

	/* Wait for job completion */
	while ((result = doca_workq_progress_retrieve(state.workq, &event, DOCA_WORKQ_RETRIEVE_FLAGS_NONE)) ==
	       DOCA_ERROR_AGAIN) {}
	
	//
	// Take the timestamp again
	//
	clock_gettime(CLOCK_REALTIME, &end_time);
	time_ns = (end_time.tv_sec * 1000000000 + end_time.tv_nsec) - (start_time.tv_sec * 1000000000 + start_time.tv_nsec);

	if (result != DOCA_SUCCESS)
		DOCA_LOG_ERR("Failed to retrieve compress job: %s", doca_get_error_string(result));

	else if (event.result.u64 != DOCA_SUCCESS)
		DOCA_LOG_ERR("compress job finished unsuccessfully");

	else if (((int)(event.type) != (int)DOCA_COMPRESS_DEFLATE_JOB) ||
		(event.user_data.u64 != DOCA_COMPRESS_DEFLATE_JOB))
		DOCA_LOG_ERR("Received wrong event");
	else {
		doca_buf_get_head(compress_job.dst_buff, (void **)&resp_head);
		doca_buf_get_data_len(compress_job.dst_buff, &data_len);
	}

	printf("Compressing %ld bytes into %ld bytes takes %.2f us\n", bytes_to_compress, data_len, (time_ns / 1000.0));


	if (doca_buf_refcount_rm(src_doca_buf, NULL) != DOCA_SUCCESS ||
	    doca_buf_refcount_rm(dst_doca_buf, NULL) != DOCA_SUCCESS)
		DOCA_LOG_ERR("Failed to decrease DOCA buffer reference count");

	/* Clean and destroy all relevant objects */
	compress_cleanup(&state, compress);

	return result;
}
