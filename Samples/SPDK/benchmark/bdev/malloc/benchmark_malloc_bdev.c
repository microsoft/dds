/*   SPDX-License-Identifier: BSD-3-Clause
 *   Copyright (C) 2018 Intel Corporation.
 *   All rights reserved.
 */

#include "spdk/stdinc.h"
#include "spdk/thread.h"
#include "spdk/bdev.h"
#include "spdk/env.h"
#include "spdk/event.h"
#include "spdk/log.h"
#include "spdk/string.h"
#include "spdk/bdev_zone.h"

#include "../include/benchmark.h"

static char *g_bdev_name = "Malloc0";

/*
 * Usage function for printing parameters that are specific to this application
 */
	static void
benchmark_bdev_usage(void)
{
	printf(" -b <bdev>                 name of the bdev to use\n");
}

/*
 * This function is called to parse the parameters that are specific to this application
 */
	static int
hello_bdev_parse_arg(int ch, char *arg)
{
	switch (ch) {
		case 'b':
			g_bdev_name = arg;
			break;
		default:
			return -EINVAL;
	}
	return 0;
}

static void
benchmark_read_one_page(void *arg);

/*
 * Callback function for read io completion.
 */
	static void
read_complete(struct spdk_bdev_io *bdev_io, bool success, void *cb_arg)
{
	struct IOContext_t *IOContext = cb_arg;
	struct benchmark_context_t *benchmark_context = IOContext->BenchCtx;

	/* Complete the I/O */
	spdk_bdev_free_io(bdev_io);

	if (!success) {
		SPDK_ERRLOG("bdev io write error: %d\n", EIO);
		spdk_put_io_channel(benchmark_context->bdev_io_channel);
		spdk_bdev_close(benchmark_context->bdev_desc);
		spdk_app_stop(-1);
		return;
	}

	benchmark_context->reads_completed++;
	if (benchmark_context->reads_completed == TOTAL_READS) {
		size_t us_start, us_end;
		SPDK_NOTICELOG("All %d reads have completed\n", TOTAL_READS);
		gettimeofday(&benchmark_context->tv_end,NULL);
		us_start = benchmark_context->tv_start.tv_sec*(uint64_t)1000000+benchmark_context->tv_start.tv_usec;
		us_end = benchmark_context->tv_end.tv_sec*(uint64_t)1000000+benchmark_context->tv_end.tv_usec;
		SPDK_NOTICELOG("Random read throughput = %.2f IOPS\n", (TOTAL_READS * 1000000.0) / (us_end - us_start));

		spdk_put_io_channel(benchmark_context->bdev_io_channel);
		spdk_bdev_close(benchmark_context->bdev_desc);
		spdk_app_stop(success ? 0 : -1);
	} else {
		if (benchmark_context->reads_issued < TOTAL_READS) {
			benchmark_read_one_page(IOContext);
		}
	}
}

	static void
benchmark_read_one_page(void *arg)
{
	struct IOContext_t *IOContext = arg;
	struct benchmark_context_t *benchmark_context = IOContext->BenchCtx;
	int rc = 0;

	rc = spdk_bdev_read(benchmark_context->bdev_desc, benchmark_context->bdev_io_channel,
			IOContext->Buff, ((benchmark_context->reads_issued * 42) % benchmark_context->total_writes) * DB_PAGE_SIZE,
			DB_PAGE_SIZE, read_complete, IOContext);

	if (rc == -ENOMEM) {
		SPDK_NOTICELOG("Queueing io\n");
		/* In case we cannot perform I/O now, queue I/O */
		benchmark_context->bdev_io_wait.bdev = benchmark_context->bdev;
		benchmark_context->bdev_io_wait.cb_fn = benchmark_read_one_page;
		benchmark_context->bdev_io_wait.cb_arg = IOContext;
		spdk_bdev_queue_io_wait(benchmark_context->bdev, benchmark_context->bdev_io_channel,
				&benchmark_context->bdev_io_wait);
	} else if (rc) {
		SPDK_ERRLOG("%s error while reading from bdev: %d\n", spdk_strerror(-rc), rc);
		spdk_put_io_channel(benchmark_context->bdev_io_channel);
		spdk_bdev_close(benchmark_context->bdev_desc);
		spdk_app_stop(-1);
	}

	benchmark_context->reads_issued++;
}

	static void
benchmark_read(void *arg)
{
	struct benchmark_context_t *benchmark_context = arg;
	int rc = 0;

	for (int i = 0; i != READ_QUEUE_DEPTH; i++) {
		struct IOContext_t *IOContext = benchmark_context->read_ctxs[i];
		rc = spdk_bdev_read(benchmark_context->bdev_desc, benchmark_context->bdev_io_channel,
				IOContext->Buff, ((benchmark_context->reads_issued * 42) % benchmark_context->total_writes) * DB_PAGE_SIZE,
				DB_PAGE_SIZE, read_complete, IOContext);

		if (rc == -ENOMEM) {
			SPDK_NOTICELOG("Queueing io\n");
			//
			// TODO: better handle queueing
			//
			/* In case we cannot perform I/O now, queue I/O */
			benchmark_context->bdev_io_wait.bdev = benchmark_context->bdev;
			benchmark_context->bdev_io_wait.cb_fn = benchmark_read_one_page;
			benchmark_context->bdev_io_wait.cb_arg = IOContext;
			spdk_bdev_queue_io_wait(benchmark_context->bdev, benchmark_context->bdev_io_channel,
					&benchmark_context->bdev_io_wait);
		} else if (rc) {
			SPDK_ERRLOG("%s error while reading from bdev: %d\n", spdk_strerror(-rc), rc);
			spdk_put_io_channel(benchmark_context->bdev_io_channel);
			spdk_bdev_close(benchmark_context->bdev_desc);
			spdk_app_stop(-1);
		}

		benchmark_context->reads_issued++;
	}
}

static void
benchmark_write_one_page(void *arg);

/*
 * Callback function for write io completion.
 */
	static void
write_complete(struct spdk_bdev_io *bdev_io, bool success, void *cb_arg)
{
	struct IOContext_t *IOContext = cb_arg;
	struct benchmark_context_t *benchmark_context = IOContext->BenchCtx;

	/* Complete the I/O */
	spdk_bdev_free_io(bdev_io);

	if (!success) {
		SPDK_ERRLOG("bdev io write error: %d\n", EIO);
		spdk_put_io_channel(benchmark_context->bdev_io_channel);
		spdk_bdev_close(benchmark_context->bdev_desc);
		spdk_app_stop(-1);
		return;
	}

	benchmark_context->writes_completed++;
	if (benchmark_context->writes_completed == benchmark_context->total_writes) {
		SPDK_NOTICELOG("All %ld writes have completed\n", benchmark_context->total_writes);
		SPDK_NOTICELOG("Start reading from the bdev\n");
		gettimeofday(&benchmark_context->tv_start,NULL);
		benchmark_read(benchmark_context);
	} else {
		if (benchmark_context->writes_issued < benchmark_context->total_writes) {
			benchmark_write_one_page(IOContext);
		}
	}
}

	static void
benchmark_write_one_page(void *arg)
{
	struct IOContext_t *IOContext = arg;
	struct benchmark_context_t *benchmark_context = IOContext->BenchCtx;
	int rc = 0;

	rc = spdk_bdev_write(benchmark_context->bdev_desc, benchmark_context->bdev_io_channel,
			IOContext->Buff, benchmark_context->writes_issued * DB_PAGE_SIZE, DB_PAGE_SIZE, write_complete,
			IOContext);

	if (rc == -ENOMEM) {
		SPDK_NOTICELOG("Queueing io\n");
		/* In case we cannot perform I/O now, queue I/O */
		benchmark_context->bdev_io_wait.bdev = benchmark_context->bdev;
		benchmark_context->bdev_io_wait.cb_fn = benchmark_write_one_page;
		benchmark_context->bdev_io_wait.cb_arg = IOContext;
		spdk_bdev_queue_io_wait(benchmark_context->bdev, benchmark_context->bdev_io_channel,
				&benchmark_context->bdev_io_wait);
	} else if (rc) {
		SPDK_ERRLOG("%s error while writing to bdev: %d\n", spdk_strerror(-rc), rc);
		spdk_put_io_channel(benchmark_context->bdev_io_channel);
		spdk_bdev_close(benchmark_context->bdev_desc);
		spdk_app_stop(-1);
	}

	benchmark_context->writes_issued++;
}

	static void
benchmark_write(void *arg)
{
	struct benchmark_context_t *benchmark_context = arg;
	int rc = 0;

	for (int i = 0; i != WRITE_QUEUE_DEPTH; i++) {
		struct IOContext_t *IOContext = benchmark_context->write_ctxs[i];
		rc = spdk_bdev_write(benchmark_context->bdev_desc, benchmark_context->bdev_io_channel,
				IOContext->Buff, benchmark_context->writes_issued * DB_PAGE_SIZE, DB_PAGE_SIZE, write_complete,
				IOContext);

		if (rc == -ENOMEM) {
			SPDK_NOTICELOG("Queueing io\n");
			//
			// TODO: better handle queueing
			//
			/* In case we cannot perform I/O now, queue I/O */
			benchmark_context->bdev_io_wait.bdev = benchmark_context->bdev;
			benchmark_context->bdev_io_wait.cb_fn = benchmark_write_one_page;
			benchmark_context->bdev_io_wait.cb_arg = IOContext;
			spdk_bdev_queue_io_wait(benchmark_context->bdev, benchmark_context->bdev_io_channel,
					&benchmark_context->bdev_io_wait);
		} else if (rc) {
			SPDK_ERRLOG("%s error while writing to bdev: %d\n", spdk_strerror(-rc), rc);
			spdk_put_io_channel(benchmark_context->bdev_io_channel);
			spdk_bdev_close(benchmark_context->bdev_desc);
			spdk_app_stop(-1);
		}

		benchmark_context->writes_issued++;
	}
}

	static void
benchmark_bdev_event_cb(enum spdk_bdev_event_type type, struct spdk_bdev *bdev,
		void *event_ctx)
{
	SPDK_NOTICELOG("Unsupported bdev event: type %d\n", type);
}

	static void
reset_zone_complete(struct spdk_bdev_io *bdev_io, bool success, void *cb_arg)
{
	struct benchmark_context_t *benchmark_context = cb_arg;

	/* Complete the I/O */
	spdk_bdev_free_io(bdev_io);

	if (!success) {
		SPDK_ERRLOG("bdev io reset zone error: %d\n", EIO);
		spdk_put_io_channel(benchmark_context->bdev_io_channel);
		spdk_bdev_close(benchmark_context->bdev_desc);
		spdk_app_stop(-1);
		return;
	}

	SPDK_NOTICELOG("Start writing to the bdev\n");
	benchmark_write(benchmark_context);
}

	static void
benchmark_reset_zone(void *arg)
{
	struct benchmark_context_t *benchmark_context = arg;
	int rc = 0;

	rc = spdk_bdev_zone_management(benchmark_context->bdev_desc, benchmark_context->bdev_io_channel,
			0, SPDK_BDEV_ZONE_RESET, reset_zone_complete, benchmark_context);

	if (rc == -ENOMEM) {
		SPDK_NOTICELOG("Queueing io\n");
		/* In case we cannot perform I/O now, queue I/O */
		benchmark_context->bdev_io_wait.bdev = benchmark_context->bdev;
		benchmark_context->bdev_io_wait.cb_fn = benchmark_reset_zone;
		benchmark_context->bdev_io_wait.cb_arg = benchmark_context;
		spdk_bdev_queue_io_wait(benchmark_context->bdev, benchmark_context->bdev_io_channel,
				&benchmark_context->bdev_io_wait);
	} else if (rc) {
		SPDK_ERRLOG("%s error while resetting zone: %d\n", spdk_strerror(-rc), rc);
		spdk_put_io_channel(benchmark_context->bdev_io_channel);
		spdk_bdev_close(benchmark_context->bdev_desc);
		spdk_app_stop(-1);
	}
}

/*
 * Our initial event that kicks off everything from main().
 */
	static void
benchmark_start(void *arg1)
{
	struct benchmark_context_t *benchmark_context = arg1;
	uint32_t buf_align;
	int rc = 0;
	benchmark_context->bdev = NULL;
	benchmark_context->bdev_desc = NULL;
	benchmark_context->total_writes = 0;
	benchmark_context->writes_issued = 0;
	benchmark_context->writes_completed = 0;
	benchmark_context->reads_issued = 0;
	benchmark_context->reads_completed = 0;

	SPDK_NOTICELOG("Successfully started the application\n");

	/*
	 * There can be many bdevs configured, but this application will only use
	 * the one input by the user at runtime.
	 *
	 * Open the bdev by calling spdk_bdev_open_ext() with its name.
	 * The function will return a descriptor
	 */
	SPDK_NOTICELOG("Opening the bdev %s\n", benchmark_context->bdev_name);
	rc = spdk_bdev_open_ext(benchmark_context->bdev_name, true, benchmark_bdev_event_cb, NULL,
			&benchmark_context->bdev_desc);
	if (rc) {
		SPDK_ERRLOG("Could not open bdev: %s\n", benchmark_context->bdev_name);
		spdk_app_stop(-1);
		return;
	}

	/* A bdev pointer is valid while the bdev is opened. */
	benchmark_context->bdev = spdk_bdev_desc_get_bdev(benchmark_context->bdev_desc);

	SPDK_NOTICELOG("Opening io channel\n");
	/* Open I/O channel */
	benchmark_context->bdev_io_channel = spdk_bdev_get_io_channel(benchmark_context->bdev_desc);
	if (benchmark_context->bdev_io_channel == NULL) {
		SPDK_ERRLOG("Could not create bdev I/O channel!!\n");
		spdk_bdev_close(benchmark_context->bdev_desc);
		spdk_app_stop(-1);
		return;
	}

	//
	// Initialize contexts
	//
	size_t devCapacity = spdk_bdev_get_block_size(benchmark_context->bdev)
		* spdk_bdev_get_num_blocks(benchmark_context->bdev);
	SPDK_NOTICELOG("Device capacity: %ld\n", devCapacity);
	InitializeContexts(benchmark_context, devCapacity);
	buf_align = spdk_bdev_get_buf_align(benchmark_context->bdev);

	//
	// Allocate write buffers
	//
	for(int i = 0; i != WRITE_QUEUE_DEPTH; i++) {
		benchmark_context->write_ctxs[i]->Buff = spdk_dma_zmalloc(DB_PAGE_SIZE, buf_align, NULL);
		if (!benchmark_context->write_ctxs[i]->Buff) {
			SPDK_ERRLOG("Failed to allocate a write buffer\n");
			spdk_put_io_channel(benchmark_context->bdev_io_channel);
			spdk_bdev_close(benchmark_context->bdev_desc);
			spdk_app_stop(-1);
			return;
		}
	}

	//
	// Allocate read buffers
	//
	for(int i = 0; i != WRITE_QUEUE_DEPTH; i++) {
		benchmark_context->read_ctxs[i]->Buff = spdk_dma_zmalloc(DB_PAGE_SIZE, buf_align, NULL);
		if (!benchmark_context->read_ctxs[i]->Buff) {
			SPDK_ERRLOG("Failed to allocate a read buffer\n");
			spdk_put_io_channel(benchmark_context->bdev_io_channel);
			spdk_bdev_close(benchmark_context->bdev_desc);
			spdk_app_stop(-1);
			return;
		}
	}

	// Prepare write data
	PrepareData(benchmark_context);

	if (spdk_bdev_is_zoned(benchmark_context->bdev)) {
		benchmark_reset_zone(benchmark_context);
		/* If bdev is zoned, the callback, reset_zone_complete, will call benchmark_write() */
		return;
	}

	SPDK_NOTICELOG("Start writing to the bdev\n");
	benchmark_write(benchmark_context);
}

	int
main(int argc, char **argv)
{
	struct spdk_app_opts opts = {};
	int rc = 0;
	struct benchmark_context_t benchmark_context = {};

	/* Set default values in opts structure. */
	spdk_app_opts_init(&opts, sizeof(opts));
	opts.name = "benchmark_malloc_bdev";

	/*
	 * Parse built-in SPDK command line parameters as well
	 * as our custom one(s).
	 */
	if ((rc = spdk_app_parse_args(argc, argv, &opts, "b:", NULL, hello_bdev_parse_arg,
					benchmark_bdev_usage)) != SPDK_APP_PARSE_ARGS_SUCCESS) {
		exit(rc);
	}
	benchmark_context.bdev_name = g_bdev_name;

	/*
	 * spdk_app_start() will initialize the SPDK framework, call benchmark_start(),
	 * and then block until spdk_app_stop() is called (or if an initialization
	 * error occurs, spdk_app_start() will return with rc even without calling
	 * benchmark_start().
	 */
	rc = spdk_app_start(&opts, benchmark_start, &benchmark_context);
	if (rc) {
		SPDK_ERRLOG("ERROR starting application\n");
	}

	/* At this point either spdk_app_stop() was called, or spdk_app_start()
	 * failed because of internal error.
	 */

	/* When the app stops, free up memory that we allocated. */
	for(int i = 0; i != WRITE_QUEUE_DEPTH; i++) {
		spdk_dma_free(benchmark_context.write_ctxs[i]->Buff);
	}
	for(int i = 0; i != READ_QUEUE_DEPTH; i++) {
		spdk_dma_free(benchmark_context.read_ctxs[i]->Buff);
	}

	//
	// Release contexts
	//
	ReleaseContexts(&benchmark_context);

	/* Gracefully close out all of the SPDK subsystems. */
	spdk_app_fini();
	return rc;
}
